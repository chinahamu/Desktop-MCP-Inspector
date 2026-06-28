#include "StreamableHttpTransport.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QVariant>

#include <utility>

namespace transport {
namespace {

[[nodiscard]] QByteArray raw_header_name(const QString& value)
{
  return value.toUtf8();
}

[[nodiscard]] std::optional<int> http_status_code(QNetworkReply* reply)
{
  const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
  if (!status.isValid()) {
    return std::nullopt;
  }

  return status.toInt();
}

[[nodiscard]] QString json_rpc_error_message(const QJsonObject& error)
{
  const auto message = error.value(QStringLiteral("message")).toString();
  return message.isEmpty() ? QStringLiteral("JSON-RPC error response") : message;
}

} // namespace

StreamableHttpTransport::StreamableHttpTransport(QObject* parent)
  : Transport(parent)
{
}

StreamableHttpTransport::~StreamableHttpTransport()
{
  stop();
}

void StreamableHttpTransport::start(const config::ServerProfile& profile)
{
  if (!config::is_streamable_http_profile(profile)) {
    emit_error(
        TransportErrorCategory::StartFailed,
        QStringLiteral("StreamableHttpTransport can only start streamable HTTP profiles"));
    set_state(TransportState::Failed);
    return;
  }

  const QUrl endpoint(profile.endpoint_url.trimmed(), QUrl::StrictMode);
  const auto scheme = endpoint.scheme().toLower();
  if (!endpoint.isValid() || endpoint.host().isEmpty() || (scheme != QStringLiteral("http") && scheme != QStringLiteral("https"))) {
    emit_error(
        TransportErrorCategory::StartFailed,
        QStringLiteral("streamable HTTP endpoint URL is invalid"),
        QJsonObject{{QStringLiteral("endpoint_url"), profile.endpoint_url}});
    set_state(TransportState::Failed);
    return;
  }

  stop_requested_ = false;
  profile_ = profile;
  if (profile_.protocol_version.trimmed().isEmpty()) {
    profile_.protocol_version = config::default_mcp_protocol_version();
  }

  manager_ = std::make_unique<QNetworkAccessManager>();
  stream_parser_.clear();
  response_parser_.clear();
  session_id_.clear();

  set_state(TransportState::Starting);
  set_state(TransportState::Running);
  open_sse_stream();
}

void StreamableHttpTransport::stop()
{
  stop_requested_ = true;

  if (!stream_reply_.isNull()) {
    auto* reply = stream_reply_.data();
    stream_reply_.clear();
    disconnect(reply, nullptr, this, nullptr);
    if (reply->isRunning()) {
      reply->abort();
    }
  }

  stream_parser_.clear();
  response_parser_.clear();
  manager_.reset();

  if (state_ != TransportState::Idle && state_ != TransportState::Failed) {
    set_state(TransportState::Stopped);
  }
}

void StreamableHttpTransport::send(const QJsonObject& message)
{
  if (state_ != TransportState::Running || manager_ == nullptr) {
    emit_error(TransportErrorCategory::WriteFailed, QStringLiteral("streamable HTTP transport is not running"));
    return;
  }

  auto request = make_request(QStringLiteral("POST"));
  request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
  request.setRawHeader("Accept", "application/json, text/event-stream");

  const auto payload = QJsonDocument(message).toJson(QJsonDocument::Compact);
  auto* reply = manager_->post(request, payload);
  connect(reply, &QNetworkReply::finished, this, [this, reply] { handle_post_finished(reply); });
}

TransportState StreamableHttpTransport::state() const
{
  return state_;
}

QString StreamableHttpTransport::session_id() const
{
  return session_id_;
}

void StreamableHttpTransport::set_state(TransportState next_state)
{
  if (state_ == next_state) {
    return;
  }

  state_ = next_state;
  emit stateChanged(state_);
}

void StreamableHttpTransport::emit_error(TransportErrorCategory category, QString message, std::optional<QJsonObject> details)
{
  TransportError error;
  error.category = category;
  error.message = std::move(message);
  error.details = std::move(details);
  emit errorOccurred(error);
}

void StreamableHttpTransport::emit_http_error(QNetworkReply* reply, const QByteArray& body)
{
  QJsonObject details;
  if (const auto status = http_status_code(reply); status.has_value()) {
    details.insert(QStringLiteral("http_status"), *status);
  }
  details.insert(QStringLiteral("body"), QString::fromUtf8(body.left(4096)));

  emit_error(
      TransportErrorCategory::HttpError,
      reply->errorString().isEmpty() ? QStringLiteral("HTTP request failed") : reply->errorString(),
      details);
}

void StreamableHttpTransport::emit_json_rpc_error(const QJsonObject& message)
{
  const auto error_object = message.value(QStringLiteral("error")).toObject();

  TransportError error;
  error.category = TransportErrorCategory::JsonRpcError;
  error.message = json_rpc_error_message(error_object);
  error.details = QJsonObject{
      {QStringLiteral("jsonrpc"), message},
  };
  emit errorOccurred(error);
}

void StreamableHttpTransport::emit_protocol_violation(QString message, QJsonObject details)
{
  emit_error(TransportErrorCategory::ProtocolViolation, std::move(message), std::move(details));
}

void StreamableHttpTransport::open_sse_stream()
{
  if (manager_ == nullptr) {
    return;
  }

  auto request = make_request(QStringLiteral("GET"));
  request.setRawHeader("Accept", "text/event-stream");
  stream_reply_ = manager_->get(request);

  connect(stream_reply_, &QNetworkReply::readyRead, this, &StreamableHttpTransport::handle_stream_ready);
  connect(stream_reply_, &QNetworkReply::finished, this, &StreamableHttpTransport::handle_stream_finished);
}

void StreamableHttpTransport::handle_stream_ready()
{
  if (stream_reply_.isNull()) {
    return;
  }

  update_session_from_reply(stream_reply_);
  handle_sse_events(stream_parser_.append(stream_reply_->readAll()));
}

void StreamableHttpTransport::handle_stream_finished()
{
  if (stream_reply_.isNull()) {
    return;
  }

  auto* reply = stream_reply_.data();
  update_session_from_reply(reply);
  const auto tail = reply->readAll();
  if (!tail.isEmpty()) {
    handle_sse_events(stream_parser_.append(tail));
  }

  const auto status = http_status_code(reply);
  if (status.has_value() && *status >= 400) {
    emit_http_error(reply, tail);
  } else if (!stop_requested_ && reply->error() != QNetworkReply::NoError
             && reply->error() != QNetworkReply::OperationCanceledError) {
    emit_error(TransportErrorCategory::ReadFailed, reply->errorString());
  }

  reply->deleteLater();
  stream_reply_.clear();
}

void StreamableHttpTransport::handle_post_finished(QNetworkReply* reply)
{
  update_session_from_reply(reply);
  const auto body = reply->readAll();
  const auto status = http_status_code(reply);

  if (status.has_value() && *status >= 400) {
    emit_http_error(reply, body);
  } else if (reply->error() != QNetworkReply::NoError) {
    emit_error(TransportErrorCategory::ReadFailed, reply->errorString());
  } else {
    handle_payload(reply, body);
  }

  reply->deleteLater();
}

void StreamableHttpTransport::handle_payload(QNetworkReply* reply, const QByteArray& body)
{
  const auto content_type = reply->header(QNetworkRequest::ContentTypeHeader).toString().toLower();
  if (content_type.contains(QStringLiteral("text/event-stream"))) {
    handle_sse_payload(body);
    return;
  }

  handle_json_payload(body);
}

void StreamableHttpTransport::handle_json_payload(const QByteArray& body)
{
  QJsonParseError parse_error;
  const auto document = QJsonDocument::fromJson(body, &parse_error);
  if (parse_error.error != QJsonParseError::NoError) {
    emit_protocol_violation(
        QStringLiteral("HTTP response body was not valid JSON: %1").arg(parse_error.errorString()),
        QJsonObject{{QStringLiteral("offset"), parse_error.offset}});
    return;
  }

  if (document.isObject()) {
    handle_json_object(document.object());
    return;
  }

  if (document.isArray()) {
    for (const auto& value : document.array()) {
      if (!value.isObject()) {
        emit_protocol_violation(QStringLiteral("JSON-RPC batch response contained a non-object item"));
        continue;
      }
      handle_json_object(value.toObject());
    }
    return;
  }

  emit_protocol_violation(QStringLiteral("HTTP response JSON was neither an object nor an array"));
}

void StreamableHttpTransport::handle_sse_payload(const QByteArray& body)
{
  response_parser_.clear();
  handle_sse_events(response_parser_.append(body));
}

void StreamableHttpTransport::handle_sse_events(const std::vector<HttpSseEvent>& events)
{
  for (const auto& event : events) {
    if (event.data.trimmed() == QStringLiteral("[DONE]")) {
      continue;
    }

    handle_json_payload(event.data.toUtf8());
  }
}

void StreamableHttpTransport::handle_json_object(const QJsonObject& object)
{
  if (object.value(QStringLiteral("jsonrpc")).toString() != QStringLiteral("2.0")) {
    emit_protocol_violation(
        QStringLiteral("HTTP response object was not a JSON-RPC 2.0 message"),
        QJsonObject{{QStringLiteral("message"), object}});
    return;
  }

  if (object.contains(QStringLiteral("error"))) {
    emit_json_rpc_error(object);
  }

  emit messageReceived(object);
}

QNetworkRequest StreamableHttpTransport::make_request(const QString& verb) const
{
  QNetworkRequest request{QUrl(profile_.endpoint_url.trimmed())};
  request.setRawHeader("MCP-Protocol-Version", profile_.protocol_version.trimmed().toUtf8());
  if (!session_id_.trimmed().isEmpty()) {
    request.setRawHeader("Mcp-Session-Id", session_id_.trimmed().toUtf8());
  }

  request.setRawHeader("X-Desktop-MCP-Inspector-Transport", verb.toUtf8().toLower());
  for (auto it = profile_.http_headers.cbegin(); it != profile_.http_headers.cend(); ++it) {
    request.setRawHeader(raw_header_name(it.key()), it.value().toUtf8());
  }

  return request;
}

void StreamableHttpTransport::update_session_from_reply(QNetworkReply* reply)
{
  const auto header = reply->rawHeader("Mcp-Session-Id");
  if (!header.trimmed().isEmpty()) {
    session_id_ = QString::fromUtf8(header.trimmed());
  }
}

} // namespace transport
