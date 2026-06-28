#include "ServerDiff.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMap>
#include <QSet>
#include <QStringList>

#include <algorithm>
#include <optional>
#include <utility>

namespace compare {
namespace {

[[nodiscard]] QString compact_json(const QJsonValue& value)
{
  if (value.isObject()) {
    return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
  }
  if (value.isArray()) {
    return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
  }
  if (value.isString()) {
    return value.toString();
  }
  if (value.isBool()) {
    return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
  }
  if (value.isDouble()) {
    return QString::number(value.toDouble(), 'g', 15);
  }
  if (value.isNull()) {
    return QStringLiteral("null");
  }
  return QStringLiteral("undefined");
}

[[nodiscard]] QString optional_string(const std::optional<QString>& value)
{
  return value.value_or(QString{});
}

[[nodiscard]] QSet<QString> key_set(const QJsonObject& object)
{
  QSet<QString> keys;
  for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
    keys.insert(it.key());
  }
  return keys;
}

template <typename T>
[[nodiscard]] QSet<QString> key_set(const QMap<QString, T>& map)
{
  QSet<QString> keys;
  for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
    keys.insert(it.key());
  }
  return keys;
}

[[nodiscard]] QSet<QString> unite_keys(QSet<QString> lhs, const QSet<QString>& rhs)
{
  lhs.unite(rhs);
  return lhs;
}

[[nodiscard]] QSet<QString> required_fields(const QJsonObject& schema)
{
  QSet<QString> fields;
  const auto required = schema.value(QStringLiteral("required"));
  if (!required.isArray()) {
    return fields;
  }

  const auto array = required.toArray();
  for (const auto& value : array) {
    if (value.isString()) {
      fields.insert(value.toString());
    }
  }
  return fields;
}

[[nodiscard]] QJsonObject schema_properties(const QJsonObject& schema)
{
  const auto properties = schema.value(QStringLiteral("properties"));
  return properties.isObject() ? properties.toObject() : QJsonObject{};
}

[[nodiscard]] QString schema_type(const QJsonObject& property_schema)
{
  const auto type = property_schema.value(QStringLiteral("type"));
  if (type.isString()) {
    return type.toString();
  }
  if (type.isArray()) {
    QStringList values;
    const auto array = type.toArray();
    for (const auto& value : array) {
      values.push_back(compact_json(value));
    }
    values.sort();
    return values.join(QStringLiteral("|"));
  }
  return QString{};
}

[[nodiscard]] bool schema_change_is_breaking(
    const QJsonObject& before,
    const QJsonObject& after,
    QString* reason)
{
  const auto before_required = required_fields(before);
  const auto after_required = required_fields(after);
  for (const auto& name : after_required) {
    if (!before_required.contains(name)) {
      if (reason != nullptr) {
        *reason = QStringLiteral("New required input field: %1").arg(name);
      }
      return true;
    }
  }

  const auto before_properties = schema_properties(before);
  const auto after_properties = schema_properties(after);
  for (auto it = before_properties.constBegin(); it != before_properties.constEnd(); ++it) {
    const auto name = it.key();
    if (!after_properties.contains(name)) {
      if (reason != nullptr) {
        *reason = QStringLiteral("Removed input property: %1").arg(name);
      }
      return true;
    }

    const auto before_property = it.value().toObject();
    const auto after_property = after_properties.value(name).toObject();
    const auto before_type = schema_type(before_property);
    const auto after_type = schema_type(after_property);
    if (!before_type.isEmpty() && !after_type.isEmpty() && before_type != after_type) {
      if (reason != nullptr) {
        *reason = QStringLiteral("Input property type changed: %1 (%2 → %3)")
            .arg(name, before_type, after_type);
      }
      return true;
    }
  }

  return false;
}

[[nodiscard]] bool resource_metadata_changed(const mcp::McpResource& before, const mcp::McpResource& after)
{
  return before.name != after.name
      || before.title != after.title
      || before.description != after.description
      || before.mime_type != after.mime_type
      || before.size != after.size
      || before.annotations != after.annotations;
}

