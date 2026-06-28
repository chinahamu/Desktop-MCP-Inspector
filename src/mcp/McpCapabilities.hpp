#pragma once

#include "McpInitialize.hpp"

#include <QJsonObject>
#include <QString>

#include <optional>

namespace mcp {

enum class McpCapabilityKey {
  Tools,
  Resources,
  Prompts,
  Logging,
  Completion,
  Roots,
  Sampling,
  Elicitation,
  Experimental,
  Unknown,
};

struct McpCapabilitySet
{
  QJsonObject raw;

  [[nodiscard]] bool empty() const;
  [[nodiscard]] bool has(QString key) const;
  [[nodiscard]] bool has(McpCapabilityKey key) const;
  [[nodiscard]] std::optional<QJsonObject> object(QString key) const;
  [[nodiscard]] std::optional<QJsonObject> object(McpCapabilityKey key) const;
  [[nodiscard]] bool list_changed_supported(McpCapabilityKey key) const;
};

class McpCapabilitiesStore
{
public:
  void set_client_capabilities(McpCapabilitySet capabilities);
  void set_server_capabilities(McpCapabilitySet capabilities);
  void set_from_initialize_params(const McpInitializeParams& params);
  void set_from_initialize_result(const McpInitializeResult& result);

  [[nodiscard]] bool has_client_capabilities() const;
  [[nodiscard]] bool has_server_capabilities() const;
  [[nodiscard]] std::optional<McpCapabilitySet> client_capabilities() const;
  [[nodiscard]] std::optional<McpCapabilitySet> server_capabilities() const;
  [[nodiscard]] bool server_supports(McpCapabilityKey key) const;
  [[nodiscard]] bool client_supports(McpCapabilityKey key) const;

  void clear();

private:
  std::optional<McpCapabilitySet> client_capabilities_;
  std::optional<McpCapabilitySet> server_capabilities_;
};

[[nodiscard]] QString capability_key_name(McpCapabilityKey key);
[[nodiscard]] McpCapabilityKey capability_key_from_name(const QString& name);
[[nodiscard]] McpCapabilitySet capabilities_from_json_object(QJsonObject object);
[[nodiscard]] McpCapabilitySet capabilities_from_initialize_params(const McpInitializeParams& params);
[[nodiscard]] McpCapabilitySet capabilities_from_initialize_result(const McpInitializeResult& result);
[[nodiscard]] QJsonObject to_json_object(const McpCapabilitySet& capabilities);

} // namespace mcp
