#pragma once

#include "HttpSseParser.hpp"
#include "ServerProfile.hpp"
#include "Transport.hpp"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QUrl>

#include <memory>
#include <optional>

class QNetworkAccessManager;
class QNetworkReply;

namespace transport {

class StreamableHttpTransport final : public Transport
{
  Q_OBJECT

public:
  explicit StreamableHttpTransport(QObject* parent = nullptr);
  ~StreamableHttpTransport() override;

  void start(const config::ServerProfile& profile) override;
  void stop() override;
  void send(const QJsonObject& message) override;

  [[nodiscard]] TransportState state() const override;
  [[nodiscard]] QString session_id() const;

private:
  void set_state(TransportState next_state);
  void emit_error(TransportErrorCategory category, QString message, std::optional<QJsonObject> details = std::nullopt);
  void emit_http_error(QNetworkReply* reply, const QByteArray& body);
  void emit_json_rpc_error(const QJsonObject& message);
  void emit_protocol_violation(QString message, QJsonObject details = {});
  void open_sse_stream();
  void handle_stream_ready();
  void handle_stream_finished();
  void handle_post_finished(QNetworkReply* reply);
  void handle_payload(QNetworkReply* reply, const QByteArray& body);
  void handle_json_payload(const QByteArray& body);
  void handle_sse_payload(const QByteArray& body);
  void handle_sse_events(const std::vector<HttpSseEvent>& events);
  void handle_json_object(const QJsonObject& object);
  [[nodiscard]] QNetworkRequest make_request(const QString& verb) const;
  void update_session_from_reply(QNetworkReply* reply);

  config::ServerProfile profile_;
  std::unique_ptr<QNetworkAccessManager> manager_;
  QPointer<QNetworkReply> stream_reply_;
  HttpSseParser stream_parser_;
  HttpSseParser response_parser_;
  TransportState state_ = TransportState::Idle;
  QString session_id_;
  bool stop_requested_ = false;
};

} // namespace transport