[[nodiscard]] QString resource_summary(const mcp::McpResource& resource)
{
  QStringList fields;
  fields << QStringLiteral("name=%1").arg(resource.name);
  if (resource.title.has_value()) {
    fields << QStringLiteral("title=%1").arg(*resource.title);
  }
  if (resource.description.has_value()) {
    fields << QStringLiteral("description=%1").arg(*resource.description);
  }
  if (resource.mime_type.has_value()) {
    fields << QStringLiteral("mimeType=%1").arg(*resource.mime_type);
  }
  if (resource.size.has_value()) {
    fields << QStringLiteral("size=%1").arg(*resource.size);
  }
  if (resource.annotations.has_value()) {
    fields << QStringLiteral("annotations=%1").arg(compact_json(QJsonValue(*resource.annotations)));
  }
  return fields.join(QStringLiteral("; "));
}

[[nodiscard]] QString prompt_summary(const mcp::McpPrompt& prompt)
{
  QStringList fields;
  if (prompt.description.has_value()) {
    fields << QStringLiteral("description=%1").arg(*prompt.description);
  }

  QStringList arguments;
  for (const auto& argument : prompt.arguments) {
    arguments << QStringLiteral("%1%2")
        .arg(argument.name, argument.required ? QStringLiteral(" (required)") : QStringLiteral(""));
  }
  fields << QStringLiteral("arguments=[%1]").arg(arguments.join(QStringLiteral(", ")));
  return fields.join(QStringLiteral("; "));
}

[[nodiscard]] QMap<QString, mcp::McpTool> index_tools(const std::vector<mcp::McpTool>& tools)
{
  QMap<QString, mcp::McpTool> indexed;
  for (const auto& tool : tools) {
    indexed.insert(tool.name, tool);
  }
  return indexed;
}

[[nodiscard]] QMap<QString, mcp::McpResource> index_resources(const std::vector<mcp::McpResource>& resources)
{
  QMap<QString, mcp::McpResource> indexed;
  for (const auto& resource : resources) {
    const auto key = !resource.uri.isEmpty() ? resource.uri : resource.name;
    indexed.insert(key, resource);
  }
  return indexed;
}

[[nodiscard]] QMap<QString, mcp::McpPrompt> index_prompts(const std::vector<mcp::McpPrompt>& prompts)
{
  QMap<QString, mcp::McpPrompt> indexed;
  for (const auto& prompt : prompts) {
    indexed.insert(prompt.name, prompt);
  }
  return indexed;
}

[[nodiscard]] QMap<QString, mcp::McpPromptArgument> index_prompt_arguments(const mcp::McpPrompt& prompt)
{
  QMap<QString, mcp::McpPromptArgument> indexed;
  for (const auto& argument : prompt.arguments) {
    indexed.insert(argument.name, argument);
  }
  return indexed;
}

[[nodiscard]] bool prompt_change_is_breaking(const mcp::McpPrompt& before, const mcp::McpPrompt& after, QString* reason)
{
  const auto before_arguments = index_prompt_arguments(before);
  const auto after_arguments = index_prompt_arguments(after);

  for (auto it = before_arguments.constBegin(); it != before_arguments.constEnd(); ++it) {
    const auto name = it.key();
    if (!after_arguments.contains(name) && it.value().required) {
      if (reason != nullptr) {
        *reason = QStringLiteral("Removed required prompt argument: %1").arg(name);
      }
      return true;
    }
  }

  for (auto it = after_arguments.constBegin(); it != after_arguments.constEnd(); ++it) {
    const auto name = it.key();
    const auto after_argument = it.value();
    if (!before_arguments.contains(name)) {
      if (after_argument.required) {
        if (reason != nullptr) {
          *reason = QStringLiteral("New required prompt argument: %1").arg(name);
        }
        return true;
      }
      continue;
    }

    const auto before_argument = before_arguments.value(name);
    if (!before_argument.required && after_argument.required) {
      if (reason != nullptr) {
        *reason = QStringLiteral("Prompt argument became required: %1").arg(name);
      }
      return true;
    }
  }

  return false;
}

