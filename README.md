# WinDbg MCP HTTP Extension (MVP)

Language: English | [简体中文](README.zh-CN.md)

## Project Overview

This project provides a minimal C++ WinDbg extension DLL that exposes an MCP-compatible HTTP endpoint (`/mcp`) and a basic `windbg.eval` tool.

### Why use it

- Run WinDbg commands through a local MCP interface.
- Integrate debugger operations into MCP clients and agent workflows.
- Validate WinDbg + MCP integration with a small, dependency-light implementation.

### Who it is for

- Engineers building MCP tooling around Windows debugging.
- Teams that want scripted or agent-driven WinDbg command execution.
- Contributors who need a small reference implementation before scaling features.

### Typical scenarios

- Execute `windbg.eval` from an MCP client to inspect register/memory state.
- Build and test JSON-RPC routing for WinDbg-backed tools.
- Verify extension loading, exported symbols, and request/response visibility in WinDbg.

## Quick Start

### Prerequisites

- Windows
- CMake 3.20+
- MSVC toolchain (Visual Studio Build Tools)
- WinDbg SDK headers/libs (`DbgEng.h`, `dbgeng.lib`)

### 1. Build the extension

```powershell
cmake -S . -B build -G "Ninja"
cmake --build build
```

Expected result:
- Build succeeds.
- `build/Debug/windbg_mcp_extension.dll` is generated.

### 2. Load the extension in WinDbg

```text
.load "D:/Repos/Project/AI-Native/dbgx-mcp/build/Debug/windbg_mcp_extension.dll"
```

Expected result:
- `.load` succeeds without `Win32 error`.
- The extension starts a local endpoint at `http://127.0.0.1:5678/mcp`.

Important:
- Use forward slashes in `.load` paths.
- In debugger command contexts, backslashes may be treated as escapes, which can cause `Win32 error 0n2`.

### 3. Verify extension presence

```text
.chain
```

Expected result:
- `windbg_mcp_extension` appears in the extension chain output.

### 4. Send the first MCP request (`initialize`)

```powershell
$req = @{
  jsonrpc = "2.0"
  id = 1
  method = "initialize"
  params = @{ protocolVersion = "2025-11-25" }
} | ConvertTo-Json -Depth 4

Invoke-RestMethod -Uri "http://127.0.0.1:5678/mcp" -Method Post -ContentType "application/json" -Body $req
```

Expected result:
- Response contains `jsonrpc`, matching `id`, and `result`.

### 5. Run a debugger command via MCP (`tools/call`)

```powershell
$req = @{
  jsonrpc = "2.0"
  id = 2
  method = "tools/call"
  params = @{
    name = "windbg.eval"
    arguments = @{ command = "r eax" }
  }
} | ConvertTo-Json -Depth 6

Invoke-RestMethod -Uri "http://127.0.0.1:5678/mcp" -Method Post -ContentType "application/json" -Body $req
```

Expected result:
- Response returns command output text from WinDbg.

## Troubleshooting `.load` Failures

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
- `Win32 error 0n2`: wrong path or path separators were parsed incorrectly.
- `Win32 error 0n126`: dependent module not found in the current environment.

## MCP Request Reference

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

## Build and Test Details

Run unit tests:

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

Unit test policy (MVP):
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
