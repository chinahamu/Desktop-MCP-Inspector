#include "SecurityRules.hpp"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonValue>
#include <QUrl>
#include <QUrlQuery>

#include <utility>

namespace security {
namespace {

[[nodiscard]] QString lower(QString value)
{
  return value.toLower().trimmed();
}

[[nodiscard]] QString executable_name(QString command)
{
  command.replace(QLatin1Char('\\'), QLatin1Char('/'));
  auto name = command.section(QLatin1Char('/'), -1).toLower().trimmed();
  if (name.endsWith(QStringLiteral(".exe"))) {
    name.chop(4);
  }
  return name;
}

[[nodiscard]] bool contains_any(const QString& haystack, const QStringList& needles)
{
  for (const auto& needle : needles) {
    if (haystack.contains(needle, Qt::CaseInsensitive)) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] RiskFinding make_finding(
    QString id,
    RiskSeverity severity,
    RiskCategory category,
    QString title,
    QString detail,
    QString recommendation,
    QString subject)
{
  return RiskFinding{
      std::move(id),
      severity,
      category,
      std::move(title),
      std::move(detail),
      std::move(recommendation),
      std::move(subject),
  };
}

[[nodiscard]] bool is_loopback_host(QString host)
{
  host = host.toLower().trimmed();
  return host == QStringLiteral("localhost") || host == QStringLiteral("127.0.0.1") || host == QStringLiteral("::1")
      || host.startsWith(QStringLiteral("127."));
}

[[nodiscard]] QString schema_type(const QJsonObject& schema)
{
  const auto type = schema.value(QStringLiteral("type"));
  if (type.isString()) {
    return type.toString().toLower();
  }
  if (type.isArray()) {
    const auto types = type.toArray();
    for (const auto& entry : types) {
      if (entry.isString()) {
        return entry.toString().toLower();
      }
    }
  }
  return {};
}

void scan_schema_object(
    const QString& tool_name,
    const QJsonObject& schema,
    const QString& path,
    std::vector<RiskFinding>& findings)
{
  const auto properties_value = schema.value(QStringLiteral("properties"));
  if (properties_value.isObject()) {
    const auto properties = properties_value.toObject();
    for (auto it = properties.begin(); it != properties.end(); ++it) {
      if (!it.value().isObject()) {
        continue;
      }

      const auto property_name = it.key();
      const auto property_schema = it.value().toObject();
      const auto property_path = path.isEmpty() ? property_name : QStringLiteral("%1.%2").arg(path, property_name);
      const auto name_lower = lower(property_name);
      const auto description_lower = lower(property_schema.value(QStringLiteral("description")).toString());
      const auto text = QStringLiteral("%1 %2").arg(name_lower, description_lower);
      const auto type = schema_type(property_schema);
      const auto subject = QStringLiteral("%1.%2").arg(tool_name, property_path);

      if (contains_any(text, QStringList{
              QStringLiteral("command"),
              QStringLiteral("cmd"),
              QStringLiteral("shell"),
              QStringLiteral("script"),
              QStringLiteral("executable"),
              QStringLiteral("process"),
              QStringLiteral("code"),
          })) {
        findings.push_back(make_finding(
            QStringLiteral("security.schema.command.%1").arg(static_cast<int>(findings.size() + 1)),
            RiskSeverity::High,
            RiskCategory::InputSchema,
            QStringLiteral("Tool schema accepts executable input"),
            QStringLiteral("Input field '%1' may allow users or model output to influence command execution.").arg(property_path),
            QStringLiteral("Require explicit user confirmation and validate this field with an allow-list before tool execution."),
            subject));
      }

      if (contains_any(text, QStringList{
              QStringLiteral("password"),
              QStringLiteral("secret"),
              QStringLiteral("token"),
              QStringLiteral("apikey"),
              QStringLiteral("api_key"),
              QStringLiteral("privatekey"),
              QStringLiteral("private_key"),
              QStringLiteral("credential"),
          })) {
        findings.push_back(make_finding(
            QStringLiteral("security.schema.secret.%1").arg(static_cast<int>(findings.size() + 1)),
            RiskSeverity::Medium,
            RiskCategory::InputSchema,
            QStringLiteral("Tool schema accepts secret-like input"),
            QStringLiteral("Input field '%1' appears to accept credentials or secret material.").arg(property_path),
            QStringLiteral("Mask this field in UI, avoid storing it, and pass it only through scoped runtime secrets."),
            subject));
      }

      if (type == QStringLiteral("string") && contains_any(text, QStringList{
              QStringLiteral("path"),
              QStringLiteral("file"),
              QStringLiteral("directory"),
              QStringLiteral("folder"),
              QStringLiteral("filename"),
          })) {
        findings.push_back(make_finding(
            QStringLiteral("security.schema.file.%1").arg(static_cast<int>(findings.size() + 1)),
            RiskSeverity::Medium,
            RiskCategory::InputSchema,
            QStringLiteral("Tool schema accepts file-system paths"),
            QStringLiteral("Input field '%1' may expose local file-system access.").arg(property_path),
            QStringLiteral("Constrain file access to a workspace root and show the resolved path before execution."),
            subject));
      }

      if (type == QStringLiteral("string") && contains_any(text, QStringList{
              QStringLiteral("url"),
              QStringLiteral("uri"),
              QStringLiteral("endpoint"),
              QStringLiteral("webhook"),
          })) {
        findings.push_back(make_finding(
            QStringLiteral("security.schema.url.%1").arg(static_cast<int>(findings.size() + 1)),
            RiskSeverity::Low,
            RiskCategory::InputSchema,
            QStringLiteral("Tool schema accepts remote endpoints"),
            QStringLiteral("Input field '%1' may cause network access to user supplied URLs.").arg(property_path),
            QStringLiteral("Validate allowed schemes and hosts before dispatching requests."),
            subject));
      }

      scan_schema_object(tool_name, property_schema, property_path, findings);
      const auto items_value = property_schema.value(QStringLiteral("items"));
      if (items_value.isObject()) {
        scan_schema_object(tool_name, items_value.toObject(), QStringLiteral("%1[]").arg(property_path), findings);
      }
    }
  }

  const auto additional_properties = schema.value(QStringLiteral("additionalProperties"));
  if (additional_properties.isBool() && additional_properties.toBool()) {
    findings.push_back(make_finding(
        QStringLiteral("security.schema.additionalProperties.%1").arg(static_cast<int>(findings.size() + 1)),
        RiskSeverity::Low,
        RiskCategory::InputSchema,
        QStringLiteral("Tool schema allows arbitrary object keys"),
        QStringLiteral("Schema path '%1' accepts additionalProperties=true, reducing argument validation strength.")
            .arg(path.isEmpty() ? QStringLiteral("<root>") : path),
        QStringLiteral("Prefer an explicit properties allow-list for tool arguments."),
        tool_name));
  }
}

} // namespace

std::vector<RiskFinding> evaluate_profile_command(const config::ServerProfile& profile)
{
  std::vector<RiskFinding> findings;
  if (!config::is_stdio_profile(profile) || profile.command.trimmed().isEmpty()) {
    return findings;
  }

  const auto command_name = executable_name(profile.command);
  const auto args_text = profile.args.join(QLatin1Char(' '));
  const auto args_lower = lower(args_text);
  const auto subject = profile.command;

  if (contains_any(command_name, QStringList{
          QStringLiteral("cmd"),
          QStringLiteral("powershell"),
          QStringLiteral("pwsh"),
          QStringLiteral("bash"),
          QStringLiteral("sh"),
          QStringLiteral("zsh"),
          QStringLiteral("fish"),
          QStringLiteral("wscript"),
          QStringLiteral("cscript"),
      })) {
    findings.push_back(make_finding(
        QStringLiteral("security.command.shell"),
        RiskSeverity::High,
        RiskCategory::ProfileCommand,
        QStringLiteral("Profile launches a shell interpreter"),
        QStringLiteral("Command '%1' starts a general-purpose shell, which increases command injection blast radius.").arg(profile.command),
        QStringLiteral("Prefer a dedicated MCP server executable and avoid shell wrappers where possible."),
        subject));
  }

  if (contains_any(command_name, QStringList{
          QStringLiteral("curl"),
          QStringLiteral("wget"),
          QStringLiteral("nc"),
          QStringLiteral("ncat"),
          QStringLiteral("socat"),
      })) {
    findings.push_back(make_finding(
        QStringLiteral("security.command.network-client"),
        RiskSeverity::Medium,
        RiskCategory::ProfileCommand,
        QStringLiteral("Profile launches a network utility"),
        QStringLiteral("Command '%1' can create arbitrary network connections.").arg(profile.command),
        QStringLiteral("Replace ad-hoc network clients with a reviewed MCP server and constrain target hosts."),
        subject));
  }

  if (contains_any(command_name, QStringList{
          QStringLiteral("python"),
          QStringLiteral("python3"),
          QStringLiteral("node"),
          QStringLiteral("ruby"),
          QStringLiteral("perl"),
          QStringLiteral("php"),
      }) && contains_any(args_lower, QStringList{QStringLiteral(" -c"), QStringLiteral(" -e"), QStringLiteral("--eval")})) {
    findings.push_back(make_finding(
        QStringLiteral("security.command.inline-code"),
        RiskSeverity::High,
        RiskCategory::ProfileCommand,
        QStringLiteral("Profile executes inline interpreter code"),
        QStringLiteral("Arguments appear to evaluate inline code instead of launching a fixed server module."),
        QStringLiteral("Move inline code into a version-controlled server file and review the file path."),
        subject));
  }

  if (contains_any(args_lower, QStringList{
          QStringLiteral("-encodedcommand"),
          QStringLiteral(" -enc"),
          QStringLiteral("frombase64string"),
      })) {
    findings.push_back(make_finding(
        QStringLiteral("security.command.encoded"),
        RiskSeverity::Critical,
        RiskCategory::ProfileCommand,
        QStringLiteral("Profile uses encoded command arguments"),
        QStringLiteral("Encoded command arguments hide the effective process behavior from review."),
        QStringLiteral("Remove encoded payloads and make the launched command auditable as plain text."),
        subject));
  }

  if (contains_any(args_lower, QStringList{
          QStringLiteral("rm -rf"),
          QStringLiteral("remove-item"),
          QStringLiteral("rmdir /s"),
          QStringLiteral("del /"),
          QStringLiteral("format "),
          QStringLiteral("chmod 777"),
      })) {
    findings.push_back(make_finding(
        QStringLiteral("security.command.destructive"),
        RiskSeverity::Critical,
        RiskCategory::ProfileCommand,
        QStringLiteral("Profile arguments include destructive operations"),
        QStringLiteral("Arguments include tokens commonly used for recursive deletion or destructive file operations."),
        QStringLiteral("Remove destructive operations from startup and require per-action confirmation inside the MCP server."),
        subject));
  }

  if (contains_any(args_text, QStringList{
          QStringLiteral("&&"),
          QStringLiteral("||"),
          QStringLiteral("|"),
          QStringLiteral(">$"),
          QStringLiteral("$("),
          QStringLiteral("`"),
      })) {
    findings.push_back(make_finding(
        QStringLiteral("security.command.shell-metachar"),
        RiskSeverity::High,
        RiskCategory::ProfileCommand,
        QStringLiteral("Profile arguments contain shell metacharacters"),
        QStringLiteral("Arguments contain shell control characters that can chain or redirect commands."),
        QStringLiteral("Pass arguments directly to the executable without shell expansion and escape user-controlled fragments."),
        subject));
  }

  return findings;
}

std::vector<RiskFinding> evaluate_profile_environment(const config::ServerProfile& profile)
{
  std::vector<RiskFinding> findings;
  for (auto it = profile.env.begin(); it != profile.env.end(); ++it) {
    const auto key = it.key();
    const auto value = it.value();
    const auto key_lower = lower(key);
    const auto value_lower = lower(value);

    if (contains_any(key_lower, QStringList{
            QStringLiteral("password"),
            QStringLiteral("passwd"),
            QStringLiteral("secret"),
            QStringLiteral("token"),
            QStringLiteral("api_key"),
            QStringLiteral("apikey"),
            QStringLiteral("private_key"),
            QStringLiteral("access_key"),
            QStringLiteral("credential"),
            QStringLiteral("bearer"),
            QStringLiteral("session"),
        })) {
      findings.push_back(make_finding(
          QStringLiteral("security.env.secret-key.%1").arg(key),
          value.trimmed().isEmpty() ? RiskSeverity::Medium : RiskSeverity::High,
          RiskCategory::ProfileEnvironment,
          QStringLiteral("Profile environment contains secret-like key"),
          QStringLiteral("Environment variable '%1' appears to hold secret material.").arg(key),
          QStringLiteral("Store this value in an OS secret store and keep it masked in UI and reports."),
          key));
      continue;
    }

    if (contains_any(value_lower, QStringList{
            QStringLiteral("sk-"),
            QStringLiteral("ghp_"),
            QStringLiteral("github_pat_"),
            QStringLiteral("xoxb-"),
            QStringLiteral("xoxp-"),
            QStringLiteral("-----begin private key-----"),
        })) {
      findings.push_back(make_finding(
          QStringLiteral("security.env.secret-value.%1").arg(key),
          RiskSeverity::Medium,
          RiskCategory::ProfileEnvironment,
          QStringLiteral("Profile environment value looks like a credential"),
          QStringLiteral("Environment variable '%1' has a value pattern commonly used by credentials.").arg(key),
          QStringLiteral("Rotate exposed credentials if this profile was shared and move the value to a secret store."),
          key));
    }
  }
  return findings;
}

std::vector<RiskFinding> evaluate_http_endpoint(const config::ServerProfile& profile)
{
  std::vector<RiskFinding> findings;
  if (!config::is_streamable_http_profile(profile) || profile.endpoint_url.trimmed().isEmpty()) {
    return findings;
  }

  const QUrl url{profile.endpoint_url.trimmed()};
  if (!url.isValid() || url.scheme().isEmpty()) {
    findings.push_back(make_finding(
        QStringLiteral("security.http.invalid-url"),
        RiskSeverity::High,
        RiskCategory::HttpEndpoint,
        QStringLiteral("HTTP profile endpoint is not a valid absolute URL"),
        QStringLiteral("Endpoint '%1' cannot be parsed as an absolute URL.").arg(profile.endpoint_url),
        QStringLiteral("Use a valid http:// or https:// endpoint and validate it before connection."),
        profile.endpoint_url));
    return findings;
  }

  const auto scheme = url.scheme().toLower();
  const auto host = url.host();
  if (scheme != QStringLiteral("http") && scheme != QStringLiteral("https")) {
    findings.push_back(make_finding(
        QStringLiteral("security.http.unsupported-scheme"),
        RiskSeverity::High,
        RiskCategory::HttpEndpoint,
        QStringLiteral("HTTP profile uses unsupported URL scheme"),
        QStringLiteral("Endpoint scheme '%1' is not supported by the streamable HTTP transport.").arg(url.scheme()),
        QStringLiteral("Use https:// for remote MCP endpoints and http:// only for trusted loopback development."),
        profile.endpoint_url));
  }

  if (scheme == QStringLiteral("http")) {
    findings.push_back(make_finding(
        QStringLiteral("security.http.cleartext"),
        is_loopback_host(host) ? RiskSeverity::Low : RiskSeverity::High,
        RiskCategory::HttpEndpoint,
        QStringLiteral("HTTP profile uses cleartext transport"),
        QStringLiteral("Endpoint '%1' does not use TLS.").arg(profile.endpoint_url),
        QStringLiteral("Use https:// for remote endpoints; keep http:// limited to loopback development servers."),
        profile.endpoint_url));
  }

  if (!url.userName().isEmpty() || !url.password().isEmpty()) {
    findings.push_back(make_finding(
        QStringLiteral("security.http.embedded-credentials"),
        RiskSeverity::Critical,
        RiskCategory::HttpEndpoint,
        QStringLiteral("HTTP profile embeds credentials in the URL"),
        QStringLiteral("Endpoint URL contains username or password components."),
        QStringLiteral("Move credentials to scoped headers or an OS secret store and never persist them in endpoint URLs."),
        profile.endpoint_url));
  }

  const QUrlQuery query{url};
  for (const auto& item : query.queryItems()) {
    if (contains_any(lower(item.first), QStringList{
            QStringLiteral("token"),
            QStringLiteral("secret"),
            QStringLiteral("key"),
            QStringLiteral("auth"),
            QStringLiteral("password"),
        })) {
      findings.push_back(make_finding(
          QStringLiteral("security.http.query-secret.%1").arg(item.first),
          RiskSeverity::High,
          RiskCategory::HttpEndpoint,
          QStringLiteral("HTTP profile carries secret-like query parameters"),
          QStringLiteral("Query parameter '%1' appears to contain authentication material.").arg(item.first),
          QStringLiteral("Use authorization headers or a secret store instead of query-string credentials."),
          profile.endpoint_url));
    }
  }

  return findings;
}

std::vector<RiskFinding> evaluate_tool_metadata(const mcp::McpTool& tool)
{
  std::vector<RiskFinding> findings;
  const auto description = tool.description.value_or(QString{});
  const auto text = lower(QStringLiteral("%1 %2").arg(tool.name, description));

  if (contains_any(text, QStringList{
          QStringLiteral("shell"),
          QStringLiteral("exec"),
          QStringLiteral("command"),
          QStringLiteral("terminal"),
          QStringLiteral("powershell"),
          QStringLiteral("subprocess"),
          QStringLiteral("run script"),
      })) {
    findings.push_back(make_finding(
        QStringLiteral("security.tool.execution.%1").arg(tool.name),
        RiskSeverity::High,
        RiskCategory::ToolMetadata,
        QStringLiteral("Tool appears to execute commands"),
        QStringLiteral("Tool '%1' metadata indicates command or process execution capability.").arg(tool.name),
        QStringLiteral("Require explicit confirmation, display the resolved command, and restrict allowed commands."),
        tool.name));
  }

  if (contains_any(text, QStringList{
          QStringLiteral("delete"),
          QStringLiteral("remove"),
          QStringLiteral("wipe"),
          QStringLiteral("format"),
          QStringLiteral("drop table"),
          QStringLiteral("rm -rf"),
      })) {
    findings.push_back(make_finding(
        QStringLiteral("security.tool.destructive.%1").arg(tool.name),
        RiskSeverity::High,
        RiskCategory::ToolMetadata,
        QStringLiteral("Tool metadata suggests destructive behavior"),
        QStringLiteral("Tool '%1' may modify or delete data based on its name or description.").arg(tool.name),
        QStringLiteral("Gate destructive tools behind confirmation and provide a dry-run mode where possible."),
        tool.name));
  }

  if (contains_any(text, QStringList{
          QStringLiteral("file"),
          QStringLiteral("filesystem"),
          QStringLiteral("directory"),
          QStringLiteral("read_file"),
          QStringLiteral("write_file"),
      })) {
    findings.push_back(make_finding(
        QStringLiteral("security.tool.filesystem.%1").arg(tool.name),
        RiskSeverity::Medium,
        RiskCategory::ToolMetadata,
        QStringLiteral("Tool appears to access the file system"),
        QStringLiteral("Tool '%1' metadata indicates file-system access.").arg(tool.name),
        QStringLiteral("Limit file access to a workspace root and show path previews before execution."),
        tool.name));
  }

  return findings;
}

std::vector<RiskFinding> evaluate_input_schema(const mcp::McpTool& tool)
{
  std::vector<RiskFinding> findings;
  scan_schema_object(tool.name, tool.input_schema, QString{}, findings);
  return findings;
}

std::vector<RiskFinding> evaluate_resource_exposure(const mcp::McpResource& resource)
{
  std::vector<RiskFinding> findings;
  const auto uri_lower = lower(resource.uri);
  const auto metadata = lower(QStringLiteral("%1 %2 %3")
                                  .arg(resource.name, resource.title.value_or(QString{}), resource.description.value_or(QString{})));

  if (uri_lower.startsWith(QStringLiteral("file://")) || uri_lower.startsWith(QLatin1Char('/'))
      || uri_lower.contains(QStringLiteral(":\\"))) {
    findings.push_back(make_finding(
        QStringLiteral("security.resource.local-file.%1").arg(resource.name),
        RiskSeverity::High,
        RiskCategory::ResourceExposure,
        QStringLiteral("Resource exposes a local file path"),
        QStringLiteral("Resource '%1' points to a local file-system location.").arg(resource.name),
        QStringLiteral("Expose only scoped workspace resources and avoid publishing absolute local paths."),
        resource.uri));
  }

  if (contains_any(uri_lower + QLatin1Char(' ') + metadata, QStringList{
          QStringLiteral(".env"),
          QStringLiteral(".ssh"),
          QStringLiteral("id_rsa"),
          QStringLiteral("private_key"),
          QStringLiteral("secret"),
          QStringLiteral("token"),
          QStringLiteral("password"),
          QStringLiteral("credential"),
      })) {
    findings.push_back(make_finding(
        QStringLiteral("security.resource.secret.%1").arg(resource.name),
        RiskSeverity::Critical,
        RiskCategory::ResourceExposure,
        QStringLiteral("Resource appears to expose secret material"),
        QStringLiteral("Resource '%1' metadata or URI resembles credentials or private configuration.").arg(resource.name),
        QStringLiteral("Remove secret resources from MCP exposure and rotate any credentials that may have been served."),
        resource.uri));
  }

  if (resource.size.has_value() && *resource.size > 10 * 1024 * 1024) {
    findings.push_back(make_finding(
        QStringLiteral("security.resource.large.%1").arg(resource.name),
        RiskSeverity::Low,
        RiskCategory::ResourceExposure,
        QStringLiteral("Resource is large enough to leak substantial data"),
        QStringLiteral("Resource '%1' is larger than 10 MiB.").arg(resource.name),
        QStringLiteral("Require user confirmation before reading or exporting large resources."),
        resource.uri));
  }

  return findings;
}

std::vector<RuleDefinition> command_argument_rules()
{
  return {RuleDefinition{
      QStringLiteral("security.command.args"),
      QStringLiteral("Command and argument risk checks"),
      RiskCategory::ProfileCommand,
      [](const RuleInput& input) {
        return input.profile.has_value() ? evaluate_profile_command(*input.profile) : std::vector<RiskFinding>{};
      },
  }};
}

std::vector<RuleDefinition> environment_secret_rules()
{
  return {RuleDefinition{
      QStringLiteral("security.env.secrets"),
      QStringLiteral("Environment secret detection"),
      RiskCategory::ProfileEnvironment,
      [](const RuleInput& input) {
        return input.profile.has_value() ? evaluate_profile_environment(*input.profile) : std::vector<RiskFinding>{};
      },
  }};
}

std::vector<RuleDefinition> http_endpoint_rules()
{
  return {RuleDefinition{
      QStringLiteral("security.http.endpoint"),
      QStringLiteral("HTTP endpoint diagnostics"),
      RiskCategory::HttpEndpoint,
      [](const RuleInput& input) {
        return input.profile.has_value() ? evaluate_http_endpoint(*input.profile) : std::vector<RiskFinding>{};
      },
  }};
}

std::vector<RuleDefinition> tool_metadata_rules()
{
  return {RuleDefinition{
      QStringLiteral("security.tool.metadata"),
      QStringLiteral("Tool metadata dangerous keyword detection"),
      RiskCategory::ToolMetadata,
      [](const RuleInput& input) {
        std::vector<RiskFinding> findings;
        for (const auto& tool : input.tools) {
          auto tool_findings = evaluate_tool_metadata(tool);
          findings.insert(
              findings.end(),
              std::make_move_iterator(tool_findings.begin()),
              std::make_move_iterator(tool_findings.end()));
        }
        return findings;
      },
  }};
}

std::vector<RuleDefinition> input_schema_rules()
{
  return {RuleDefinition{
      QStringLiteral("security.tool.input-schema"),
      QStringLiteral("Tool input schema risk detection"),
      RiskCategory::InputSchema,
      [](const RuleInput& input) {
        std::vector<RiskFinding> findings;
        for (const auto& tool : input.tools) {
          auto tool_findings = evaluate_input_schema(tool);
          findings.insert(
              findings.end(),
              std::make_move_iterator(tool_findings.begin()),
              std::make_move_iterator(tool_findings.end()));
        }
        return findings;
      },
  }};
}

std::vector<RuleDefinition> resource_exposure_rules()
{
  return {RuleDefinition{
      QStringLiteral("security.resource.exposure"),
      QStringLiteral("Resource exposure diagnostics"),
      RiskCategory::ResourceExposure,
      [](const RuleInput& input) {
        std::vector<RiskFinding> findings;
        for (const auto& resource : input.resources) {
          auto resource_findings = evaluate_resource_exposure(resource);
          findings.insert(
              findings.end(),
              std::make_move_iterator(resource_findings.begin()),
              std::make_move_iterator(resource_findings.end()));
        }
        return findings;
      },
  }};
}

std::vector<RuleDefinition> default_security_rules()
{
  std::vector<RuleDefinition> rules;
  auto append = [&rules](std::vector<RuleDefinition> part) {
    rules.insert(rules.end(), std::make_move_iterator(part.begin()), std::make_move_iterator(part.end()));
  };

  append(command_argument_rules());
  append(environment_secret_rules());
  append(http_endpoint_rules());
  append(tool_metadata_rules());
  append(input_schema_rules());
  append(resource_exposure_rules());
  return rules;
}

} // namespace security
