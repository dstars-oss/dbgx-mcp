## 1. 工程与构建基础

- [x] 1.1 建立 CMake 工程骨架（DLL 目标、CTest、测试目标）
- [x] 1.2 创建源码目录结构（扩展入口、MCP 核心、WinDbg 适配层）
- [x] 1.3 确认仅使用系统库/WinDbg 接口，不引入第三方依赖

## 2. WinDbg 扩展与命令执行适配

- [x] 2.1 实现扩展导出入口（初始化/卸载/可卸载判断）
- [x] 2.2 实现 `IDebugControl::Execute` 封装与输出捕获
- [x] 2.3 实现线程安全命令执行通道（用于 `windbg.eval`）

## 3. HTTP MCP 最小实现

- [x] 3.1 实现本地 HTTP 监听（`127.0.0.1`）与最小请求解析
- [x] 3.2 实现 JSON-RPC 路由：`initialize`、`tools/list`、`tools/call`
- [x] 3.3 实现 `windbg.eval` 参数校验与结果返回
- [x] 3.4 实现基础安全校验（`Origin` 最小检查、错误码返回）

## 4. 测试与规范映射

- [x] 4.1 为 HTTP/JSON-RPC 路由编写单元测试（正常/异常路径）
- [x] 4.2 为 `windbg.eval` 调用路径编写单元测试（替身执行器）
- [x] 4.3 建立“规格场景 ↔ 测试用例”映射并纳入文档
- [x] 4.4 运行 `ctest` 并修复失败项

## 5. 文档与验收

- [x] 5.1 编写 `README.md`（构建、`.load`、MCP 调用示例）
- [x] 5.2 完成最小手工验收步骤记录（WinDbg 中加载并调用 `windbg.eval`）