void append_entry(std::vector<DiffEntry>& entries, DiffEntry entry)
{
  entries.push_back(std::move(entry));
}

void sort_entries(std::vector<DiffEntry>& entries)
{
  std::sort(entries.begin(), entries.end(), [](const DiffEntry& lhs, const DiffEntry& rhs) {
    if (lhs.section != rhs.section) {
      return static_cast<int>(lhs.section) < static_cast<int>(rhs.section);
    }
    if (lhs.identifier != rhs.identifier) {
      return lhs.identifier < rhs.identifier;
    }
    return lhs.title < rhs.title;
  });
}

[[nodiscard]] ServerDiffSummary summarize(const std::vector<DiffEntry>& entries)
{
  ServerDiffSummary summary;
  for (const auto& entry : entries) {
    switch (entry.change) {
      case DiffChangeType::Added:
        ++summary.added;
        break;
      case DiffChangeType::Removed:
        ++summary.removed;
        break;
      case DiffChangeType::Modified:
        ++summary.modified;
        break;
    }

    switch (entry.breaking) {
      case DiffBreakingLevel::None:
        break;
      case DiffBreakingLevel::Potential:
        ++summary.potential;
        break;
      case DiffBreakingLevel::Breaking:
        ++summary.breaking;
        break;
    }
  }
  return summary;
}

} // namespace

qsizetype ServerDiffSummary::total() const
{
  return added + removed + modified;
}

bool ServerDiffSummary::has_breaking_changes() const
{
  return breaking > 0;
}

bool ServerDiffResult::has_changes() const
{
  return !entries.empty();
}

bool ServerDiffResult::has_breaking_changes() const
{
  return summary.has_breaking_changes();
}

QString to_string(DiffSection section)
{
  switch (section) {
    case DiffSection::Capability:
      return QStringLiteral("Capability");
    case DiffSection::Tool:
      return QStringLiteral("Tool");
    case DiffSection::InputSchema:
      return QStringLiteral("Input schema");
    case DiffSection::Resource:
      return QStringLiteral("Resource");
    case DiffSection::Prompt:
      return QStringLiteral("Prompt");
  }
  return QStringLiteral("Unknown");
}

QString to_string(DiffChangeType change)
{
  switch (change) {
    case DiffChangeType::Added:
      return QStringLiteral("added");
    case DiffChangeType::Removed:
      return QStringLiteral("removed");
    case DiffChangeType::Modified:
      return QStringLiteral("modified");
  }
  return QStringLiteral("unknown");
}

QString to_string(DiffBreakingLevel breaking)
{
  switch (breaking) {
    case DiffBreakingLevel::None:
      return QStringLiteral("none");
    case DiffBreakingLevel::Potential:
      return QStringLiteral("potential");
    case DiffBreakingLevel::Breaking:
      return QStringLiteral("breaking");
  }
  return QStringLiteral("unknown");
}

std::vector<DiffEntry> diff_capabilities(const QJsonObject& before, const QJsonObject& after)
{
  std::vector<DiffEntry> entries;
  const auto before_keys = key_set(before);
  const auto after_keys = key_set(after);
  const auto all_keys = unite_keys(before_keys, after_keys);

  for (const auto& key : all_keys) {
    const auto before_has_key = before.contains(key);
    const auto after_has_key = after.contains(key);
    if (before_has_key && !after_has_key) {
      append_entry(entries, DiffEntry{
          DiffSection::Capability,
          DiffChangeType::Removed,
          DiffBreakingLevel::Breaking,
          key,
          QStringLiteral("Capability removed"),
          QStringLiteral("A server capability disappeared from the initialize result."),
          compact_json(before.value(key)),
          {},
      });
      continue;
    }
    if (!before_has_key && after_has_key) {
      append_entry(entries, DiffEntry{
          DiffSection::Capability,
          DiffChangeType::Added,
          DiffBreakingLevel::None,
          key,
          QStringLiteral("Capability added"),
          QStringLiteral("A server capability was newly advertised."),
          {},
          compact_json(after.value(key)),
      });
      continue;
    }

    if (before.value(key) != after.value(key)) {
      append_entry(entries, DiffEntry{
          DiffSection::Capability,
          DiffChangeType::Modified,
          DiffBreakingLevel::Potential,
          key,
          QStringLiteral("Capability changed"),
          QStringLiteral("A server capability changed its advertised value."),
          compact_json(before.value(key)),
          compact_json(after.value(key)),
      });
    }
  }

  sort_entries(entries);
  return entries;
}

