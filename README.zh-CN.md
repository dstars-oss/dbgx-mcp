# WinDbg MCP HTTP 扩展（MVP）

Language: [English](README.md) | 简体中文

本仓库包含一个使用 C++ 编写的最小化 WinDbg 扩展 DLL，提供兼容 MCP 的 HTTP 端点，以及基础工具 `windbg.eval`。

## 目标

- C++ WinDbg 扩展 DLL。
- 无第三方依赖（仅使用 Windows 与 WinDbg 库）。
- 基于 CMake 的构建。
- 最小 MCP JSON-RPC 方法集：
  - `initialize`
  - `tools/list`
  - `tools/call`
- 基础工具：
  - `windbg.eval`：执行任意 WinDbg 命令并返回文本输出。

## 构建

前置条件：

- Windows
- CMake 3.20+
- MSVC 工具链（Visual Studio Build Tools）
- WinDbg SDK 头文件与库（`DbgEng.h`, `dbgeng.lib`）

配置并构建：

```powershell
cmake -S . -B build -G "Ninja"
cmake --build build
```

运行单元测试：

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

## 在 WinDbg 中加载

加载扩展：

```text
.load "D:/Repos/Project/AI-Native/dbgx-mcp/build/Debug/windbg_mcp_extension.dll"
```

重要说明：`.load` 路径建议使用正斜杠 `/`。在调试器命令上下文中，反斜杠可能会被当作转义字符，导致路径分隔符被错误处理，从而出现 `Win32 error 0n2`（`The system cannot find the file specified`）。

加载后，扩展会在本地启动 HTTP 端点：

```text
http://127.0.0.1:5678/mcp
```

## MCP 调用示例

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

### `tools/call`（`windbg.eval`）

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

## 安全说明（MVP）

- 仅绑定到 `127.0.0.1`。
- 当请求包含 `Origin` 时进行校验，仅允许 `http://localhost...` 与 `http://127.0.0.1...`。
- 支持 HTTP `POST /mcp` 的 JSON-RPC 调用。
- 当前 MVP 中 `GET /mcp` 返回 405（暂不支持 SSE 流）。

## `.load` 失败排查

1. 确认 DLL 路径存在且为绝对路径。
2. 验证必需导出符号：

```powershell
cmake --build build --config Debug --target check_windbg_exports
```

3. 在测试流程中执行导出检查：

```powershell
ctest --test-dir build -C Debug --output-on-failure -R verify_windbg_exports
```

4. 若仍加载失败，检查依赖模块：

```powershell
dumpbin /dependents build\Debug\windbg_mcp_extension.dll
```

常见错误：
- `Win32 error 0n2`：路径错误，或路径分隔符被错误解析。
- `Win32 error 0n126`：当前环境缺少依赖模块。

## 最小手动验收（WinDbg）

1. 构建 Debug 二进制。
2. 执行 `verify_windbg_exports` 检查。
3. 在 WinDbg/CDB 中按上文格式使用正斜杠绝对路径执行 `.load`。
4. 运行 `.chain` 并确认出现 `windbg_mcp_extension`。
5. 向 `http://127.0.0.1:5678/mcp` 发送最小 `initialize` 或 `tools/call` 请求，确认 WinDbg 输出中可见请求/响应摘要。
6. 若加载失败，按排查步骤处理。

## 单元测试策略（MVP）

- 优先测试纯逻辑：JSON 解析与 JSON-RPC 路由。
- 将 WinDbg 与 socket 操作保持为轻量适配层。
- 规范中的每个关键行为至少映射一个测试。

### 规范与测试映射

| 规范场景 | 单元测试 |
| --- | --- |
| 初始化请求成功 | `TestInitialize` |
| 工具列表请求成功 | `TestToolsList` |
| 命令执行成功 | `TestToolsCallSuccess` |
| 缺少命令参数 | `TestToolsCallMissingCommand` |
| 未知方法被拒绝 | `TestUnknownMethod` |
| MCP 请求摘要包含 RPC 元信息且掩码敏感头 | `TestIoEchoRequestSummaryMasksSensitiveHeader` |
| MCP 响应摘要覆盖成功与错误路径 | `TestIoEchoResponseSummaryCoversSuccessAndError` |
| 超长 MCP 摘要会被截断并带标记 | `TestIoEchoSummaryTruncatesLongPayload` |
| 导出符号检查通过 | `verify_windbg_exports` |
| 缺失导出被阻断 | `verify_windbg_exports_missing_symbol` (WILL_FAIL) |
| 加载命令路径写法可复用 | `Load in WinDbg` command examples |
| 加载失败有诊断指引 | `Troubleshooting .load failures` section |
| 非法 JSON 处理 | `TestParseError` |
