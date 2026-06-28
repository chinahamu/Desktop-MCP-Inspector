# Developer setup

This guide explains how to prepare a local development environment for Desktop MCP Inspector, configure the CMake project, build the Qt application, and run the test suite.

## Supported development targets

The project is intended to support Windows 11, macOS, and Linux desktop environments. The current CI workflow is focused on Windows while the early CMake and Qt setup stabilizes.

## Required tools

Install the following tools before configuring the project:

| Tool | Minimum version | Notes |
| --- | --- | --- |
| C++ compiler | C++20 capable | MSVC, Clang, or GCC. |
| CMake | 3.24 | Required by `CMakePresets.json`. |
| Qt | 6.5 | `Core` and `Widgets` components are required. |
| Ninja | latest stable | Used by the default CMake presets. |
| Git | latest stable | Required for source checkout and dependencies fetched by CMake. |

Catch2 is fetched by CMake when tests are enabled, so the first configure step needs network access unless the dependency is already available in the CMake build cache.

## Windows setup

Recommended Windows environment:

- Visual Studio 2022 with the Desktop development with C++ workload
- CMake 3.24 or later
- Ninja
- Qt 6.5 or later for MSVC

Qt can be installed with the official Qt installer or with a package manager used by CI. Make sure CMake can find Qt by setting one of the following before configuring:

```powershell
$env:CMAKE_PREFIX_PATH = "C:\Qt\6.5.3\msvc2019_64"
```

or by passing the prefix directly:

```powershell
cmake --preset debug -DCMAKE_PREFIX_PATH="C:\Qt\6.5.3\msvc2019_64"
```

If your local Qt installation uses a different version or compiler ABI, replace the path with your installed Qt prefix.

## macOS setup

Install Xcode command line tools, CMake, Ninja, and Qt 6.5 or later. With Homebrew, a typical setup is:

```bash
brew install cmake ninja qt
```

Then expose Qt to CMake:

```bash
export CMAKE_PREFIX_PATH="$(brew --prefix qt)"
```

## Linux setup

Install a C++20 compiler, CMake, Ninja, and Qt 6 development packages. Package names vary by distribution. On Ubuntu-like systems, the package set is typically similar to:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build qt6-base-dev
```

## Configure, build, and test

From the repository root:

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

The debug preset writes build files under `build/debug`. The test preset enables failure output so failed Catch2 tests are easier to inspect.

For an optimized local build:

```bash
cmake --preset release
cmake --build --preset release
```

## Test configuration

Tests are controlled by the `DMI_BUILD_TESTS` CMake option. It defaults to `ON`.

Disable tests when only building the application shell:

```bash
cmake --preset debug -DDMI_BUILD_TESTS=OFF
cmake --build --preset debug
```

Enable warnings as errors for stricter local checks:

```bash
cmake --preset debug -DDMI_WARNINGS_AS_ERRORS=ON
cmake --build --preset debug
```

## Running the application

After a successful debug build, the application target is named `DesktopMCPInspector` through the CMake target `desktop_mcp_inspector`.

The exact executable path depends on the generator and platform, but it will be located under the selected build directory, such as `build/debug`.

## Continuous integration

The `Build and Test` GitHub Actions workflow runs on pull requests, pushes to `main`, and manual dispatch. It installs the build toolchain, configures the debug preset, builds the project, and runs CTest.

When changing CMake, Qt, or tests, confirm that the same commands used locally are reflected in `.github/workflows/build-test.yml`.

## Troubleshooting

### CMake cannot find Qt

Check that Qt is installed and that `CMAKE_PREFIX_PATH` points to the Qt prefix containing `lib/cmake/Qt6/Qt6Config.cmake`.

On Windows, also confirm that the Qt package matches the compiler ABI, such as an MSVC Qt package for an MSVC build.

### Ninja is not found

Install Ninja and confirm it is available on `PATH`:

```bash
ninja --version
```

### Catch2 fetch fails

The first configure can download Catch2 through CMake `FetchContent`. Retry with network access, or use a pre-populated CMake dependency cache in restricted environments.

### Tests are not discovered

Confirm that `DMI_BUILD_TESTS` is `ON` and that `ctest --preset debug` is executed after a successful build.

## Development workflow

1. Create a branch from the latest `main`.
2. Configure with `cmake --preset debug`.
3. Build with `cmake --build --preset debug`.
4. Run tests with `ctest --preset debug`.
5. Update relevant documentation and the task list.
6. Open a pull request against `main`.
