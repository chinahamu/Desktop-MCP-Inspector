#include <catch2/catch_test_macros.hpp>

#include "McpResource.hpp"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include <stdexcept>
#include <variant>

TEST_CASE("McpResource serializes and parses resource descriptors", "[mcp][resources]")
{
  const mcp::McpResource resource{
      QStringLiteral("demo://readme.md"),
      QStringLiteral("demo-readme"),
      QStringLiteral("Demo README"),
      QStringLiteral("A markdown demo resource"),
      QStringLiteral("text/markdown"),
      128,
      QJsonObject{{QStringLiteral("audience"), QStringLiteral("developer")}},
  };

  const auto object = mcp::to_json_object(resource);
  REQUIRE(object.value(QStringLiteral("uri")).toString() == QStringLiteral("demo://readme.md"));
  REQUIRE(object.value(QStringLiteral("name")).toString() == QStringLiteral("demo-readme"));
  REQUIRE(object.value(QStringLiteral("mimeType")).toString() == QStringLiteral("text/markdown"));
  REQUIRE(object.value(QStringLiteral("size")).toInt() == 128);

  const auto parsed = mcp::resource_from_json_object(object);
  REQUIRE(parsed.has_value());
  REQUIRE(*parsed == resource);
}

TEST_CASE("resources/list request is tracked and supports pagination cursor", "[mcp][resources]")
{
  mcp::PendingRequestStore pending_requests;

  const auto request = mcp::make_resources_list_request(pending_requests, QStringLiteral("cursor-1"));

  REQUIRE(request.id == mcp::RequestId::number(1));
  REQUIRE(request.method == QStringLiteral("resources/list"));
  REQUIRE(request.params.has_value());
  REQUIRE(request.params->toObject().value(QStringLiteral("cursor")).toString() == QStringLiteral("cursor-1"));
  REQUIRE(pending_requests.contains(request.id));
}

TEST_CASE("resources/list response parses resources and next cursor", "[mcp][resources]")
{
  mcp::JsonRpcResponse response;
  response.id = mcp::RequestId::number(1);
  response.result = QJsonValue{QJsonObject{
      {QStringLiteral("resources"), QJsonArray{
          QJsonObject{
              {QStringLiteral("uri"), QStringLiteral("demo://config.json")},
              {QStringLiteral("name"), QStringLiteral("session-config")},
              {QStringLiteral("description"), QStringLiteral("Session config")},
              {QStringLiteral("mimeType"), QStringLiteral("application/json")},
          },
      }},
      {QStringLiteral("nextCursor"), QStringLiteral("cursor-2")},
  }};

  const auto parsed = mcp::parse_resources_list_response(response);
  const auto* result = std::get_if<mcp::McpResourcesListResult>(&parsed);

  REQUIRE(result != nullptr);
  REQUIRE(result->resources.size() == 1);
  REQUIRE(result->resources.front().uri == QStringLiteral("demo://config.json"));
  REQUIRE(result->resources.front().mime_type == QStringLiteral("application/json"));
  REQUIRE(result->next_cursor == QStringLiteral("cursor-2"));
}

TEST_CASE("resources/list validates malformed resource entries", "[mcp][resources]")
{
  const auto parsed = mcp::parse_resources_list_result(QJsonObject{
      {QStringLiteral("resources"), QJsonArray{
          QJsonObject{
              {QStringLiteral("name"), QStringLiteral("missing-uri")},
          },
      }},
  });

  const auto* error = std::get_if<mcp::McpResourceParseError>(&parsed);
  REQUIRE(error != nullptr);
  REQUIRE(error->code == mcp::McpResourceParseErrorCode::InvalidResource);
}

TEST_CASE("resources/read request serializes resource URI", "[mcp][resources]")
{
  mcp::PendingRequestStore pending_requests;

  const auto request = mcp::make_resources_read_request(pending_requests, QStringLiteral("demo://readme.md"));

  REQUIRE(request.id == mcp::RequestId::number(1));
  REQUIRE(request.method == QStringLiteral("resources/read"));
  REQUIRE(request.params.has_value());
  REQUIRE(request.params->toObject().value(QStringLiteral("uri")).toString() == QStringLiteral("demo://readme.md"));
  REQUIRE(pending_requests.contains(request.id));
}

TEST_CASE("resources/read rejects blank URIs", "[mcp][resources]")
{
  mcp::PendingRequestStore pending_requests;

  REQUIRE_THROWS_AS(
      mcp::make_resources_read_request(pending_requests, QStringLiteral("   ")),
      std::invalid_argument);
}

TEST_CASE("resources/read response parses content array", "[mcp][resources]")
{
  mcp::JsonRpcResponse response;
  response.id = mcp::RequestId::number(1);
  response.result = QJsonValue{QJsonObject{
      {QStringLiteral("contents"), QJsonArray{
          QJsonObject{
              {QStringLiteral("uri"), QStringLiteral("demo://readme.md")},
              {QStringLiteral("mimeType"), QStringLiteral("text/markdown")},
              {QStringLiteral("text"), QStringLiteral("# Demo")},
          },
      }},
  }};

  const auto parsed = mcp::parse_resources_read_response(response);
  const auto* result = std::get_if<mcp::McpResourceReadResult>(&parsed);

  REQUIRE(result != nullptr);
  REQUIRE(result->contents.size() == 1);
  REQUIRE(result->contents.at(0).toObject().value(QStringLiteral("text")).toString() == QStringLiteral("# Demo"));
  REQUIRE(result->raw_result.contains(QStringLiteral("contents")));
}
