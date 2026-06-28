#include "McpResource.hpp"

#include <QJsonValue>

#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace mcp {
namespace {

[[nodiscard]] McpResourceParseError make_error(McpResourceParseErrorCode code, QString message)
{
  return {code, std::move(message)};
}

[[nodiscard]] bool is_present_string(const QJsonValue& value)
{
  return !value.isUndefined() && value.isString();
}

[[nodiscard]] std::optional<qint64> integer_from_json_value(const QJsonValue& value)
{
  if (!value.isDouble()) {
    return std::nullopt;
  }

  const auto number = value.toDouble();
  if (!std::isfinite(number) || std::trunc(number) != number) {
    return std::nullopt;
  }
  if (number < 0.0 || number > static_cast<double>(std::numeric_limits<qint64>::max())) {
    return std::nullopt;
  }

  return static_cast<qint64>(number);
}

} // namespace

bool is_valid_resource(const McpResource& resource)
{
  return !resource.uri.trimmed().isEmpty() && !resource.name.trimmed().isEmpty();
}

QJsonObject to_json_object(const McpResource& resource)
{
  QJsonObject object;
  object.insert(QStringLiteral("uri"), resource.uri);
  object.insert(QStringLiteral("name"), resource.name);
  if (resource.title.has_value()) {
    object.insert(QStringLiteral("title"), *resource.title);
  }
  if (resource.description.has_value()) {
    object.insert(QStringLiteral("description"), *resource.description);
  }
  if (resource.mime_type.has_value()) {
    object.insert(QStringLiteral("mimeType"), *resource.mime_type);
  }
  if (resource.size.has_value()) {
    object.insert(QStringLiteral("size"), static_cast<double>(*resource.size));
  }
  if (resource.annotations.has_value()) {
    object.insert(QStringLiteral("annotations"), *resource.annotations);
  }
  return object;
}

QJsonObject to_json_object(const McpResourcesListResult& result)
{
  QJsonArray resources;
  for (const auto& resource : result.resources) {
    resources.append(to_json_object(resource));
  }

  QJsonObject object;
  object.insert(QStringLiteral("resources"), resources);
  if (result.next_cursor.has_value()) {
    object.insert(QStringLiteral("nextCursor"), *result.next_cursor);
  }
  return object;
}

QJsonObject to_json_object(const McpResourceReadResult& result)
{
  QJsonObject object = result.raw_result;
  object.insert(QStringLiteral("contents"), result.contents);
  return object;
}

std::optional<McpResource> resource_from_json_object(const QJsonObject& object)
{
  const auto uri_value = object.value(QStringLiteral("uri"));
  if (!uri_value.isString() || uri_value.toString().trimmed().isEmpty()) {
    return std::nullopt;
  }

  const auto name_value = object.value(QStringLiteral("name"));
  if (!name_value.isString() || name_value.toString().trimmed().isEmpty()) {
    return std::nullopt;
  }

  McpResource resource;
  resource.uri = uri_value.toString();
  resource.name = name_value.toString();

  const auto title_value = object.value(QStringLiteral("title"));
  if (!title_value.isUndefined()) {
    if (!title_value.isString()) {
      return std::nullopt;
    }
    resource.title = title_value.toString();
  }

  const auto description_value = object.value(QStringLiteral("description"));
  if (!description_value.isUndefined()) {
    if (!description_value.isString()) {
      return std::nullopt;
    }
    resource.description = description_value.toString();
  }

  const auto mime_type_value = object.value(QStringLiteral("mimeType"));
  if (!mime_type_value.isUndefined()) {
    if (!mime_type_value.isString()) {
      return std::nullopt;
    }
    resource.mime_type = mime_type_value.toString();
  }

  const auto size_value = object.value(QStringLiteral("size"));
  if (!size_value.isUndefined()) {
    auto parsed_size = integer_from_json_value(size_value);
    if (!parsed_size.has_value()) {
      return std::nullopt;
    }
    resource.size = parsed_size;
  }

  const auto annotations_value = object.value(QStringLiteral("annotations"));
  if (!annotations_value.isUndefined()) {
    if (!annotations_value.isObject()) {
      return std::nullopt;
    }
    resource.annotations = annotations_value.toObject();
  }

  if (!is_valid_resource(resource)) {
    return std::nullopt;
  }

  return resource;
}

