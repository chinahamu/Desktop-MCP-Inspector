# Sample MCP server connection

This guide explains how to connect Desktop MCP Inspector to the repository's bundled demo MCP server.

The sample server is intentionally small and local-only. It supports:

- `initialize`
- `tools/list`
- `tools/call` for an `echo` tool

## Requirements

- Python 3.10 or later
- A built Desktop MCP Inspector executable

## Start the sample server manually

From the repository root, run:

```bash
python examples/mcp-servers/stdio-echo-server.py
```

The server reads one JSON-RPC object per line from stdin and writes one JSON-RPC object per line to stdout. Press `Ctrl+C` to stop it.

## Import the sample profile

Use the profile import flow and choose:

```text
examples/profiles/python-stdio-echo.mcp.json
```

The imported profile uses these values:

| Field | Value |
| --- | --- |
| Name | `python-stdio-echo` |
| Command | `python` |
| Args | `examples/mcp-servers/stdio-echo-server.py` |
| Working directory | repository root |
| Timeout | `5000` ms |
| Protocol version | `2025-06-18` |

On systems where Python is exposed as `python3`, change the command field to `python3` after import.

## Expected inspector flow

1. Select the `python-stdio-echo` profile.
2. Click Connect.
3. Run initialization.
4. Open the Tools panel.
5. Run `tools/list` and confirm the `echo` tool is visible.
6. Call `echo` with this JSON input:

```json
{
  "text": "hello from Desktop MCP Inspector"
}
```

The result should contain a single text content item with the same text.

## Troubleshooting

### The process starts and exits immediately

Confirm the working directory is the repository root and the script path exists.

### No tools are shown

Confirm initialization completed first, then rerun `tools/list`.

### Windows cannot find Python

Install Python from python.org or the Microsoft Store, then set the profile command to the correct launcher, such as `py` or `python`.
