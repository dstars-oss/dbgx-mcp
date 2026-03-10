# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

dbgx-mcp is a C++ WinDbg extension DLL that exposes an MCP (Model Context Protocol) HTTP endpoint at `/mcp`. It provides a `windbg.eval` tool for remote WinDbg command execution via JSON-RPC. Zero third-party dependencies — only Windows SDK and WinDbg SDK.

## Build & Test

**Target platform:** Windows only (MSVC toolchain). Requires CMake 3.20+, WinDbg SDK (`DbgEng.h`, `dbgeng.lib`).

```powershell
# Configure
cmake -S . -B build -G "Ninja"

# Build
cmake --build build

# Run all tests (unit tests + export verification)
ctest --test-dir build -C Debug --output-on-failure

# Run only unit tests
ctest --test-dir build -C Debug --output-on-failure -R unit_tests

# Verify DLL exports
cmake --build build --config Debug --target check_windbg_exports
```

The build produces `build/Debug/dbgx-mcp.dll`.

## Architecture

```
dbgx-mcp.cpp          -- WinDbg extension entry points (DllMain lifecycle),
                          HTTP request orchestration, diagnostic log emission
  ├── mcp/http_server  -- Raw Winsock HTTP server with port-conflict auto-fallback
  ├── mcp/json_rpc     -- JSON-RPC 2.0 router: initialize, tools/list, tools/call
  ├── mcp/json         -- Hand-rolled JSON parser (no dependencies), FieldMap-based
  ├── mcp/io_echo      -- Request/response diagnostic summary builder for WinDbg output
  └── windbg/
      ├── command_executor.hpp      -- IWinDbgCommandExecutor interface
      └── dbgeng_command_executor   -- Real DbgEng implementation
```

**Key design patterns:**
- `IWinDbgCommandExecutor` interface enables test doubles — unit tests use `FakeExecutor` to avoid DbgEng dependency.
- Unit test binary links only `ws2_32` (no `dbgeng`), so tests compile on any Windows machine with Winsock.
- JSON handling uses `dbgx::json::FieldMap` (string→raw-JSON-value map) for lazy field extraction, not a DOM tree.
- Request tracing uses `trace_id` correlation (`rpc:<id>` or `local-<seq>`) across lifecycle stages.

## Conventions

- C++20, namespaces: `dbgx::mcp`, `dbgx::windbg`, `dbgx::json`
- Headers in `include/dbgx/`, sources in `src/`, tests in `tests/`
- DLL exports defined in `src/dbgx-mcp.def` — must stay aligned with `DebugExtensionInitialize`, `DebugExtensionCanUnload`, `DebugExtensionUninitialize`, `DebugExtensionUnload`
- Tests are a single `tests/unit_tests.cpp` with a hand-rolled test harness (no framework). Each test function takes `int* failures` and uses `Expect()` + `Contains()` helpers.
- Specs managed via openspec: `openspec/specs/` for current specs, `openspec/changes/` for change tracking

## Spec Management

This project uses the **openspec** workflow (see `.codex/skills/openspec-*/SKILL.md`). Active specs live in `openspec/specs/`, completed changes are archived in `openspec/changes/archive/`.
