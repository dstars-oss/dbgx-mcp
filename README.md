# WinDbg MCP HTTP Extension (MVP)

Language: English | [简体中文](README.zh-CN.md)

This repository contains a minimal WinDbg extension DLL written in C++ that exposes an MCP-compatible HTTP endpoint and a basic `windbg.eval` tool.

## Goals

- C++ WinDbg extension DLL.
- No third-party dependencies (Windows and WinDbg libraries only).
- CMake-based build.
- Minimal MCP JSON-RPC methods:
  - `initialize`
  - `tools/list`
  - `tools/call`
- Basic tool:
  - `windbg.eval`: execute any WinDbg command and return text output.

## Build

Prerequisites:

- Windows
- CMake 3.20+
- MSVC toolchain (Visual Studio Build Tools)
- WinDbg SDK headers/libs (`DbgEng.h`, `dbgeng.lib`)

Configure and build:

```powershell
cmake -S . -B build -G "Ninja"
cmake --build build
```

Run unit tests:

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

## Load in WinDbg

Load extension:

```text
.load "D:/Repos/Project/AI-Native/dbgx-mcp/build/Debug/windbg_mcp_extension.dll"
```

Important: prefer forward slashes in the `.load` path. In debugger command contexts, backslashes can be treated as escape characters and path separators may be stripped, which produces `Win32 error 0n2` (`The system cannot find the file specified`).

After loading, the extension starts a local HTTP endpoint at:

```text
http://127.0.0.1:5678/mcp
```

## MCP Calls

### `initialize`

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "initialize",
  "params": {
    "protocolVersion": "2025-11-25"
  }
}
```

### `tools/list`

```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "method": "tools/list",
  "params": {}
}
```

### `tools/call` (`windbg.eval`)

```json
{
  "jsonrpc": "2.0",
  "id": 3,
  "method": "tools/call",
  "params": {
    "name": "windbg.eval",
    "arguments": {
      "command": "r eax"
    }
  }
}
```

## Security Notes (MVP)

- Binds to `127.0.0.1` only.
- Validates `Origin` when present, allowing only `http://localhost...` and `http://127.0.0.1...`.
- Supports HTTP `POST /mcp` for JSON-RPC.
- `GET /mcp` returns 405 in this MVP (no SSE stream yet).

## Troubleshooting `.load` failures

1. Confirm the DLL path exists and is absolute.
2. Verify required exports are present:

```powershell
cmake --build build --config Debug --target check_windbg_exports
```

3. Run export checks in test flow:

```powershell
ctest --test-dir build -C Debug --output-on-failure -R verify_windbg_exports
```

4. If loading still fails, inspect dependent modules:

```powershell
dumpbin /dependents build\Debug\windbg_mcp_extension.dll
```

Common errors:
- `Win32 error 0n2`: path is wrong or path separators were parsed incorrectly.
- `Win32 error 0n126`: dependent module not found in the current environment.

## Minimal manual acceptance (WinDbg)

1. Build Debug binaries.
2. Run `verify_windbg_exports` checks.
3. In WinDbg/CDB, execute `.load` with the forward-slash absolute path format shown above.
4. Run `.chain` and confirm `windbg_mcp_extension` appears.
5. Send a minimal `initialize` or `tools/call` request to `http://127.0.0.1:5678/mcp` and confirm WinDbg output shows request/response summaries.
6. If loading fails, follow the troubleshooting steps.

## Unit test policy (MVP)

- Test pure logic first: JSON parsing and JSON-RPC routing.
- Keep WinDbg and socket operations in thin adapters.
- Every key behavior in the spec maps to at least one test.

### Spec-to-test mapping

| Spec scenario | Unit test |
| --- | --- |
| Initialize request succeeds | `TestInitialize` |
| Tools list request succeeds | `TestToolsList` |
| Command execution succeeds | `TestToolsCallSuccess` |
| Missing command argument | `TestToolsCallMissingCommand` |
| Unknown method is rejected | `TestUnknownMethod` |
| MCP request summary includes RPC metadata and masks sensitive headers | `TestIoEchoRequestSummaryMasksSensitiveHeader` |
| MCP response summary covers both success and error outcomes | `TestIoEchoResponseSummaryCoversSuccessAndError` |
| Long MCP summaries are truncated with marker | `TestIoEchoSummaryTruncatesLongPayload` |
| Export symbol check passes | `verify_windbg_exports` |
| Missing export is blocked | `verify_windbg_exports_missing_symbol` (WILL_FAIL) |
| Load command path format is reusable | `Load in WinDbg` command examples |
| Load failure has diagnostics | `Troubleshooting .load failures` section |
| Invalid JSON handling | `TestParseError` |
