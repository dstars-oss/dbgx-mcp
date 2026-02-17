# WinDbg MCP HTTP 扩展（MVP）

Language: [English](README.md) | 简体中文

## 项目概览

本项目提供一个最小化的 C++ WinDbg 扩展 DLL，对外暴露兼容 MCP 的 HTTP 端点（`/mcp`）以及基础工具 `windbg.eval`。

### 为什么使用它

- 通过本地 MCP 接口执行 WinDbg 命令。
- 将调试动作接入 MCP 客户端和智能体工作流。
- 以小而清晰、依赖最少的实现验证 WinDbg + MCP 集成。

### 适用人群

- 构建 Windows 调试相关 MCP 工具的工程师。
- 需要脚本化或智能体驱动 WinDbg 命令执行的团队。
- 想先基于小型参考实现再扩展能力的贡献者。

### 典型场景

- 从 MCP 客户端调用 `windbg.eval` 查看寄存器/内存状态。
- 验证 WinDbg 工具的 JSON-RPC 路由能力。
- 检查扩展加载、导出符号与 WinDbg 内请求/响应可见性。

## Quick Start

### 前置条件

- Windows
- CMake 3.20+
- MSVC 工具链（Visual Studio Build Tools）
- WinDbg SDK 头文件与库（`DbgEng.h`, `dbgeng.lib`）

### 1. 构建扩展

```powershell
cmake -S . -B build -G "Ninja"
cmake --build build
```

预期结果：
- 构建成功。
- 生成 `build/Debug/windbg_mcp_extension.dll`。

### 2. 在 WinDbg 中加载扩展

```text
.load "D:/Repos/Project/AI-Native/dbgx-mcp/build/Debug/windbg_mcp_extension.dll"
```

预期结果：
- `.load` 成功，不出现 `Win32 error`。
- 扩展启动本地端点 `http://127.0.0.1:5678/mcp`。

重要说明：
- `.load` 路径请使用正斜杠 `/`。
- 在调试器命令上下文中，反斜杠可能被当作转义字符，导致 `Win32 error 0n2`。

### 3. 验证扩展已加载

```text
.chain
```

预期结果：
- 扩展链输出中出现 `windbg_mcp_extension`。

### 4. 发送首个 MCP 请求（`initialize`）

```powershell
$req = @{
  jsonrpc = "2.0"
  id = 1
  method = "initialize"
  params = @{ protocolVersion = "2025-11-25" }
} | ConvertTo-Json -Depth 4

Invoke-RestMethod -Uri "http://127.0.0.1:5678/mcp" -Method Post -ContentType "application/json" -Body $req
```

预期结果：
- 响应包含 `jsonrpc`、匹配的 `id` 和 `result`。

### 5. 通过 MCP 执行调试命令（`tools/call`）

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

预期结果：
- 响应返回 WinDbg 命令输出文本。

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

## MCP 请求参考

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

## 构建与测试细节

运行单元测试：

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

单元测试策略（MVP）：
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
