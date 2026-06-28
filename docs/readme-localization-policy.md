# README localization policy

Desktop MCP Inspector keeps the primary README readable for both international contributors and Japanese users.

## Policy

- Keep `README.md` primarily in English.
- Add short Japanese companion notes for setup, sample-server usage, and release status when they help Japanese users avoid ambiguity.
- Prefer one canonical README instead of diverging `README.ja.md` until the project reaches a larger public release.
- Use English for source code, commit messages, issue labels, workflow names, and CMake variables.
- Use Japanese where the repository plan or task list is already Japanese, such as files under `plan/`.

## README structure

Recommended order:

1. Project overview in English
2. Screenshot or UI preview
3. Quick start
4. Sample MCP server connection
5. Packaging and releases
6. Documentation links
7. Security posture
8. License
9. Japanese notes

## Translation rules

- Do not maintain separate wording that changes feature promises between languages.
- Keep Japanese notes concise and point back to canonical docs for details.
- When a release step changes, update the English section first, then update the Japanese notes if they mention the same step.
- Prefer direct technical terms such as `profile`, `tools/list`, `AppImage`, and `CPack` instead of forced translations.

## Future split criteria

Create a dedicated `README.ja.md` when at least two of these become true:

- Japanese installation instructions exceed the short notes section.
- The project has Japanese screenshots or release notes that need separate maintenance.
- External Japanese users start filing issues about README readability.
- A release requires localized store or package metadata.
