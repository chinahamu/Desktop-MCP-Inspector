# Source layout

This directory is organized by responsibility so that MCP protocol logic, transport handling, UI code, and security analysis can evolve independently.

| Directory | Responsibility |
| --- | --- |
| `app/` | Application entry point, top-level window, and application controller wiring. |
| `mcp/` | MCP protocol data types, JSON-RPC messages, client/session behavior, and capability models. |
| `transport/` | Transport abstractions and implementations such as stdio and Streamable HTTP. |
| `timeline/` | Timeline events, in-memory/persistent stores, and Qt table models for traffic visualization. |
| `security/` | Security scanner, rule engine, risk findings, risk scoring, and MCP-specific checks. |
| `config/` | Server profiles, `mcp.json` parsing/exporting, validation, and profile diff logic. |
| `ui/` | Reusable inspector panels and Qt widgets outside the top-level app shell. |

Implementation files will be added incrementally by task. Empty module directories are kept with `.gitkeep` until their first source files are introduced.
