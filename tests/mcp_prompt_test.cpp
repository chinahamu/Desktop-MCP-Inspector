#include <catch2/catch_test_macros.hpp>

#include "McpPrompt.hpp"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include <stdexcept>
#include <variant>

TEST_CASE("McpPrompt serializes and parses prompt descriptors", "[mcp][prompts]")
{
  const mcp::McpPrompt prompt{
      QStringLiteral("summarize-resource"),
      QStringLiteral("Summarize a resource"),
      std::vector<mcp::McpPromptArgument>{
          mcp::McpPromptArgument{QStringLiteral("uri"), QStringLiteral("Resource URI"), true},
          mcp::McpPromptArgument{QStringLiteral("style"), QStringLiteral("Summary style"), false},
      },
  };

  const auto object = mcp::to_json_object(prompt);
  REQUIRE(object.value(QStringLiteral("name")).toString() == QStringLiteral("summarize-resource"));
  REQUIRE(object.value(QStringLiteral("description")).toString() == QStringLiteral("Summarize a resource"));
  REQUIRE(object.value(QStringLiteral("arguments")).toArray().size() == 2);

  const auto parsed = mcp::prompt_from_json_object(object);
  REQUIRE(parsed.has_value());
  REQUIRE(*parsed == prompt);
}

TEST_CASE("prompts/list request is tracked and supports pagination cursor", "[mcp][prompts]")
{
  mcp::PendingRequestStore pending_requests;

  const auto request = mcp::make_prompts_list_request(pending_requests, QStringLiteral("cursor-1"));

  REQUIRE(request.id == mcp::RequestId::number(1));
  REQUIRE(request.method == QStringLiteral("prompts/list"));
  REQUIRE(request.params.has_value());
  REQUIRE(request.params->toObject().value(QStringLiteral("cursor")).toString() == QStringLiteral("cursor-1"));
  REQUIRE(pending_requests.contains(request.id));
}

TEST_CASE("prompts/list response parses prompts and next cursor", "[mcp][prompts]")
{
  mcp::JsonRpcResponse response;
  response.id = mcp::RequestId::number(1);
  response.result = QJsonValue{QJsonObject{
      {QStringLiteral("prompts"), QJsonArray{
          QJsonObject{
              {QStringLiteral("name"), QStringLiteral("summarize-resource")},
              {QStringLiteral("description"), QStringLiteral("Summarize a resource")},
              {QStringLiteral("arguments"), QJsonArray{
                  QJsonObject{
                      {QStringLiteral("name"), QStringLiteral("uri")},
                      {QStringLiteral("required"), true},
                  },
              }},
          },
      }},
      {QStringLiteral("nextCursor"), QStringLiteral("cursor-2")},
  }};

  const auto parsed = mcp::parse_prompts_list_response(response);
  const auto* result = std::get_if<mcp::McpPromptsListResult>(&parsed);

  REQUIRE(result != nullptr);
  REQUIRE(result->prompts.size() == 1);
  REQUIRE(result->prompts.front().name == QStringLiteral("summarize-resource"));
  REQUIRE(result->prompts.front().arguments.size() == 1);
  REQUIRE(result->prompts.front().arguments.front().required);
  REQUIRE(result->next_cursor == QStringLiteral("cursor-2"));
}

TEST_CASE("prompts/list validates malformed prompt arguments", "[mcp][prompts]")
{
  const auto parsed = mcp::parse_prompts_list_result(QJsonObject{
      {QStringLiteral("prompts"), QJsonArray{
          QJsonObject{
              {QStringLiteral("name"), QStringLiteral("broken")},
              {QStringLiteral("arguments"), QJsonArray{QStringLiteral("not-an-object")}},
          },
      }},
  });

  const auto* error = std::get_if<mcp::McpPromptParseError>(&parsed);
  REQUIRE(error != nullptr);
  REQUIRE(error->code == mcp::McpPromptParseErrorCode::InvalidPrompt);
}

TEST_CASE("prompts/get request serializes name and JSON arguments", "[mcp][prompts]")
{
  mcp::PendingRequestStore pending_requests;

  const auto request = mcp::make_prompts_get_request(
      pending_requests,
      QStringLiteral("summarize-resource"),
      QJsonObject{{QStringLiteral("uri"), QStringLiteral("demo://readme.md")}});

  REQUIRE(request.id == mcp::RequestId::number(1));
  REQUIRE(request.method == QStringLiteral("prompts/get"));
  REQUIRE(request.params.has_value());

  const auto params = request.params->toObject();
  REQUIRE(params.value(QStringLiteral("name")).toString() == QStringLiteral("summarize-resource"));
  REQUIRE(params.value(QStringLiteral("arguments")).toObject().value(QStringLiteral("uri")).toString() == QStringLiteral("demo://readme.md"));
  REQUIRE(pending_requests.contains(request.id));
}

TEST_CASE("prompts/get rejects blank prompt names", "[mcp][prompts]")
{
  mcp::PendingRequestStore pending_requests;

  REQUIRE_THROWS_AS(
      mcp::make_prompts_get_request(pending_requests, QStringLiteral("   "), QJsonObject{}),
      std::invalid_argument);
}

TEST_CASE("prompts/get response parses messages array", "[mcp][prompts]")
{
  mcp::JsonRpcResponse response;
  response.id = mcp::RequestId::number(1);
  response.result = QJsonValue{QJsonObject{
      {QStringLiteral("description"), QStringLiteral("Summary prompt")},
      {QStringLiteral("messages"), QJsonArray{
          QJsonObject{
              {QStringLiteral("role"), QStringLiteral("user")},
              {QStringLiteral("content"), QJsonObject{
                  {QStringLiteral("type"), QStringLiteral("text")},
                  {QStringLiteral("text"), QStringLiteral("Summarize this resource")},
              }},
          },
      }},
  }};

  const auto parsed = mcp::parse_prompts_get_response(response);
  const auto* result = std::get_if<mcp::McpPromptGetResult>(&parsed);

  REQUIRE(result != nullptr);
  REQUIRE(result->description == QStringLiteral("Summary prompt"));
  REQUIRE(result->messages.size() == 1);
  REQUIRE(result->messages.at(0).toObject().value(QStringLiteral("role")).toString() == QStringLiteral("user"));
  REQUIRE(result->raw_result.contains(QStringLiteral("messages")));
}
