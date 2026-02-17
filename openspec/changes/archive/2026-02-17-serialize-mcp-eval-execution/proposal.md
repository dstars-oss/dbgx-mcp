## 为什么

当前 `windbg.eval` 的 MCP 工具描述只说明“执行命令并返回文本”，没有明确告诉 AI 客户端必须按顺序逐条执行命令。实际调试中命令常存在前后依赖，若 AI 并行发起多个 `tools/call`，会导致上下文错位、结果难以解释与排障效率下降。

## 变更内容

- 增强 `windbg.eval` 在 `tools/list` 返回中的描述与参数说明，明确“必须串行执行、上一条完成后再发下一条”的调用约束。
- 为串行约束补充可测试的协议级行为定义（面向工具元数据），确保不同 AI 客户端都能接收到一致指引。
- 补充测试与验收用例，验证工具定义中串行提示持续存在且不破坏既有 JSON-RPC 结构与调用语义。
- 不引入 BREAKING 变更：`windbg.eval` 工具名、输入字段与 `tools/call` 结果结构保持兼容。

## 功能 (Capabilities)

### 新增功能

<!-- 无 -->

### 修改功能

- `windbg-http-mcp-eval`: 调整 `windbg.eval` 工具元数据规范，新增“串行执行 eval 命令”的明确约束与验证场景。

## 影响

- 代码范围：`src/mcp/json_rpc.cpp`（`tools/list` 中 `windbg.eval` 描述与输入字段说明）。
- 测试范围：`tests/unit_tests.cpp`、`tests/mcp_output_spec_test.py`（补充/更新串行提示断言）。
- 协议影响：仅增强工具描述文本与文档化约束，不改变现有方法集合和响应包络。
