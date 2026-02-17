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

## WinDbg Debug Log Guide

The extension emits lifecycle-aware debug logs in WinDbg output for each `/mcp` request.

### Key fields

- `trace_id`: Request correlation key. Uses `rpc:<id>` when JSON-RPC `id` exists, otherwise `local-<seq>`.
- `stage`: Lifecycle phase (`request_received`, `route_dispatch`, `tool_execute_start`, `tool_execute_end`, `response_sent`).
- `duration_ms`: Elapsed milliseconds since request start.
- `rpc_method` / `rpc_id` / `tool`: Core RPC context fields for troubleshooting.
- `rpc_outcome`: Parsed result status (`success`, `error`, `unknown`).

### Example: successful `tools/call`

```text
[windbg-mcp] mcp.request method=POST trace_id=rpc:2 stage=request_received duration_ms=0 path=/mcp rpc_method=tools/call rpc_id=2 tool=windbg.eval body_bytes=...
[windbg-mcp] mcp.stage trace_id=rpc:2 stage=route_dispatch duration_ms=0 rpc_method=tools/call rpc_id=2 tool=windbg.eval outcome=in_progress msg=dispatching JSON-RPC request
[windbg-mcp] mcp.stage trace_id=rpc:2 stage=tool_execute_start duration_ms=0 rpc_method=tools/call rpc_id=2 tool=windbg.eval outcome=in_progress msg=entering tool executor
[windbg-mcp] mcp.response status=200 trace_id=rpc:2 stage=tool_execute_end duration_ms=4 has_body=true rpc_id=2 rpc_outcome=success tool=windbg.eval result=...
[windbg-mcp] mcp.response status=200 trace_id=rpc:2 stage=response_sent duration_ms=4 has_body=true rpc_id=2 rpc_outcome=success tool=windbg.eval result=...
```

### Example: invalid params failure

```text
[windbg-mcp] mcp.request method=POST trace_id=rpc:3 stage=request_received duration_ms=0 path=/mcp rpc_method=tools/call rpc_id=3 tool=windbg.eval body_bytes=...
[windbg-mcp] mcp.response status=200 trace_id=rpc:3 stage=tool_execute_end duration_ms=1 has_body=true rpc_id=3 rpc_outcome=error tool=windbg.eval error={"code":-32602,...}
[windbg-mcp] mcp.response status=200 trace_id=rpc:3 stage=response_sent duration_ms=1 has_body=true rpc_id=3 rpc_outcome=error tool=windbg.eval error={"code":-32602,...}
```

### Blocking diagnosis signal

If you see `stage=tool_execute_start` for a `trace_id` but never see `stage=tool_execute_end` or `stage=response_sent` with the same `trace_id`, the request is stalled inside command execution (not in HTTP routing).

Safety behavior remains unchanged:
- Sensitive headers are masked (`authorization=<masked>`).
- Long values are truncated with `...(truncated)`.

## Manual Validation Checklist (Log Readability)

1. Success path:
- Send a normal `tools/call` (for example `r eax`).
- Verify stage order: `request_received` -> `tool_execute_start` -> `tool_execute_end` -> `response_sent`.
- Verify `rpc_outcome=success` and consistent `trace_id`.

2. Failure path:
- Send `tools/call` without `arguments.command`.
- Verify response remains JSON-RPC error (`-32602`).
- Verify logs include `rpc_outcome=error` with matching `trace_id`.

3. Blocking observability path:
- Send a long-running command (for example `g`) in a suitable debug session.
- Verify `tool_execute_start` appears before completion.
- If no `tool_execute_end`/`response_sent` appears for the same `trace_id`, diagnose as execution-stage stall.

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
| Request summary includes trace/stage/tool fields | `TestIoEchoRequestSummaryIncludesTraceContext` |
| Missing JSON-RPC id is detected from request metadata | `TestIoEchoParseRequestMetaMissingId` |
| Local trace id stays consistent across lifecycle logs | `TestIoEchoLocalTraceIdConsistencyAcrossStages` |
| MCP response summary covers both success and error outcomes | `TestIoEchoResponseSummaryCoversSuccessAndError` |
| Tool result with `isError=true` is reported as error outcome | `TestIoEchoResponseSummaryTreatsToolIsErrorAsError` |
| Blocking diagnosis uses execution-before-response stage ordering | `TestIoEchoBlockingLocatabilityStageOrder` |
| Long MCP summaries are truncated with marker | `TestIoEchoSummaryTruncatesLongPayload` |
| Export symbol check passes | `verify_windbg_exports` |
| Missing export is blocked | `verify_windbg_exports_missing_symbol` (WILL_FAIL) |
| Load command path format is reusable | `Load in WinDbg` command examples |
| Load failure has diagnostics | `Troubleshooting .load failures` section |
| Invalid JSON handling | `TestParseError` |
