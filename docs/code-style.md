# Code style and static analysis

This document defines the formatting and static analysis policy for Desktop MCP Inspector.

## Goals

- Keep C++ and Qt code consistent across contributors and platforms.
- Make formatting mechanical so reviews can focus on behavior and design.
- Introduce static analysis early without making the initial codebase difficult to evolve.

## clang-format

The repository uses `.clang-format` at the project root.

Recommended command from the repository root:

```bash
clang-format -i src/app/*.{cpp,hpp} tests/*.{cpp,hpp}
```

As more modules are implemented, format changed C++ files under:

- `src/`
- `tests/`

The main project conventions are:

- C++20 formatting baseline.
- 2-space indentation.
- 100-character column limit.
- Left-aligned pointer and reference markers.
- Custom brace wrapping for classes, structs, enums, unions, and functions.
- Include ordering is case-sensitive and preserves include blocks.

## clang-tidy

The repository uses `.clang-tidy` at the project root.

The active check groups are:

- `bugprone-*`
- `clang-analyzer-*`
- `cppcoreguidelines-*`
- `modernize-*`
- `performance-*`
- `portability-*`
- `readability-*`

Some checks are intentionally disabled while the project is still small and UI-heavy:

- magic-number checks
- strict identifier-length checks
- trailing-return-type migration
- selected pointer/bounds checks that are noisy for Qt integration code

Warnings are not treated as errors by default. Individual CI or release tasks may tighten this later.

## Running clang-tidy manually

Generate `compile_commands.json` with the debug preset:

```bash
cmake --preset debug
```

Then run clang-tidy on a source file:

```bash
clang-tidy src/app/main.cpp -p build/debug
```

Run it on multiple changed files by passing each file explicitly or by using your editor/IDE integration.

## Naming guidance

The default naming guidance is:

- classes, structs, and enums: `CamelCase`
- functions and variables: `lower_case`
- private data members: trailing underscore, for example `status_label_`
- namespaces: `lower_case`

Existing Qt names and framework overrides can follow Qt conventions where needed.

## Pull request expectations

Before opening a pull request that changes C++ files:

1. Run `clang-format` on changed C++ files.
2. Build with `cmake --build --preset debug`.
3. Run tests with `ctest --preset debug`.
4. Run `clang-tidy` on changed implementation files when practical.
5. Note any skipped checks in the PR description.

## Future work

Later infrastructure tasks may add CI jobs for format verification and clang-tidy. Until then, these settings establish the local policy and editor integration baseline.
