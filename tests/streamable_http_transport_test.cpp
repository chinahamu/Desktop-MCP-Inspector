#include <catch2/catch_test_macros.hpp>

#include "ServerProfile.hpp"
#include "StreamableHttpTransport.hpp"
#include "Transport.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QHash>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QUrl>

#include <algorithm>
#include <functional>
#include <optional>
#include <vector>

namespace {

QCoreApplication& ensure_qcore_application()
{
  if (auto* instance = QCoreApplication::instance()) {
    return *instance;
  }

  static int argc = 1;
  static char app_name[] = "desktop_mcp_inspector_tests";
  static char* argv[] = {app_name, nullptr};
  static QCoreApplication application(argc, argv);
  return application;
}

bool wait_until(const std::function<bool()>& predicate, int timeout_ms = 3000)
{
  auto& app = ensure_qcore_application();
  QElapsedTimer timer;
  timer.start();

  while (timer.elapsed() < timeout_ms) {
    if (predicate()) {
      return true;
    }

    app.processEvents(QEventLoop::AllEvents, 20);
    QThread::msleep(5);
  }

  app.processEvents(QEventLoop::AllEvents, 20);
  return predicate();
}

struct RecordedHttpRequest
{
  QString method;
  QString path;
  QMap<QString, QString> headers;
  QByteArray body;
};

class MinimalHttpServer final : public QObject
{
public:
  explicit MinimalHttpServer(QObject* parent = nullptr)
    : QObject(parent)
  {
    connect(&server_, &QTcpServer::newConnection, this, [this] { handle_new_connection(); });
  }

  ~MinimalHttpServer() override
  {
    server_.close();
  }

  [[nodiscard]] bool listen()
  {
    return server_.listen(QHostAddress::LocalHost, 0);
  }

  [[nodiscard]] QString endpoint_url() const
  {
    return QStringLiteral("http://127.0.0.1:%1/mcp").arg(server_.serverPort());
  }

  [[nodiscard]] std::optional<RecordedHttpRequest> first_request(const QString& method) const
  {
    const auto found = std::find_if(requests.cbegin(), requests.cend(), [&method](const RecordedHttpRequest& request) {
      return request.method == method;
    });
    if (found == requests.cend()) {
      return std::nullopt;
    }

    return *found;
  }

  std::vector<RecordedHttpRequest> requests;
  bool http_error_response = false;
  bool json_rpc_error_response = false;

private:
  void handle_new_connection()
  {
    while (auto* socket = server_.nextPendingConnection()) {
      connect(socket, &QTcpSocket::readyRead, this, [this, socket] { handle_ready_read(socket); });
      connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
    }
  }

  void handle_ready_read(QTcpSocket* socket)
  {
    auto& buffer = buffers_[socket];
    buffer.append(socket->readAll());

    while (true) {
      const auto header_end = buffer.indexOf("\r\n\r\n");
      if (header_end < 0) {
        return;
      }

      const auto header = buffer.left(header_end);
      const auto lines = header.split('\n');
      if (lines.isEmpty()) {
        return;
      }

      const auto request_line = lines.front().trimmed().split(' ');
      if (request_line.size() < 2) {
        socket->disconnectFromHost();
        return;
      }

      int content_length = 0;
      RecordedHttpRequest request;
      request.method = QString::fromUtf8(request_line.at(0));
      request.path = QString::fromUtf8(request_line.at(1));

      for (int index = 1; index < lines.size(); ++index) {
        const auto line = lines.at(index).trimmed();
        const auto separator = line.indexOf(':');
        if (separator < 0) {
          continue;
        }

        const auto name = QString::fromUtf8(line.left(separator)).toLower();
        const auto value = QString::fromUtf8(line.mid(separator + 1).trimmed());
        request.headers.insert(name, value);
        if (name == QStringLiteral("content-length")) {
          content_length = value.toInt();
        }
      }

      const auto total_size = header_end + 4 + content_length;
      if (buffer.size() < total_size) {
        return;
      }

      request.body = buffer.mid(header_end + 4, content_length);
      buffer.remove(0, total_size);
      requests.push_back(request);
      respond(socket, request);
    }
  }

