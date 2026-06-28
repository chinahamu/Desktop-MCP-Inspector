# Contributing to Desktop MCP Inspector

Thank you for your interest in contributing to Desktop MCP Inspector.

This project is in the planning and early implementation stage. The initial focus is a small, reviewable C++/Qt MVP for inspecting stdio MCP servers.

## Development principles

- Keep protocol logic, transport logic, UI, storage, and security rules separated.
- Preserve raw JSON-RPC payloads for timeline and debugging features.
- Prefer small pull requests mapped to one task from `plan/07_task_list.md`.
- Keep behavior deterministic and testable where possible.
- Treat unknown MCP servers and profiles as untrusted inputs.

## Branch naming

Use short task-oriented branch names:

```text
feature/t-00-initial-files
feature/t-10-json-rpc-types
fix/t-25-process-failure-handling
docs/t-155-sample-mcp-server
```

## Commit messages

Use a concise task prefix when possible:

```text
T-10 Add JSON-RPC message types
T-22 Implement stdio process startup
T-90 Add risk finding model
```

## Pull request checklist

Before opening a pull request, check the following:

- The PR maps to one clearly scoped task or a small set of related tasks.
- The task list in `plan/07_task_list.md` is updated when the task is completed.
- New behavior is documented when it affects users or contributors.
- Tests are added or updated when code behavior changes.
- Security-sensitive changes mention risk assumptions and unsafe inputs.

## C++ and Qt guidelines

The concrete coding standards will be finalized after the CMake project is introduced. Until then, use these defaults:

- C++20 baseline.
- Prefer clear ownership and RAII over raw manual lifetime management.
- Keep UI classes thin; move protocol and security behavior into non-UI services.
- Avoid blocking the UI thread for process, network, or file operations.
- Prefer explicit error types over string-only error handling for protocol code.

## Formatting and linting

A formal `.clang-format` and clang-tidy policy will be added in a later task. For now:

- Use consistent formatting within each file.
- Keep public headers readable and minimal.
- Avoid large unrelated reformatting in functional PRs.

## Security-sensitive contributions

This application may launch local processes and inspect MCP servers that can access files or secrets. Contributions should avoid introducing behavior that silently executes untrusted commands, leaks environment variables, or stores secrets without masking.

If you believe you have found a security issue, follow `SECURITY.md` instead of opening a public issue.

## License

By contributing, you agree that your contribution is licensed under the Apache License 2.0.
