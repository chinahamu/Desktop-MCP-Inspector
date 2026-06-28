add_executable(
  dmi_stdio_mock_server
  fixtures/stdio_mock_server.cpp
)

target_link_libraries(
  dmi_stdio_mock_server
  PRIVATE
    desktop_mcp_inspector_project_options
    desktop_mcp_inspector_warnings
)

add_executable(
  desktop_mcp_inspector_tests
  smoke_test.cpp
  app_settings_test.cpp
  mcp_message_types_test.cpp
  mcp_message_parse_test.cpp
  mcp_client_pending_test.cpp
  mcp_error_mapper_test.cpp
  mcp_protocol_info_test.cpp
  mcp_initialize_test.cpp
  mcp_initialized_notification_test.cpp
  mcp_capabilities_test.cpp
  mcp_request_lifecycle_test.cpp
  mcp_core_flow_test.cpp
  mcp_tool_test.cpp
  mcp_resource_test.cpp
  mcp_prompt_test.cpp
  mcp_notification_test.cpp
  profile_store_test.cpp
  profile_validation_test.cpp
  security_rules_test.cpp
  transport_interface_test.cpp
  transport_connection_test.cpp
  stdio_transport_test.cpp
  stdio_json_line_parser_test.cpp
  stdio_log_buffer_test.cpp
  stdio_process_error_test.cpp
  stdio_launch_spec_test.cpp
  stdio_protocol_violation_test.cpp
  stdio_transport_integration_test.cpp
  streamable_http_transport_test.cpp
  timeline_model_test.cpp
)

add_dependencies(desktop_mcp_inspector_tests dmi_stdio_mock_server)

target_compile_definitions(
  desktop_mcp_inspector_tests
  PRIVATE
    DMI_STDIO_MOCK_SERVER_PATH="$<TARGET_FILE:dmi_stdio_mock_server>"
)

target_link_libraries(
  desktop_mcp_inspector_tests
  PRIVATE
    desktop_mcp_inspector_app
    desktop_mcp_inspector_config
    desktop_mcp_inspector_mcp
    desktop_mcp_inspector_security
    desktop_mcp_inspector_timeline
    desktop_mcp_inspector_transport
    desktop_mcp_inspector_project_options
    desktop_mcp_inspector_warnings
    Qt6::Network
    Qt6::Sql
    Catch2::Catch2WithMain
)

catch_discover_tests(desktop_mcp_inspector_tests)
