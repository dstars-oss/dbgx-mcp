## 上下文

项目当前为新建状态，没有既有运行时代码。目标是在 WinDbg 进程内以 C++ DLL 扩展方式提供最小 MCP HTTP 能力，并满足以下硬约束：

- 仅使用 C++ 与 Windows/WinDbg 自带库；禁止第三方依赖。
- 使用 CMake 管理构建。
- 提供最小可用 MCP 方法与 `windbg.eval`。
- 将单元测试规范纳入交付门禁。

参考实现来自 `3rdpart/WinDbg-Samples/`，协议约束来自 `docs/MCP-transports.md`。

## 目标 / 非目标

**目标：**
- 构建可 `.load` 的 WinDbg 扩展 DLL。
- 在扩展进程内启动本地 HTTP 服务，处理 JSON-RPC POST。
- 支持 `initialize`、`tools/list`、`tools/call`。
- 提供 `windbg.eval`，执行任意 WinDbg 命令并返回文本输出。
- 提供可运行的单元测试（针对可单测逻辑模块）。

**非目标：**
- 不实现完整通用 JSON 解析器。
- 不实现 SSE 流式返回。
- 不实现生产级认证与多租户会话管理。
- 不覆盖全部 MCP 扩展特性（如 prompts/resources 等）。

## 决策

### 决策 1：扩展形态采用经典 DbgEng 扩展导出

- 采用 `DebugExtensionInitialize` / `DebugExtensionUninitialize` / `DebugExtensionCanUnload` 导出函数。
- 原因：与微软样例一致，集成路径最短，且可直接在 WinDbg 中加载。
- 备选方案：Data Model COM 风格扩展。未采用原因：样板代码更多，不利于 MVP 快速落地。

### 决策 2：HTTP 服务使用 WinSock + 手写最小 HTTP 解析

- 采用 WinSock2 监听 `127.0.0.1`，处理最小请求格式：`POST /mcp`。
- 原因：满足“无第三方依赖”约束，且足以支撑 MVP。
- 备选方案：引入第三方 HTTP 库。未采用原因：违反依赖约束。

### 决策 3：JSON-RPC 处理采用受限字段提取而非完整 JSON AST

- 对已知 MCP 最小方法集合实现受限字段解析（`jsonrpc`、`id`、`method`、`params` 关键字段）。
- 原因：降低实现复杂度，保证可测试。
- 备选方案：完整 JSON 解析器。未采用原因：在无第三方依赖前提下实现成本高，超出本次 MVP 目标。

### 决策 4：WinDbg 命令执行通过 `IDebugControl::Execute` + 输出回调捕获

- 通过扩展内部 `IDebugClient` / `IDebugControl` 调用 `Execute`。
- 使用自定义输出回调对象收集文本输出并返回给 `windbg.eval`。
- 原因：与 DbgEng 原生能力一致，避免外部进程注入或脚本桥接。
- 备选方案：启动外部 WinDbg 控制进程。未采用原因：架构更重，且不满足“插件内实现”目标。

### 决策 5：测试策略采用“可单测核心 + 适配层薄封装”

- 将 HTTP 解析、JSON-RPC 路由、MCP 响应组装做成纯 C++ 模块并单元测试。
- WinDbg/Socket 相关操作封装到薄适配层，通过接口注入以便替身测试。
- 原因：满足单元测试约束并保持平台相关代码可控。
- 备选方案：仅做集成手测。未采用原因：不满足测试门禁约束。

## 风险 / 权衡

- [受限 JSON 解析兼容性有限] -> 明确只承诺 MCP 最小请求形态；异常输入返回标准错误。
- [WinDbg API 调用可能受线程上下文影响] -> 命令执行串行化并提供最小互斥保护。
- [HTTP 实现简化导致协议覆盖不完整] -> 在 README 中明确 MVP 支持范围与不支持项。
- [扩展卸载时后台线程未退出风险] -> 引入可中断监听循环与确定性 stop/join 流程。

## 迁移计划

1. 新增 CMake 工程、源码目录、测试目录。
2. 实现最小可运行 DLL 与核心模块。
3. 通过 CTest 验证纯逻辑模块。
4. 在 WinDbg 手动加载并执行最小调用验证。

回滚策略：移除新增 DLL 与 HTTP 入口代码，保留仓库结构不影响其他模块（当前无存量业务代码）。

## 未决问题

- 默认监听端口是否固定为 8080，或通过扩展命令参数配置。
- 是否在后续迭代补充 MCP 会话头（`MCP-Session-Id`）管理。
