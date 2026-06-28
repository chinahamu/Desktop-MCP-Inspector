#include "McpCapabilities.hpp"

#include <utility>

namespace mcp {

bool McpCapabilitySet::empty() const
{
  return raw.isEmpty();
}

bool McpCapabilitySet::has(QString key) const
{
  return raw.contains(key) && raw.value(key).isObject();
}

bool McpCapabilitySet::has(McpCapabilityKey key) const
{
  return has(capability_key_name(key));
}

std::optional<QJsonObject> McpCapabilitySet::object(QString key) const
{
  const auto value = raw.value(key);
  if (!value.isObject()) {
    return std::nullopt;
  }

  return value.toObject();
}

std::optional<QJsonObject> McpCapabilitySet::object(McpCapabilityKey key) const
{
  return object(capability_key_name(key));
}

bool McpCapabilitySet::list_changed_supported(McpCapabilityKey key) const
{
  const auto capability_object = object(key);
  if (!capability_object.has_value()) {
    return false;
  }

  return capability_object->value(QStringLiteral("listChanged")).toBool(false);
}

void McpCapabilitiesStore::set_client_capabilities(McpCapabilitySet capabilities)
{
  client_capabilities_ = std::move(capabilities);
}

void McpCapabilitiesStore::set_server_capabilities(McpCapabilitySet capabilities)
{
  server_capabilities_ = std::move(capabilities);
}

void McpCapabilitiesStore::set_from_initialize_params(const McpInitializeParams& params)
{
  set_client_capabilities(capabilities_from_initialize_params(params));
}

void McpCapabilitiesStore::set_from_initialize_result(const McpInitializeResult& result)
{
  set_server_capabilities(capabilities_from_initialize_result(result));
}

bool McpCapabilitiesStore::has_client_capabilities() const
{
  return client_capabilities_.has_value();
}

bool McpCapabilitiesStore::has_server_capabilities() const
{
  return server_capabilities_.has_value();
}

std::optional<McpCapabilitySet> McpCapabilitiesStore::client_capabilities() const
{
  return client_capabilities_;
}

std::optional<McpCapabilitySet> McpCapabilitiesStore::server_capabilities() const
{
  return server_capabilities_;
}

bool McpCapabilitiesStore::server_supports(McpCapabilityKey key) const
{
  return server_capabilities_.has_value() && server_capabilities_->has(key);
}

bool McpCapabilitiesStore::client_supports(McpCapabilityKey key) const
{
  return client_capabilities_.has_value() && client_capabilities_->has(key);
}

void McpCapabilitiesStore::clear()
{
  client_capabilities_.reset();
  server_capabilities_.reset();
}

QString capability_key_name(McpCapabilityKey key)
{
  switch (key) {
    case McpCapabilityKey::Tools:
      return QStringLiteral("tools");
    case McpCapabilityKey::Resources:
      return QStringLiteral("resources");
    case McpCapabilityKey::Prompts:
      return QStringLiteral("prompts");
    case McpCapabilityKey::Logging:
      return QStringLiteral("logging");
    case McpCapabilityKey::Completion:
      return QStringLiteral("completion");
    case McpCapabilityKey::Roots:
      return QStringLiteral("roots");
    case McpCapabilityKey::Sampling:
      return QStringLiteral("sampling");
    case McpCapabilityKey::Elicitation:
      return QStringLiteral("elicitation");
    case McpCapabilityKey::Experimental:
      return QStringLiteral("experimental");
    case McpCapabilityKey::Unknown:
      return QStringLiteral("unknown");
  }

  return QStringLiteral("unknown");
}

McpCapabilityKey capability_key_from_name(const QString& name)
{
  if (name == QStringLiteral("tools")) {
    return McpCapabilityKey::Tools;
  }
  if (name == QStringLiteral("resources")) {
    return McpCapabilityKey::Resources;
  }
  if (name == QStringLiteral("prompts")) {
    return McpCapabilityKey::Prompts;
  }
  if (name == QStringLiteral("logging")) {
    return McpCapabilityKey::Logging;
  }
  if (name == QStringLiteral("completion")) {
    return McpCapabilityKey::Completion;
  }
  if (name == QStringLiteral("roots")) {
    return McpCapabilityKey::Roots;
  }
  if (name == QStringLiteral("sampling")) {
    return McpCapabilityKey::Sampling;
  }
  if (name == QStringLiteral("elicitation")) {
    return McpCapabilityKey::Elicitation;
  }
  if (name == QStringLiteral("experimental")) {
    return McpCapabilityKey::Experimental;
  }

  return McpCapabilityKey::Unknown;
}

McpCapabilitySet capabilities_from_json_object(QJsonObject object)
{
  return McpCapabilitySet{std::move(object)};
}

McpCapabilitySet capabilities_from_initialize_params(const McpInitializeParams& params)
{
  return capabilities_from_json_object(params.capabilities);
}

McpCapabilitySet capabilities_from_initialize_result(const McpInitializeResult& result)
{
  return capabilities_from_json_object(result.capabilities);
}

QJsonObject to_json_object(const McpCapabilitySet& capabilities)
{
  return capabilities.raw;
}

} // namespace mcp
