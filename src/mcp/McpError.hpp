#pragma once

#include "McpTypes.hpp"

#include <QString>

namespace mcp {

enum class McpErrorCategory {
  Parse,
  InvalidRequest,
  MethodNotFound,
  InvalidParams,
  Internal,
  Server,
  Transport,
  Unknown,
};

enum class McpErrorSeverity {
  Info,
  Warning,
  Error,
  Critical,
};

struct McpErrorDescriptor
{
  McpErrorCategory category = McpErrorCategory::Unknown;
  McpErrorSeverity severity = McpErrorSeverity::Error;
  QString title;
  QString detail;
  QString recommendation;
  bool retryable = false;
};

[[nodiscard]] McpErrorCategory category_for_error_code(int code);
[[nodiscard]] McpErrorSeverity severity_for_category(McpErrorCategory category);
[[nodiscard]] QString category_name(McpErrorCategory category);
[[nodiscard]] QString severity_name(McpErrorSeverity severity);
[[nodiscard]] QString title_for_error_code(int code);
[[nodiscard]] QString recommendation_for_category(McpErrorCategory category);
[[nodiscard]] bool is_retryable_error_code(int code);

[[nodiscard]] McpErrorDescriptor describe_error(const JsonRpcError& error);
[[nodiscard]] McpErrorDescriptor describe_error(const JsonRpcResponse& response);

} // namespace mcp
