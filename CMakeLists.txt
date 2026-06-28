cmake_minimum_required(VERSION 3.24)

project(
  DesktopMcpInspector
  VERSION 0.1.0
  DESCRIPTION "Native desktop inspector for Model Context Protocol servers"
  HOMEPAGE_URL "https://github.com/chinahamu/Desktop-MCP-Inspector"
  LANGUAGES CXX
)

include(GNUInstallDirs)
include(InstallRequiredSystemLibraries)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(ProjectOptions)
include(CompilerWarnings)
include(Testing)

dmi_setup_project_options()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Sql Network)
qt_standard_project_setup()

if(APPLE)
  # List of targets to inspect and clean of AGL framework link option
  set(_targets
    Qt6::Gui
    Qt6::Widgets
    Qt::Gui
    Qt::Widgets
    Qt6::Gui_private
    Qt6::Widgets_private
    Qt::Gui_private
    Qt::Widgets_private
    OpenGL::GL
    OpenGL::GLU
    WrapOpenGL::WrapOpenGL
  )
  foreach(_target IN LISTS _targets)
    if(TARGET ${_target})
      # 1. Clean INTERFACE_LINK_LIBRARIES
      get_target_property(_libs ${_target} INTERFACE_LINK_LIBRARIES)
      message(STATUS "Target ${_target} original libs: ${_libs}")
      if(_libs)
        set(_new_libs "")
        foreach(_lib IN LISTS _libs)
          if(NOT "${_lib}" MATCHES "AGL")
            list(APPEND _new_libs "${_lib}")
          endif()
        endforeach()
        set_target_properties(${_target} PROPERTIES INTERFACE_LINK_LIBRARIES "${_new_libs}")
        get_target_property(_libs_after ${_target} INTERFACE_LINK_LIBRARIES)
        message(STATUS "Target ${_target} updated libs: ${_libs_after}")
      endif()

      # 2. Clean INTERFACE_LINK_OPTIONS
      get_target_property(_opts ${_target} INTERFACE_LINK_OPTIONS)
      message(STATUS "Target ${_target} original opts: ${_opts}")
      if(_opts)
        set(_new_opts "")
        foreach(_opt IN LISTS _opts)
          if(NOT "${_opt}" MATCHES "AGL")
            list(APPEND _new_opts "${_opt}")
          endif()
        endforeach()
        set_target_properties(${_target} PROPERTIES INTERFACE_LINK_OPTIONS "${_new_opts}")
        get_target_property(_opts_after ${_target} INTERFACE_LINK_OPTIONS)
        message(STATUS "Target ${_target} updated opts: ${_opts_after}")
      endif()
    endif()
  endforeach()
endif()

add_library(desktop_mcp_inspector_project_options INTERFACE)
target_compile_features(desktop_mcp_inspector_project_options INTERFACE cxx_std_20)
target_link_libraries(
  desktop_mcp_inspector_project_options
  INTERFACE
    Qt6::Core
    Qt6::Widgets
)

add_library(desktop_mcp_inspector_warnings INTERFACE)
dmi_set_project_warnings(desktop_mcp_inspector_warnings)

