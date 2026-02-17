## 1. MCP 工具元数据串行约束

- [x] 1.1 在 `src/mcp/json_rpc.cpp` 更新 `windbg.eval.description`，明确必须串行逐条执行命令
- [x] 1.2 在 `src/mcp/json_rpc.cpp` 更新 `inputSchema.properties.command.description`，明确需等待上一条执行完成后再提交下一条
- [x] 1.3 校验 `tools/list` 输出 JSON 结构与现有字段保持兼容（工具名、schema、required、additionalProperties 不变）

## 2. 自动化测试覆盖

- [x] 2.1 更新 `tests/unit_tests.cpp` 的 `TestToolsList`，新增对串行执行提示的断言
- [x] 2.2 更新 `tests/mcp_output_spec_test.py` 的 `case_tools_list`，断言工具描述与 `command` 字段描述包含串行约束语义
- [x] 2.3 运行 `ctest --test-dir build -C Debug --output-on-failure`，确认现有与新增断言全部通过

## 3. 最小验收

- [x] 3.1 手工发送一次 `tools/list` 请求，确认返回的 `windbg.eval` 描述可直接指导 AI 串行执行
- [x] 3.2 手工发送一次 `tools/call`（含合法 `command`），确认返回包络与错误语义无回归