  void respond(QTcpSocket* socket, const RecordedHttpRequest& request)
  {
    if (request.method == QStringLiteral("GET")) {
      const QByteArray body =
          "event: message\r\n"
          "data: {\"jsonrpc\":\"2.0\",\"method\":\"notifications/progress\",\"params\":{\"token\":\"startup\"}}\r\n"
          "\r\n";
      write_response(socket, QByteArrayLiteral("200 OK"), QByteArrayLiteral("text/event-stream"), body);
      return;
    }

    if (http_error_response) {
      write_response(socket, QByteArrayLiteral("500 Internal Server Error"), QByteArrayLiteral("text/plain"), QByteArrayLiteral("server failed"));
      return;
    }

    QJsonValue id_value = 1;
    const auto request_document = QJsonDocument::fromJson(request.body);
    if (request_document.isObject() && request_document.object().contains(QStringLiteral("id"))) {
      id_value = request_document.object().value(QStringLiteral("id"));
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("jsonrpc"), QStringLiteral("2.0"));
    payload.insert(QStringLiteral("id"), id_value);
    if (json_rpc_error_response) {
      payload.insert(QStringLiteral("error"), QJsonObject{
          {QStringLiteral("code"), -32601},
          {QStringLiteral("message"), QStringLiteral("missing tool")},
      });
    } else {
      payload.insert(QStringLiteral("result"), QJsonObject{
          {QStringLiteral("ok"), true},
          {QStringLiteral("transport"), QStringLiteral("streamable_http")},
      });
    }

    write_response(
        socket,
        QByteArrayLiteral("200 OK"),
        QByteArrayLiteral("application/json"),
        QJsonDocument(payload).toJson(QJsonDocument::Compact));
  }

  void write_response(QTcpSocket* socket, const QByteArray& status, const QByteArray& content_type, const QByteArray& body)
  {
    QByteArray response;
    response.append("HTTP/1.1 ");
    response.append(status);
    response.append("\r\nContent-Type: ");
    response.append(content_type);
    response.append("\r\nMcp-Session-Id: session-abc");
    response.append("\r\nContent-Length: ");
    response.append(QByteArray::number(body.size()));
    response.append("\r\nConnection: close\r\n\r\n");
    response.append(body);
    socket->write(response);
    socket->disconnectFromHost();
  }

  QTcpServer server_;
  QHash<QTcpSocket*, QByteArray> buffers_;
};

config::ServerProfile http_profile(const MinimalHttpServer& server)
{
  config::ServerProfile profile;
  profile.id = QStringLiteral("http-mock-server");
  profile.name = QStringLiteral("HTTP mock server");
  profile.transport = config::ServerTransportKind::StreamableHttp;
  profile.endpoint_url = server.endpoint_url();
  profile.protocol_version = QStringLiteral("2025-06-18");
  profile.http_headers.insert(QStringLiteral("X-Test-Client"), QStringLiteral("desktop-mcp-inspector"));
  return profile;
}

} // namespace

