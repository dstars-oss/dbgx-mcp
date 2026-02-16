## 为什么

当前项目尚未具备可远程调用 WinDbg 的能力，无法通过标准 MCP 客户端自动化执行调试命令。
我们需要一个最小可运行实现：以 C++ WinDbg DLL 插件为宿主，提供 HTTP 版 MCP 接口，并暴露基础 `windbg.eval` 能力。

## 变更内容

- 新增 C++ WinDbg 扩展 DLL，可由 WinDbg 直接加载。
- 在插件内实现最小 HTTP MCP 服务，默认仅监听 `127.0.0.1`。
- 实现最小 MCP JSON-RPC 能力：`initialize`、`tools/list`、`tools/call`。
- 提供 `windbg.eval` 工具，接收任意 WinDbg 命令并返回文本输出。
- 使用 CMake 管理工程与构建流程，不引入第三方依赖。
- 增加基础文档（构建、加载、调用示例）与单元测试工程。

## 功能 (Capabilities)

### 新增功能
- `windbg-http-mcp-eval`: 通过 WinDbg 插件在本地提供 MCP HTTP 接口，并支持调用 `windbg.eval` 执行命令。

### 修改功能

## 影响

- `CMakeLists.txt`: 定义 WinDbg 扩展 DLL 与测试目标。
- `src/`: WinDbg 扩展入口、HTTP 处理、MCP 路由、命令执行适配。
- `tests/`: 对 MCP 路由与命令执行抽象层进行单元测试。
- `README.md`: 记录构建、加载、运行与调用方式。
- `docs/MCP-transports.md`: 作为传输协议实现参考。
- `3rdpart/WinDbg-Samples/`: 作为 WinDbg 扩展接口实现参考。

## 约束

- 必须使用 C++ 开发 WinDbg DLL 插件。
- 禁止第三方依赖，仅允许 Windows 系统库与 WinDbg 提供接口。
- 必须使用 CMake 管理构建，并通过 CTest 执行单元测试。
- HTTP 传输遵循 `docs/MCP-transports.md` 的最小可用子集。
- 每条规格场景至少对应一个单元测试用例。
