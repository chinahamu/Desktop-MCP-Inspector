#!/usr/bin/env python3
"""Minimal newline-delimited MCP stdio server for Desktop MCP Inspector demos.

This sample intentionally uses one JSON-RPC message per line because the current
Desktop MCP Inspector stdio transport is optimized for newline-delimited local
inspection fixtures.
"""

from __future__ import annotations

import json
import sys
from typing import Any

PROTOCOL_VERSION = "2025-06-18"


def write_message(message: dict[str, Any]) -> None:
    sys.stdout.write(json.dumps(message, separators=(",", ":")) + "\n")
    sys.stdout.flush()


def success(request_id: Any, result: dict[str, Any]) -> None:
    write_message({"jsonrpc": "2.0", "id": request_id, "result": result})


def failure(request_id: Any, code: int, message: str) -> None:
    write_message({"jsonrpc": "2.0", "id": request_id, "error": {"code": code, "message": message}})


def handle_request(request: dict[str, Any]) -> None:
    method = request.get("method")
    request_id = request.get("id")
    params = request.get("params") or {}

    if request_id is None:
        # Notifications such as initialized do not need a response.
        return

    if method == "initialize":
        success(
            request_id,
            {
                "protocolVersion": PROTOCOL_VERSION,
                "capabilities": {"tools": {}},
                "serverInfo": {"name": "desktop-mcp-inspector-echo", "version": "0.1.0"},
            },
        )
        return

    if method == "tools/list":
        success(
            request_id,
            {
                "tools": [
                    {
                        "name": "echo",
                        "description": "Return the provided text as MCP content.",
                        "inputSchema": {
                            "type": "object",
                            "properties": {"text": {"type": "string", "description": "Text to echo."}},
                            "required": ["text"],
                        },
                    }
                ]
            },
        )
        return

    if method == "tools/call":
        tool_name = params.get("name")
        arguments = params.get("arguments") or {}
        if tool_name != "echo":
            failure(request_id, -32602, f"Unknown tool: {tool_name}")
            return

        text = arguments.get("text", "")
        success(request_id, {"content": [{"type": "text", "text": str(text)}], "isError": False})
        return

    failure(request_id, -32601, f"Method not found: {method}")


def main() -> int:
    for raw_line in sys.stdin:
        line = raw_line.strip()
        if not line:
            continue
        try:
            message = json.loads(line)
            if not isinstance(message, dict):
                raise ValueError("JSON-RPC message must be an object")
            handle_request(message)
        except Exception as exc:  # noqa: BLE001 - demo server should keep running.
            failure(None, -32700, f"Parse or dispatch error: {exc}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
