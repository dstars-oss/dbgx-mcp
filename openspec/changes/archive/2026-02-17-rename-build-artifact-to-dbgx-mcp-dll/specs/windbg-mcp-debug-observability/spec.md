# windbg-mcp-debug-observability 规范（增量）

## MODIFIED Requirements

### 需求:WinDbg 调试日志必须提供请求级关联能力
系统必须为每个 MCP 请求在 WinDbg 输出中提供可关联的请求标识，用于串联同一请求在不同阶段的日志记录。

#### 场景:请求包含 JSON-RPC id 时可直接关联
- **当** 客户端发送包含 `id` 的 JSON-RPC 请求
- **那么** 系统必须在该请求相关的阶段日志中稳定输出同一 `rpc_id`。

#### 场景:请求缺少 JSON-RPC id 时仍可关联
- **当** 客户端发送通知类请求或请求中缺少 `id`
- **那么** 系统必须输出进程内可追踪的本地关联标识
- **并且** 该标识必须在该请求生命周期内保持一致。

#### 场景:加载的产物文件名可被日志追踪
- **当** 请求日志生成阶段执行，且扩展 DLL 已加载
- **那么** 日志必须包含当前会话加载文件名，且该值必须为 `dbgx-mcp.dll`。

## ADDED Requirements

## REMOVED Requirements
