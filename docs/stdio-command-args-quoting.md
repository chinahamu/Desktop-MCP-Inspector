# stdio command/args quoting behavior

This project launches stdio MCP servers with `QProcess::setProgram()` and `QProcess::setArguments()`.

## Rule

Keep the executable path and each argument as separate fields:

- `ServerProfile::command` contains only the executable path or executable name.
- `ServerProfile::args` contains one already-tokenized argument per list item.
- Do not concatenate `command` and `args` into a single shell command string.
- Do not pre-escape spaces or quotes for a shell unless the user explicitly chooses a shell as the executable.

## Why

`QProcess` performs platform-specific process creation. Passing arguments as a `QStringList` avoids most quoting differences between Windows, macOS, and Linux because the app does not ask a shell to split a command line.

This matters for arguments such as:

- paths with spaces: `C:\Program Files\MCP Server\server.py`
- macOS app support paths: `/Users/example/Library/Application Support/MCP/server.py`
- JSON snippets: `{"enabled":true}`
- quoted values: `quoted "inner" value`
- empty string arguments
- Unicode arguments

## Shell usage

When shell behavior is required, configure the shell itself as `command` and pass the shell flags and script as separate arguments.

Examples:

### Windows PowerShell

```text
command: powershell.exe
args:
  - -NoProfile
  - -ExecutionPolicy
  - Bypass
  - -File
  - C:\path with spaces\server.ps1
```

### macOS/Linux shell

```text
command: /bin/sh
args:
  - -c
  - exec "$0" "$@"
  - /path with spaces/server
  - --flag
```

Prefer direct executable launch over shell launch whenever possible.

## Verification coverage

`stdio_launch_spec_test.cpp` verifies that command, args, cwd, env, and timeout are preserved unchanged before they are applied to `QProcess`.
