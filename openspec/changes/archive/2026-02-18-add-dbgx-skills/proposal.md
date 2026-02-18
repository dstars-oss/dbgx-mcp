## 为什么

当前项目缺少统一的 `dbgx-mcp` 使用技能文档，导致 TTD 高风险命令（尤其 `dx ...SeekTo()`）在不同会话中被重复误用，出现卡住、诊断路径不一致、经验难复用的问题。现在需要把已验证的实战流程沉淀为标准技能，降低排障成本并提升稳定性。

## 变更内容

- 新增通用技能 `dbgx-skills`，覆盖 `dbgx-mcp` 的执行策略与 TTD 稳定跳转流程。
- 将已有的点状经验（仅 `SeekTo` 回避）升级为可复用的通用操作规范。
- 删除旧的单一场景技能，统一入口到 `dbgx-skills`。
- 不修改 `windbg.eval` 协议与扩展对外 API，不引入新的运行时行为变更。

## 功能 (Capabilities)

### 新增功能
- `dbgx-skills`: 提供 `dbgx-mcp` 通用操作规范，定义稳定执行流程，并明确 `SeekTo` 的替代导航方案。

### 修改功能
- `<none>`: 无现有 capability 需要修改。

## 影响

- 受影响目录：`.codex/skills/`
- 新增文档：`.codex/skills/dbgx-skills/SKILL.md`
- 移除文档：旧单点技能目录（已被通用技能替代）
- 对运行时代码、HTTP/MCP 协议、依赖版本无直接影响