JsonRpcRequest make_resources_list_request(PendingRequestStore& pending_requests, std::optional<QString> cursor)
{
  if (!cursor.has_value() || cursor->isEmpty()) {
    return pending_requests.make_request(QString::fromUtf8(kMcpResourcesListMethod));
  }

  return pending_requests.make_request(
      QString::fromUtf8(kMcpResourcesListMethod),
      QJsonValue{QJsonObject{{QStringLiteral("cursor"), *cursor}}});
}

JsonRpcRequest make_resources_read_request(PendingRequestStore& pending_requests, QString uri)
{
  if (uri.trimmed().isEmpty()) {
    throw std::invalid_argument{"resources/read requires a non-empty resource URI"};
  }

  return pending_requests.make_request(
      QString::fromUtf8(kMcpResourcesReadMethod),
      QJsonValue{QJsonObject{{QStringLiteral("uri"), std::move(uri)}}});
}

McpResourcesListParseResult parse_resources_list_response(const JsonRpcResponse& response)
{
  if (response.error.has_value()) {
    return make_error(McpResourceParseErrorCode::ResponseError, response.error->message);
  }

  if (!response.result.has_value()) {
    return make_error(McpResourceParseErrorCode::MissingResult, QStringLiteral("resources/list response does not contain a result"));
  }

  if (!response.result->isObject()) {
    return make_error(McpResourceParseErrorCode::InvalidResult, QStringLiteral("resources/list result must be a JSON object"));
  }

  return parse_resources_list_result(response.result->toObject());
}

McpResourcesListParseResult parse_resources_list_result(const QJsonObject& result)
{
  const auto resources_value = result.value(QStringLiteral("resources"));
  if (!resources_value.isArray()) {
    return make_error(McpResourceParseErrorCode::InvalidResources, QStringLiteral("resources/list result resources must be an array"));
  }

  McpResourcesListResult list_result;
  const auto resources = resources_value.toArray();
  list_result.resources.reserve(static_cast<std::size_t>(resources.size()));
  for (const auto& value : resources) {
    if (!value.isObject()) {
      return make_error(McpResourceParseErrorCode::InvalidResource, QStringLiteral("resources/list entries must be objects"));
    }

    auto resource = resource_from_json_object(value.toObject());
    if (!resource.has_value()) {
      return make_error(McpResourceParseErrorCode::InvalidResource, QStringLiteral("resources/list entry must contain uri and name"));
    }

    list_result.resources.push_back(std::move(*resource));
  }

  const auto cursor_value = result.value(QStringLiteral("nextCursor"));
  if (!cursor_value.isUndefined()) {
    if (!is_present_string(cursor_value)) {
      return make_error(McpResourceParseErrorCode::InvalidCursor, QStringLiteral("resources/list nextCursor must be a string when present"));
    }
    list_result.next_cursor = cursor_value.toString();
  }

  return list_result;
}

McpResourceReadParseResult parse_resources_read_response(const JsonRpcResponse& response)
{
  if (response.error.has_value()) {
    return make_error(McpResourceParseErrorCode::ResponseError, response.error->message);
  }

  if (!response.result.has_value()) {
    return make_error(McpResourceParseErrorCode::MissingResult, QStringLiteral("resources/read response does not contain a result"));
  }

  if (!response.result->isObject()) {
    return make_error(McpResourceParseErrorCode::InvalidResult, QStringLiteral("resources/read result must be a JSON object"));
  }

  return parse_resources_read_result(response.result->toObject());
}

McpResourceReadParseResult parse_resources_read_result(const QJsonObject& result)
{
  const auto contents_value = result.value(QStringLiteral("contents"));
  if (!contents_value.isArray()) {
    return make_error(McpResourceParseErrorCode::InvalidContent, QStringLiteral("resources/read result contents must be an array"));
  }

  const auto contents = contents_value.toArray();
  for (const auto& value : contents) {
    if (!value.isObject()) {
      return make_error(McpResourceParseErrorCode::InvalidContent, QStringLiteral("resources/read content entries must be objects"));
    }
  }

  McpResourceReadResult read_result;
  read_result.contents = contents;
  read_result.raw_result = result;
  return read_result;
}

} // namespace mcp
