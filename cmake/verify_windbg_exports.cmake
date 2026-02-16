cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED DLL_PATH OR DLL_PATH STREQUAL "")
  message(FATAL_ERROR "DLL_PATH is required (path to windbg extension DLL).")
endif()

if(NOT EXISTS "${DLL_PATH}")
  message(FATAL_ERROR "DLL_PATH does not exist: ${DLL_PATH}")
endif()

if(NOT DEFINED REQUIRED_EXPORTS OR REQUIRED_EXPORTS STREQUAL "")
  message(FATAL_ERROR "REQUIRED_EXPORTS is required (pipe-separated export names).")
endif()

set(_required_exports "${REQUIRED_EXPORTS}")
string(REPLACE "|" ";" _required_exports "${_required_exports}")

set(_dump_tool "")
set(_dump_args "")

if(DEFINED LINKER_EXECUTABLE AND NOT LINKER_EXECUTABLE STREQUAL "" AND EXISTS "${LINKER_EXECUTABLE}")
  set(_dump_tool "${LINKER_EXECUTABLE}")
  set(_dump_args /dump /exports "${DLL_PATH}")
else()
  find_program(_dumpbin dumpbin)
  if(_dumpbin)
    set(_dump_tool "${_dumpbin}")
    set(_dump_args /exports "${DLL_PATH}")
  endif()
endif()

if(_dump_tool STREQUAL "")
  message(FATAL_ERROR "Could not locate export dump tool. Set LINKER_EXECUTABLE or add dumpbin to PATH.")
endif()

execute_process(
  COMMAND "${_dump_tool}" ${_dump_args}
  RESULT_VARIABLE _dump_result
  OUTPUT_VARIABLE _dump_stdout
  ERROR_VARIABLE _dump_stderr
)

if(NOT _dump_result EQUAL 0)
  message(FATAL_ERROR
    "Failed to inspect exports from ${DLL_PATH}\n"
    "Tool: ${_dump_tool}\n"
    "Exit code: ${_dump_result}\n"
    "stderr:\n${_dump_stderr}\n"
    "stdout:\n${_dump_stdout}")
endif()

set(_missing_exports "")
foreach(_required_export IN LISTS _required_exports)
  if(NOT _dump_stdout MATCHES "(^|[ \t\r\n])${_required_export}([ \t\r\n]|$)")
    list(APPEND _missing_exports "${_required_export}")
  endif()
endforeach()

if(_missing_exports)
  list(JOIN _missing_exports ", " _missing_exports_text)
  message(FATAL_ERROR
    "Missing required exports: ${_missing_exports_text}\n"
    "Checked DLL: ${DLL_PATH}\n"
    "Hint: ensure src/windbg_mcp_extension.def and exported function declarations stay aligned.\n"
    "Export dump output:\n${_dump_stdout}")
endif()

message(STATUS "Export verification passed for ${DLL_PATH}")