std::vector<DiffEntry> diff_tools(const std::vector<mcp::McpTool>& before, const std::vector<mcp::McpTool>& after)
{
  std::vector<DiffEntry> entries;
  const auto before_tools = index_tools(before);
  const auto after_tools = index_tools(after);
  const auto all_names = unite_keys(key_set(before_tools), key_set(after_tools));

  for (const auto& name : all_names) {
    const auto before_has_tool = before_tools.contains(name);
    const auto after_has_tool = after_tools.contains(name);
    if (before_has_tool && !after_has_tool) {
      append_entry(entries, DiffEntry{
          DiffSection::Tool,
          DiffChangeType::Removed,
          DiffBreakingLevel::Breaking,
          name,
          QStringLiteral("Tool removed"),
          QStringLiteral("Clients that call this tool will fail unless they are updated."),
          optional_string(before_tools.value(name).description),
          {},
      });
      continue;
    }
    if (!before_has_tool && after_has_tool) {
      append_entry(entries, DiffEntry{
          DiffSection::Tool,
          DiffChangeType::Added,
          DiffBreakingLevel::None,
          name,
          QStringLiteral("Tool added"),
          QStringLiteral("A new callable tool is available."),
          {},
          optional_string(after_tools.value(name).description),
      });
      continue;
    }

    const auto before_tool = before_tools.value(name);
    const auto after_tool = after_tools.value(name);
    if (before_tool.description != after_tool.description) {
      append_entry(entries, DiffEntry{
          DiffSection::Tool,
          DiffChangeType::Modified,
          DiffBreakingLevel::Potential,
          name,
          QStringLiteral("Tool metadata changed"),
          QStringLiteral("The tool description changed. Review prompt and documentation expectations."),
          optional_string(before_tool.description),
          optional_string(after_tool.description),
      });
    }

    if (before_tool.input_schema != after_tool.input_schema) {
      QString reason;
      const auto breaking = schema_change_is_breaking(before_tool.input_schema, after_tool.input_schema, &reason)
          ? DiffBreakingLevel::Breaking
          : DiffBreakingLevel::Potential;
      append_entry(entries, DiffEntry{
          DiffSection::InputSchema,
          DiffChangeType::Modified,
          breaking,
          name,
          QStringLiteral("Input schema changed"),
          reason.isEmpty() ? QStringLiteral("The tool input schema changed. Validate recorded calls and saved arguments.")
                           : reason,
          compact_json(QJsonValue(before_tool.input_schema)),
          compact_json(QJsonValue(after_tool.input_schema)),
      });
    }
  }

  sort_entries(entries);
  return entries;
}

