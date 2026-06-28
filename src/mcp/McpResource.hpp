#pragma once

#include "McpClient.hpp"
#include "McpTypes.hpp"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QtTypes>

#include <optional>
#include <variant>
#include <vector>

namespace mcp {

inline constexpr auto kMcpResourcesListMethod = "resources/list";
inline constexpr auto kMcpResourcesReadMethod = "resources/read";

struct McpResource
{
  QString uri;
  QString name;
  std::optional<QString> title;
  std::optional<QString> description;
  std::optional<QString> mime_type;
  std::optional<qint64> size;
  std::optional<QJsonObject> annotations;

  friend bool operator==(const McpResource& lhs, const McpResource& rhs) = default;
};

struct McpResourcesListResult
{
  std::vector<McpResource> resources;
  std::optional<QString> next_cursor;
};

struct McpResourceReadResult
{
  QJsonArray contents;
  QJsonObject raw_result;
};

enum class McpResourceParseErrorCode {
  ResponseError,
  MissingResult,
  InvalidResult,
  InvalidResource,
  InvalidResources,
  InvalidCursor,
  InvalidContent,
};

struct McpResourceParseError
{
  McpResourceParseErrorCode code = McpResourceParseErrorCode::InvalidResult;
  QString message;
};

using McpResourcesListParseResult = std::variant<McpResourcesListResult, McpResourceParseError>;
using McpResourceReadParseResult = std::variant<McpResourceReadResult, McpResourceParseError>;

[[nodiscard]] bool is_valid_resource(const McpResource& resource);
[[nodiscard]] QJsonObject to_json_object(const McpResource& resource);
[[nodiscard]] QJsonObject to_json_object(const McpResourcesListResult& result);
[[nodiscard]] QJsonObject to_json_object(const McpResourceReadResult& result);
[[nodiscard]] std::optional<McpResource> resource_from_json_object(const QJsonObject& object);

[[nodiscard]] JsonRpcRequest make_resources_list_request(
    PendingRequestStore& pending_requests,
    std::optional<QString> cursor = std::nullopt);
[[nodiscard]] JsonRpcRequest make_resources_read_request(
    PendingRequestStore& pending_requests,
    QString uri);

[[nodiscard]] McpResourcesListParseResult parse_resources_list_response(const JsonRpcResponse& response);
[[nodiscard]] McpResourcesListParseResult parse_resources_list_result(const QJsonObject& result);
[[nodiscard]] McpResourceReadParseResult parse_resources_read_response(const JsonRpcResponse& response);
[[nodiscard]] McpResourceReadParseResult parse_resources_read_result(const QJsonObject& result);

} // namespace mcp
