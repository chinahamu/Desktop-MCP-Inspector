include_guard(GLOBAL)

function(dmi_setup_project_options)
  set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard to use")
  set(CMAKE_CXX_STANDARD_REQUIRED ON CACHE BOOL "Require the configured C++ standard")
  set(CMAKE_CXX_EXTENSIONS OFF CACHE BOOL "Disable compiler-specific C++ extensions")

  set(CMAKE_AUTOMOC ON CACHE BOOL "Enable Qt Meta-Object Compiler integration")
  set(CMAKE_AUTORCC ON CACHE BOOL "Enable Qt resource compiler integration")
  set(CMAKE_AUTOUIC ON CACHE BOOL "Enable Qt UI compiler integration")

  set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile_commands.json for tooling")

  if(MSVC)
    add_link_options($<$<CONFIG:Debug>:/INCREMENTAL>)
  endif()

  option(DMI_BUILD_TESTS "Build Desktop MCP Inspector tests" ON)
  option(DMI_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
endfunction()

function(dmi_enable_msvc_hot_reload target_name)
  if(NOT MSVC)
    return()
  endif()

  # Visual Studio Hot Reload requires Edit and Continue debug information (/ZI)
  # on every translation unit, including the executable's main.cpp. The target
  # property is preferred by newer CMake generators; the explicit option keeps
  # older CMake/VS project generation compatible.
  set_property(
    TARGET ${target_name}
    PROPERTY MSVC_DEBUG_INFORMATION_FORMAT
      "$<$<CONFIG:Debug>:EditAndContinue>$<$<NOT:$<CONFIG:Debug>>:ProgramDatabase>"
  )
  target_compile_options(${target_name} PRIVATE $<$<CONFIG:Debug>:/ZI>)
  target_link_options(${target_name} PRIVATE $<$<CONFIG:Debug>:/INCREMENTAL>)
endfunction()