TEST_CASE("streamable HTTP transport parses GET SSE and POST JSON responses", "[transport][http][integration]")
{
  ensure_qcore_application();
  MinimalHttpServer server;
  REQUIRE(server.listen());

  transport::StreamableHttpTransport subject;
  std::vector<QJsonObject> messages;
  std::vector<transport::TransportError> errors;

  REQUIRE(QObject::connect(
      &subject,
      &transport::Transport::messageReceived,
      [&messages](const QJsonObject& message) { messages.push_back(message); }));
  REQUIRE(QObject::connect(
      &subject,
      &transport::Transport::errorOccurred,
      [&errors](const transport::TransportError& error) { errors.push_back(error); }));

  subject.start(http_profile(server));
  REQUIRE(subject.state() == transport::TransportState::Running);
  REQUIRE(wait_until([&subject] { return subject.session_id() == QStringLiteral("session-abc"); }));
  REQUIRE(wait_until([&messages] {
    return std::any_of(messages.cbegin(), messages.cend(), [](const QJsonObject& message) {
      return message.value(QStringLiteral("method")).toString() == QStringLiteral("notifications/progress");
    });
  }));

  subject.send(QJsonObject{
      {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
      {QStringLiteral("id"), 7},
      {QStringLiteral("method"), QStringLiteral("tools/list")},
  });

  REQUIRE(wait_until([&server] { return server.first_request(QStringLiteral("POST")).has_value(); }));
  REQUIRE(wait_until([&messages] {
    return std::any_of(messages.cbegin(), messages.cend(), [](const QJsonObject& message) {
      return message.value(QStringLiteral("id")).toInt() == 7;
    });
  }));

  const auto post = server.first_request(QStringLiteral("POST"));
  REQUIRE(post.has_value());
  REQUIRE(post->headers.value(QStringLiteral("mcp-session-id")) == QStringLiteral("session-abc"));
  REQUIRE(post->headers.value(QStringLiteral("mcp-protocol-version")) == QStringLiteral("2025-06-18"));
  REQUIRE(post->headers.value(QStringLiteral("x-test-client")) == QStringLiteral("desktop-mcp-inspector"));

  REQUIRE(std::none_of(errors.cbegin(), errors.cend(), [](const transport::TransportError& error) {
    return error.category == transport::TransportErrorCategory::HttpError
        || error.category == transport::TransportErrorCategory::ProtocolViolation;
  }));

  subject.stop();
  REQUIRE(subject.state() == transport::TransportState::Stopped);
}

TEST_CASE("streamable HTTP transport separates HTTP and JSON-RPC errors", "[transport][http][integration]")
{
  ensure_qcore_application();

  MinimalHttpServer http_error_server;
  http_error_server.http_error_response = true;
  REQUIRE(http_error_server.listen());

  transport::StreamableHttpTransport http_subject;
  std::vector<transport::TransportError> http_errors;
  REQUIRE(QObject::connect(
      &http_subject,
      &transport::Transport::errorOccurred,
      [&http_errors](const transport::TransportError& error) { http_errors.push_back(error); }));
  http_subject.start(http_profile(http_error_server));
  REQUIRE(wait_until([&http_subject] { return http_subject.session_id() == QStringLiteral("session-abc"); }));
  http_subject.send(QJsonObject{
      {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
      {QStringLiteral("id"), 8},
      {QStringLiteral("method"), QStringLiteral("tools/list")},
  });

  REQUIRE(wait_until([&http_errors] {
    return std::any_of(http_errors.cbegin(), http_errors.cend(), [](const transport::TransportError& error) {
      return error.category == transport::TransportErrorCategory::HttpError;
    });
  }));
  http_subject.stop();

  MinimalHttpServer json_rpc_error_server;
  json_rpc_error_server.json_rpc_error_response = true;
  REQUIRE(json_rpc_error_server.listen());

  transport::StreamableHttpTransport json_subject;
  std::vector<QJsonObject> messages;
  std::vector<transport::TransportError> json_errors;
  REQUIRE(QObject::connect(
      &json_subject,
      &transport::Transport::messageReceived,
      [&messages](const QJsonObject& message) { messages.push_back(message); }));
  REQUIRE(QObject::connect(
      &json_subject,
      &transport::Transport::errorOccurred,
      [&json_errors](const transport::TransportError& error) { json_errors.push_back(error); }));
  json_subject.start(http_profile(json_rpc_error_server));
  REQUIRE(wait_until([&json_subject] { return json_subject.session_id() == QStringLiteral("session-abc"); }));
  json_subject.send(QJsonObject{
      {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
      {QStringLiteral("id"), 9},
      {QStringLiteral("method"), QStringLiteral("tools/call")},
  });

  REQUIRE(wait_until([&json_errors] {
    return std::any_of(json_errors.cbegin(), json_errors.cend(), [](const transport::TransportError& error) {
      return error.category == transport::TransportErrorCategory::JsonRpcError;
    });
  }));
  REQUIRE(wait_until([&messages] {
    return std::any_of(messages.cbegin(), messages.cend(), [](const QJsonObject& message) {
      return message.value(QStringLiteral("id")).toInt() == 9 && message.contains(QStringLiteral("error"));
    });
  }));
  json_subject.stop();
}
