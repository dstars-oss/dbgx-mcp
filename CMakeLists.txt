cmake_minimum_required(VERSION 3.20)

project(dbgx_mcp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(WINDBG_EXTENSION_DEF_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/windbg_mcp_extension.def")
set(WINDBG_EXPORT_CHECK_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_windbg_exports.cmake")
set(WINDBG_REQUIRED_EXPORTS
  DebugExtensionInitialize
  DebugExtensionCanUnload
  DebugExtensionUninitialize
  DebugExtensionUnload
)
string(REPLACE ";" "|" WINDBG_REQUIRED_EXPORTS_ARG "${WINDBG_REQUIRED_EXPORTS}")
set(WINDBG_REQUIRED_EXPORTS_WITH_SENTINEL
  ${WINDBG_REQUIRED_EXPORTS}
  DebugExtensionMissingForGuardrail
)
string(REPLACE ";" "|" WINDBG_REQUIRED_EXPORTS_WITH_SENTINEL_ARG "${WINDBG_REQUIRED_EXPORTS_WITH_SENTINEL}")

add_library(windbg_mcp_extension SHARED
  src/mcp/http_server.cpp
  src/mcp/json.cpp
  src/mcp/json_rpc.cpp
  src/windbg/dbgeng_command_executor.cpp
  src/windbg_mcp_extension.cpp
  src/windbg_mcp_extension.def
)

target_include_directories(windbg_mcp_extension PRIVATE include)

target_compile_definitions(windbg_mcp_extension PRIVATE
  WIN32_LEAN_AND_MEAN
  NOMINMAX
)

target_link_libraries(windbg_mcp_extension PRIVATE
  dbgeng
  ws2_32
)

if(MSVC)
  target_link_options(windbg_mcp_extension PRIVATE
    "/DEF:${WINDBG_EXTENSION_DEF_FILE}"
  )
endif()

add_custom_target(check_windbg_exports
  COMMAND "${CMAKE_COMMAND}"
    "-DDLL_PATH=$<TARGET_FILE:windbg_mcp_extension>"
    "-DLINKER_EXECUTABLE=${CMAKE_LINKER}"
    "-DREQUIRED_EXPORTS=${WINDBG_REQUIRED_EXPORTS_ARG}"
    -P "${WINDBG_EXPORT_CHECK_SCRIPT}"
  DEPENDS windbg_mcp_extension
  COMMENT "Verifying WinDbg extension exports"
  VERBATIM
)

enable_testing()

add_executable(unit_tests
  src/mcp/json.cpp
  src/mcp/json_rpc.cpp
  tests/unit_tests.cpp
)

target_include_directories(unit_tests PRIVATE include)

add_test(NAME unit_tests COMMAND unit_tests)

add_test(NAME verify_windbg_exports
  COMMAND "${CMAKE_COMMAND}"
    "-DDLL_PATH=$<TARGET_FILE:windbg_mcp_extension>"
    "-DLINKER_EXECUTABLE=${CMAKE_LINKER}"
    "-DREQUIRED_EXPORTS=${WINDBG_REQUIRED_EXPORTS_ARG}"
    -P "${WINDBG_EXPORT_CHECK_SCRIPT}"
)

add_test(NAME verify_windbg_exports_missing_symbol
  COMMAND "${CMAKE_COMMAND}"
    "-DDLL_PATH=$<TARGET_FILE:windbg_mcp_extension>"
    "-DLINKER_EXECUTABLE=${CMAKE_LINKER}"
    "-DREQUIRED_EXPORTS=${WINDBG_REQUIRED_EXPORTS_WITH_SENTINEL_ARG}"
    -P "${WINDBG_EXPORT_CHECK_SCRIPT}"
)

set_tests_properties(verify_windbg_exports_missing_symbol PROPERTIES
  WILL_FAIL TRUE
)