file(
  GLOB_RECURSE
  DMI_CORE_SOURCES
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)
list(REMOVE_ITEM DMI_CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/app/main.cpp")

add_library(desktop_mcp_inspector_core ${DMI_CORE_SOURCES})

target_include_directories(
  desktop_mcp_inspector_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app
    ${CMAKE_CURRENT_SOURCE_DIR}/src/compare
    ${CMAKE_CURRENT_SOURCE_DIR}/src/config
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mcp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recorder
    ${CMAKE_CURRENT_SOURCE_DIR}/src/report
    ${CMAKE_CURRENT_SOURCE_DIR}/src/security
    ${CMAKE_CURRENT_SOURCE_DIR}/src/timeline
    ${CMAKE_CURRENT_SOURCE_DIR}/src/transport
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
)

target_link_libraries(
  desktop_mcp_inspector_core
  PUBLIC
    desktop_mcp_inspector_project_options
    Qt6::Core
    Qt6::Widgets
    Qt6::Sql
    Qt6::Network
)

target_link_libraries(
  desktop_mcp_inspector_core
  PRIVATE
    desktop_mcp_inspector_warnings
)

dmi_enable_msvc_hot_reload(desktop_mcp_inspector_core)

qt_add_executable(
  desktop_mcp_inspector
  src/app/main.cpp
  src/app/resources.qrc
)

set_target_properties(
  desktop_mcp_inspector
  PROPERTIES
    OUTPUT_NAME DesktopMCPInspector
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(
  desktop_mcp_inspector
  PRIVATE
    desktop_mcp_inspector_core
    desktop_mcp_inspector_warnings
)

dmi_enable_msvc_hot_reload(desktop_mcp_inspector)

install(
  TARGETS desktop_mcp_inspector
  BUNDLE DESTINATION .
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
  FILES README.md LICENSE
  DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

install(
  DIRECTORY docs/
  DESTINATION ${CMAKE_INSTALL_DOCDIR}/docs
  FILES_MATCHING
    PATTERN "*.md"
    PATTERN "*.svg"
)

install(
  DIRECTORY examples/
  DESTINATION ${CMAKE_INSTALL_DATADIR}/DesktopMCPInspector/examples
)

if((WIN32 OR APPLE) AND COMMAND qt_generate_deploy_app_script)
  qt_generate_deploy_app_script(
    TARGET desktop_mcp_inspector
    OUTPUT_SCRIPT DMI_QT_DEPLOY_SCRIPT
  )
  install(SCRIPT ${DMI_QT_DEPLOY_SCRIPT})
endif()

if(DMI_BUILD_TESTS)
  enable_testing()
  dmi_setup_testing()

  add_executable(
    desktop_mcp_inspector_security_tests
    tests/security/SecurityScannerTest.cpp
  )

  target_link_libraries(
    desktop_mcp_inspector_security_tests
    PRIVATE
      desktop_mcp_inspector_core
      desktop_mcp_inspector_warnings
      Catch2::Catch2WithMain
  )

  dmi_enable_msvc_hot_reload(desktop_mcp_inspector_security_tests)

  catch_discover_tests(desktop_mcp_inspector_security_tests)

  add_executable(
    desktop_mcp_inspector_report_tests
    tests/report/ReportWriterSnapshotTest.cpp
  )

  target_link_libraries(
    desktop_mcp_inspector_report_tests
    PRIVATE
      desktop_mcp_inspector_core
      desktop_mcp_inspector_warnings
      Catch2::Catch2WithMain
  )

  dmi_enable_msvc_hot_reload(desktop_mcp_inspector_report_tests)

  catch_discover_tests(desktop_mcp_inspector_report_tests)

  add_executable(
    desktop_mcp_inspector_config_tests
    tests/config/McpJsonProfileDiffTest.cpp
  )

  target_link_libraries(
    desktop_mcp_inspector_config_tests
    PRIVATE
      desktop_mcp_inspector_core
      desktop_mcp_inspector_warnings
      Catch2::Catch2WithMain
  )

  dmi_enable_msvc_hot_reload(desktop_mcp_inspector_config_tests)

  catch_discover_tests(desktop_mcp_inspector_config_tests)

  add_executable(
    desktop_mcp_inspector_compare_tests
    tests/compare/ServerDiffTest.cpp
  )

  target_link_libraries(
    desktop_mcp_inspector_compare_tests
    PRIVATE
      desktop_mcp_inspector_core
      desktop_mcp_inspector_warnings
      Catch2::Catch2WithMain
  )

  dmi_enable_msvc_hot_reload(desktop_mcp_inspector_compare_tests)

  catch_discover_tests(desktop_mcp_inspector_compare_tests)
endif()

set(CPACK_PACKAGE_NAME "DesktopMCPInspector")
set(CPACK_PACKAGE_VENDOR "Meta Alchemist")
set(CPACK_PACKAGE_CONTACT "https://github.com/chinahamu/Desktop-MCP-Inspector/issues")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Desktop inspector for Model Context Protocol servers")
set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Desktop MCP Inspector")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_EXECUTABLES "DesktopMCPInspector" "Desktop MCP Inspector")
set(CPACK_CREATE_DESKTOP_LINKS "DesktopMCPInspector")
set(CPACK_PACKAGE_FILE_NAME "DesktopMCPInspector-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")

if(WIN32 AND NOT CPACK_GENERATOR)
  set(CPACK_GENERATOR "NSIS;ZIP")
elseif(APPLE AND NOT CPACK_GENERATOR)
  set(CPACK_GENERATOR "DragNDrop;TGZ")
elseif(UNIX AND NOT APPLE AND NOT CPACK_GENERATOR)
  set(CPACK_GENERATOR "DEB;TGZ")
endif()

set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Meta Alchemist")
set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

include(CPack)
