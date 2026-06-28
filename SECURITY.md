# Security Policy

Desktop MCP Inspector is intended to inspect and debug MCP servers that may expose powerful local or remote capabilities. Security reports are welcome and should be handled carefully.

## Supported versions

The project is currently in the planning / initial implementation stage and has not published a stable release.

| Version | Supported |
| --- | --- |
| unreleased `main` | Best effort |
| released versions | Not available yet |

## Reporting a vulnerability

Please do not open a public issue for a suspected vulnerability.

Use GitHub's private vulnerability reporting feature if it is enabled for this repository. If private reporting is not available, contact the maintainer privately and include enough information to reproduce the issue safely.

A useful report includes:

- A clear description of the issue
- Affected commit, branch, or release
- Reproduction steps
- Expected vs. actual behavior
- Impact assessment
- Whether secrets, local files, command execution, or remote endpoints are involved
- Suggested fix, if known

## Security areas of interest

Reports are especially useful for issues involving:

- Unsafe command execution or argument handling
- Accidental execution of untrusted MCP server definitions
- Exposure of environment variables or secrets
- Unsafe persistence of profiles, logs, or tool call payloads
- Inadequate masking of tokens, keys, passwords, or credentials
- Incorrect handling of paths, working directories, or file URLs
- Remote endpoint trust issues for Streamable HTTP transport
- Cross-platform quoting issues on Windows, macOS, or Linux
- Log injection or report injection through server-controlled output

## Safe testing expectations

When testing this project:

- Use disposable MCP servers and test directories.
- Do not pass production credentials through environment variables.
- Avoid testing destructive tools against real user data.
- Prefer temporary workspaces for file-system MCP servers.
- Clearly mark proof-of-concept payloads as non-production.

## Disclosure process

The maintainer will make a best-effort attempt to:

1. Confirm receipt of the report.
2. Reproduce and assess the impact.
3. Prepare a fix or mitigation.
4. Credit the reporter if desired.
5. Publish a security note if the issue affects released users.

Because the project is not yet released, fixes may initially land directly as normal pull requests without a formal advisory unless the issue affects published artifacts.

## Out of scope

The following are usually out of scope unless they demonstrate a concrete vulnerability in this project:

- Vulnerabilities in third-party MCP servers under inspection
- General risks caused by intentionally running untrusted commands outside this application
- Issues requiring local administrator/root access without additional impact
- Denial-of-service cases that only affect intentionally huge local test inputs during development
