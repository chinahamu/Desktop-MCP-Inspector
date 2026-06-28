#!/usr/bin/env python3
"""Tiny stdio MCP demo server for Desktop MCP Inspector MVP profiles."""

from __future__ import annotations

import datetime as _dt
import json
import sys
from typing import Any

TOOLS: list[dict[str, Any]] = [
    {
        "name": "echo",
        "description": "Echo a text payload back to the caller.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "text": {"type": "string", "description": "Text to echo back"}
            },
            "required": ["text"],
        },
    },
    {
        "name": "time.now",
        "description": "Return the current local timestamp from the demo server process.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "timezone": {"type": "string", "default": "local"}
            },
        },
    },
]


def _write(message: dict[str, Any]) -> None:
    sys.stdout.write(json.dumps(message, ensure_ascii=False, separators=(",", ":")) + "\n")
    sys.stdout.flush()


def _success(request_id: Any, result: dict[str, Any]) -> dict[str, Any]:
    return {"jsonrpc": "2.0", "id": request_id, "result": result}


def _error(request_id: Any, code: int, message: str, data: Any | None = None) -> dict[str, Any]:
    error: dict[str, Any] = {"code": code, "message": message}
    if data is not None:
        error["data"] = data
    return {"jsonrpc": "2.0", "id": request_id, "error": error}


def _tool_text(text: str) -> dict[str, Any]:
    return {"content": [{"type": "text", "text": text}]}


def _handle_request(request: dict[str, Any]) -> dict[str, Any] | None:
    request_id = request.get("id")
    method = request.get("method")

    if method == "notifications/initialized":
        return None

    if method == "initialize":
        return _success(
            request_id,
            {
                "protocolVersion": request.get("params", {}).get("protocolVersion", "2025-06-18"),
                "capabilities": {"tools": {"listChanged": False}},
                "serverInfo": {"name": "desktop-mcp-inspector-demo", "version": "0.1.0"},
                "instructions": "Use tools/list and tools/call to inspect the demo echo and time tools.",
            },
        )

    if method == "tools/list":
        return _success(request_id, {"tools": TOOLS})

    if method == "tools/call":
        params = request.get("params", {})
        name = params.get("name")
        arguments = params.get("arguments", {})
        if name == "echo":
            return _success(request_id, _tool_text(str(arguments.get("text", ""))))
        if name == "time.now":
            timezone = arguments.get("timezone", "local")
            timestamp = _dt.datetime.now().astimezone().isoformat(timespec="seconds")
            return _success(request_id, _tool_text(f"{timezone}: {timestamp}"))
        return _success(
            request_id,
            {
                "content": [{"type": "text", "text": f"Unknown tool: {name}"}],
                "isError": True,
            },
        )

    return _error(request_id, -32601, f"Method not found: {method}")


def main() -> int:
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            request = json.loads(line)
        except json.JSONDecodeError as exc:
            _write(_error(None, -32700, "Parse error", {"detail": str(exc)}))
            continue

        response = _handle_request(request)
        if response is not None:
            _write(response)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
