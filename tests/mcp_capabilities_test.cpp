#include <catch2/catch_test_macros.hpp>

#include "McpCapabilities.hpp"
#include "McpInitialize.hpp"
#include "McpTypes.hpp"

#include <QJsonObject>
#include <QString>

#include <optional>

TEST_CASE("capability keys map to MCP capability names", "[mcp][capabilities]")
{
  REQUIRE(mcp::capability_key_name(mcp::McpCapabilityKey::Tools) == QStringLiteral("tools"));
  REQUIRE(mcp::capability_key_name(mcp::McpCapabilityKey::Resources) == QStringLiteral("resources"));
  REQUIRE(mcp::capability_key_name(mcp::McpCapabilityKey::Prompts) == QStringLiteral("prompts"));
  REQUIRE(mcp::capability_key_name(mcp::McpCapabilityKey::Logging) == QStringLiteral("logging"));
  REQUIRE(mcp::capability_key_name(mcp::McpCapabilityKey::Completion) == QStringLiteral("completion"));
  REQUIRE(mcp::capability_key_name(mcp::McpCapabilityKey::Roots) == QStringLiteral("roots"));
  REQUIRE(mcp::capability_key_name(mcp::McpCapabilityKey::Sampling) == QStringLiteral("sampling"));
  REQUIRE(mcp::capability_key_name(mcp::McpCapabilityKey::Elicitation) == QStringLiteral("elicitation"));
  REQUIRE(mcp::capability_key_name(mcp::McpCapabilityKey::Experimental) == QStringLiteral("experimental"));

  REQUIRE(mcp::capability_key_from_name(QStringLiteral("tools")) == mcp::McpCapabilityKey::Tools);
  REQUIRE(mcp::capability_key_from_name(QStringLiteral("unknown-feature")) == mcp::McpCapabilityKey::Unknown);
}

TEST_CASE("capability set exposes object capabilities and listChanged support", "[mcp][capabilities]")
{
  const auto capabilities = mcp::capabilities_from_json_object(QJsonObject{
      {QStringLiteral("tools"), QJsonObject{{QStringLiteral("listChanged"), true}}},
      {QStringLiteral("resources"), QJsonObject{{QStringLiteral("subscribe"), true}, {QStringLiteral("listChanged"), false}}},
      {QStringLiteral("invalid"), QStringLiteral("not-object")},
  });

  REQUIRE_FALSE(capabilities.empty());
  REQUIRE(capabilities.has(mcp::McpCapabilityKey::Tools));
  REQUIRE(capabilities.has(QStringLiteral("resources")));
  REQUIRE_FALSE(capabilities.has(QStringLiteral("invalid")));
  REQUIRE(capabilities.list_changed_supported(mcp::McpCapabilityKey::Tools));
  REQUIRE_FALSE(capabilities.list_changed_supported(mcp::McpCapabilityKey::Resources));
  REQUIRE(capabilities.object(mcp::McpCapabilityKey::Resources).has_value());
  REQUIRE_FALSE(capabilities.object(QStringLiteral("missing")).has_value());
}

TEST_CASE("capabilities can be extracted from initialize params and result", "[mcp][capabilities]")
{
  const mcp::McpInitializeParams params{
      mcp::default_protocol_version(),
      mcp::default_client_info(QStringLiteral("0.1.0")),
      QJsonObject{{QStringLiteral("roots"), QJsonObject{{QStringLiteral("listChanged"), true}}}},
  };

  const mcp::McpInitializeResult result{
      mcp::default_protocol_version(),
      mcp::McpServerInfo{QStringLiteral("test-server"), QStringLiteral("1.0.0")},
      QJsonObject{{QStringLiteral("tools"), QJsonObject{}}, {QStringLiteral("logging"), QJsonObject{}}},
      std::nullopt,
  };

  const auto client_capabilities = mcp::capabilities_from_initialize_params(params);
  const auto server_capabilities = mcp::capabilities_from_initialize_result(result);

  REQUIRE(client_capabilities.has(mcp::McpCapabilityKey::Roots));
  REQUIRE(client_capabilities.list_changed_supported(mcp::McpCapabilityKey::Roots));
  REQUIRE(server_capabilities.has(mcp::McpCapabilityKey::Tools));
  REQUIRE(server_capabilities.has(mcp::McpCapabilityKey::Logging));
}

TEST_CASE("capabilities store keeps client and server capabilities", "[mcp][capabilities]")
{
  mcp::McpCapabilitiesStore store;
  REQUIRE_FALSE(store.has_client_capabilities());
  REQUIRE_FALSE(store.has_server_capabilities());

  store.set_client_capabilities(mcp::capabilities_from_json_object(QJsonObject{
      {QStringLiteral("sampling"), QJsonObject{}},
  }));
  store.set_server_capabilities(mcp::capabilities_from_json_object(QJsonObject{
      {QStringLiteral("prompts"), QJsonObject{{QStringLiteral("listChanged"), true}}},
  }));

  REQUIRE(store.has_client_capabilities());
  REQUIRE(store.has_server_capabilities());
  REQUIRE(store.client_supports(mcp::McpCapabilityKey::Sampling));
  REQUIRE(store.server_supports(mcp::McpCapabilityKey::Prompts));
  REQUIRE_FALSE(store.server_supports(mcp::McpCapabilityKey::Tools));
  REQUIRE(store.server_capabilities()->list_changed_supported(mcp::McpCapabilityKey::Prompts));

  store.clear();
  REQUIRE_FALSE(store.has_client_capabilities());
  REQUIRE_FALSE(store.has_server_capabilities());
}

TEST_CASE("capabilities store imports initialize exchange data", "[mcp][capabilities]")
{
  const mcp::McpInitializeParams params{
      mcp::default_protocol_version(),
      mcp::default_client_info(QStringLiteral("0.1.0")),
      QJsonObject{{QStringLiteral("elicitation"), QJsonObject{}}},
  };
  const mcp::McpInitializeResult result{
      mcp::default_protocol_version(),
      mcp::McpServerInfo{QStringLiteral("server"), QStringLiteral("1")},
      QJsonObject{{QStringLiteral("resources"), QJsonObject{{QStringLiteral("subscribe"), true}}}},
      std::nullopt,
  };

  mcp::McpCapabilitiesStore store;
  store.set_from_initialize_params(params);
  store.set_from_initialize_result(result);

  REQUIRE(store.client_supports(mcp::McpCapabilityKey::Elicitation));
  REQUIRE(store.server_supports(mcp::McpCapabilityKey::Resources));
  REQUIRE(store.server_capabilities()->object(mcp::McpCapabilityKey::Resources)->value(QStringLiteral("subscribe")).toBool());
}
