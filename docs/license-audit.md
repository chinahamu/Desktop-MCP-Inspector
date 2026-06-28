# License audit notes

Desktop MCP Inspector is licensed under Apache-2.0. The canonical license text is stored in `LICENSE`.

This document records the current third-party dependency and release-tooling license review items for v0.1.0.

## Project license

| Item | License | Status |
| --- | --- | --- |
| Desktop MCP Inspector source | Apache-2.0 | Confirmed by `LICENSE` and README |

## Runtime dependencies

| Dependency | Use | License notes | Redistribution note |
| --- | --- | --- | --- |
| Qt 6 Core / Widgets / Sql / Network | Application runtime | Qt is available under commercial and open-source terms, including LGPL/GPL variants depending on modules and distribution. | Confirm the chosen release packaging path satisfies the Qt license used for the build. Prefer dynamic linking unless a commercial license permits another mode. |
| Platform C/C++ runtime | Compiler/runtime support | Depends on compiler and OS distribution. | Windows installer may include required MSVC runtime files through CMake's system runtime support. |

## Test-only dependencies

| Dependency | Use | License notes | Redistribution note |
| --- | --- | --- | --- |
| Catch2 v3.7.1 | Unit tests | Boost Software License 1.0 | Fetched by CMake for tests. Not required in release packages. |

## Release tooling

| Tool or action | Use | License review action |
| --- | --- | --- |
| CPack | Package generation | Distributed as part of CMake tooling, not bundled in app packages. |
| NSIS | Windows installer generation | Review NSIS license before distributing generated installers if installer stubs are bundled. |
| linuxdeploy / linuxdeploy-plugin-qt | AppImage generation | Review licenses before publishing AppImage assets. |
| GitHub Actions marketplace actions | CI/release automation | Not redistributed in application packages. Pin versions and review action maintainers periodically. |

## v0.1.0 audit checklist

- [ ] Confirm all runtime libraries included in each package.
- [ ] Confirm Qt deployment mode and obligations for each platform.
- [ ] Confirm generated Windows installer includes any required notices.
- [ ] Confirm AppImage includes notices for bundled shared libraries.
- [ ] Confirm no source file copied from third-party projects is missing attribution.
- [ ] Add a `NOTICE` file if bundled dependencies require attribution beyond the Apache-2.0 license text.

## Current conclusion

The repository license is Apache-2.0 and is already present. No additional bundled runtime notices are committed yet because release artifacts have not been generated and inspected. Re-run this audit whenever release packaging starts bundling Qt or platform runtime libraries.