std::vector<DiffEntry> diff_resources(
    const std::vector<mcp::McpResource>& before,
    const std::vector<mcp::McpResource>& after)
{
  std::vector<DiffEntry> entries;
  const auto before_resources = index_resources(before);
  const auto after_resources = index_resources(after);
  const auto all_uris = unite_keys(key_set(before_resources), key_set(after_resources));

  for (const auto& uri : all_uris) {
    const auto before_has_resource = before_resources.contains(uri);
    const auto after_has_resource = after_resources.contains(uri);
    if (before_has_resource && !after_has_resource) {
      append_entry(entries, DiffEntry{
          DiffSection::Resource,
          DiffChangeType::Removed,
          DiffBreakingLevel::Breaking,
          uri,
          QStringLiteral("Resource removed"),
          QStringLiteral("Saved resource references or prompts may fail because this URI is no longer advertised."),
          resource_summary(before_resources.value(uri)),
          {},
      });
      continue;
    }
    if (!before_has_resource && after_has_resource) {
      append_entry(entries, DiffEntry{
          DiffSection::Resource,
          DiffChangeType::Added,
          DiffBreakingLevel::None,
          uri,
          QStringLiteral("Resource added"),
          QStringLiteral("A new MCP resource is available."),
          {},
          resource_summary(after_resources.value(uri)),
      });
      continue;
    }

    const auto before_resource = before_resources.value(uri);
    const auto after_resource = after_resources.value(uri);
    if (resource_metadata_changed(before_resource, after_resource)) {
      append_entry(entries, DiffEntry{
          DiffSection::Resource,
          DiffChangeType::Modified,
          DiffBreakingLevel::Potential,
          uri,
          QStringLiteral("Resource metadata changed"),
          QStringLiteral("Resource metadata changed. Verify MIME type, size, annotations, and user-facing labels."),
          resource_summary(before_resource),
          resource_summary(after_resource),
      });
    }
  }

  sort_entries(entries);
  return entries;
}

std::vector<DiffEntry> diff_prompts(
    const std::vector<mcp::McpPrompt>& before,
    const std::vector<mcp::McpPrompt>& after)
{
  std::vector<DiffEntry> entries;
  const auto before_prompts = index_prompts(before);
  const auto after_prompts = index_prompts(after);
  const auto all_names = unite_keys(key_set(before_prompts), key_set(after_prompts));

  for (const auto& name : all_names) {
    const auto before_has_prompt = before_prompts.contains(name);
    const auto after_has_prompt = after_prompts.contains(name);
    if (before_has_prompt && !after_has_prompt) {
      append_entry(entries, DiffEntry{
          DiffSection::Prompt,
          DiffChangeType::Removed,
          DiffBreakingLevel::Breaking,
          name,
          QStringLiteral("Prompt removed"),
          QStringLiteral("Clients that reference this prompt will fail unless they are updated."),
          prompt_summary(before_prompts.value(name)),
          {},
      });
      continue;
    }
    if (!before_has_prompt && after_has_prompt) {
      append_entry(entries, DiffEntry{
          DiffSection::Prompt,
          DiffChangeType::Added,
          DiffBreakingLevel::None,
          name,
          QStringLiteral("Prompt added"),
          QStringLiteral("A new prompt template is available."),
          {},
          prompt_summary(after_prompts.value(name)),
      });
      continue;
    }

    const auto before_prompt = before_prompts.value(name);
    const auto after_prompt = after_prompts.value(name);
    if (before_prompt != after_prompt) {
      QString reason;
      const auto breaking = prompt_change_is_breaking(before_prompt, after_prompt, &reason)
          ? DiffBreakingLevel::Breaking
          : DiffBreakingLevel::Potential;
      append_entry(entries, DiffEntry{
          DiffSection::Prompt,
          DiffChangeType::Modified,
          breaking,
          name,
          QStringLiteral("Prompt metadata changed"),
          reason.isEmpty() ? QStringLiteral("Prompt description or argument metadata changed.")
                           : reason,
          prompt_summary(before_prompt),
          prompt_summary(after_prompt),
      });
    }
  }

  sort_entries(entries);
  return entries;
}

ServerDiffResult diff_server_snapshots(const ServerSnapshot& before, const ServerSnapshot& after)
{
  ServerDiffResult result;
  result.before_name = before.name;
  result.after_name = after.name;

  auto append_all = [&result](std::vector<DiffEntry> entries) {
    result.entries.insert(result.entries.end(), entries.begin(), entries.end());
  };

  append_all(diff_capabilities(before.capabilities, after.capabilities));
  append_all(diff_tools(before.tools, after.tools));
  append_all(diff_resources(before.resources, after.resources));
  append_all(diff_prompts(before.prompts, after.prompts));
  sort_entries(result.entries);
  result.summary = summarize(result.entries);
  return result;
}

} // namespace compare
