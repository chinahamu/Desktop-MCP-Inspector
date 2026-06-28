# Roadmap

This roadmap summarizes the planned development path for Desktop MCP Inspector. The detailed task checklist remains in `plan/07_task_list.md`; this document gives contributors a shorter product-level view.

## Current status

Desktop MCP Inspector is in early implementation. Phase 0 foundation work is complete enough to support regular development:

- CMake, Qt 6, and C++20 project setup
- minimal Qt `MainWindow` shell
- module directory layout
- Catch2 / CTest test foundation
- GitHub Actions build/test workflow
- developer setup and code style documentation
- centralized application metadata and settings initialization

## v0.1.0 goal

The first usable release should let a developer connect to a local stdio MCP server, inspect the handshake and tools, execute a tool call, and review raw JSON-RPC traffic and stderr logs.

v0.1.0 is complete when:

- the desktop app launches on Windows
- a stdio MCP server can be configured and started
- `initialize` and `initialized` complete successfully
- `tools/list` results are displayed
- `tools/call` can be executed with JSON input
- request, response, notification, error, and stderr events appear in a timeline
- GitHub Actions build/test passes
- README includes basic usage and screenshots

## Milestones

### Phase 0: Foundation

Status: mostly complete.

Scope:

- repository governance files
- build and test foundation
- CI workflow
- source layout
- developer setup documentation
- code formatting and static analysis policy
- application metadata and settings foundation

### Phase 1: MCP core and stdio MVP

Status: next focus.

Scope:

- JSON-RPC request, response, notification, and error types
- JSON parse/serialize helpers
- request ID and pending request lifecycle
- MCP initialize / initialized flow
- capability model
- transport abstraction
- stdio transport with `QProcess`
- stdout newline-delimited JSON parser
- stderr capture
- timeline event model and in-memory store
- minimal tools/list and tools/call support
- basic profile editor and connect/disconnect UI

Expected outcome: a working local stdio MCP inspector MVP.

### Phase 2: Inspector panels and schema forms

Scope:

- resources panel
- prompts panel
- notification display
- JSON Schema subset parser
- schema-driven input forms
- validation and JSON argument generation

Expected outcome: better coverage of MCP primitives and safer tool execution UX.

### Phase 3: Streamable HTTP transport

Scope:

- HTTP endpoint profiles
- JSON HTTP request/response handling
- `text/event-stream` handling
- session ID and protocol version headers
- HTTP transport error reporting

Expected outcome: support for both stdio and Streamable HTTP MCP servers.

### Phase 4: Security Workbench and reports

Scope:

- risk finding and risk score models
- rule engine
- command, environment, endpoint, tool, schema, and resource exposure checks
- security panel UI
- Markdown and HTML report export
- timeline summary reporting

Expected outcome: a practical local review tool for risky MCP configurations and server capabilities.

### Phase 5: Profile store and mcp.json editor

Scope:

- persistent profile storage
- `mcp.json` parser
- import/export UI
- validation report
- profile diff model and UI
- protocol version selector

Expected outcome: useful day-to-day MCP profile management.

### Phase 6: Test recorder and replay

Scope:

- test case model
- tool call recording
- replay runner
- assertions
- replay result UI
- import/export

Expected outcome: repeatable MCP server regression tests from inspector sessions.

### Phase 7: Server diff and compare

Scope:

- capability diff
- tools diff
- input schema diff
- resources and prompts diff
- breaking change detection
- diff report export

Expected outcome: review server changes across versions or environments.

### Phase 8: Packaging and OSS operations

Scope:

- Windows installer
- macOS DMG
- Linux package workflow
- GitHub Releases automation
- screenshots and sample server documentation
- issue and pull request templates
- release checklist
- dependency license review

Expected outcome: a distributable open-source desktop app.

## Prioritization rule

When in doubt, prioritize work that moves the project toward the v0.1.0 stdio MVP:

1. JSON-RPC and initialize support
2. stdio process transport
3. timeline visibility
4. tools/list and tools/call
5. profile connection UI
6. documentation and examples for a first demo
