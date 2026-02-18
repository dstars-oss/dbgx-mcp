## 1. 技能结构与命名统一

- [x] 1.1 创建并保留 `.codex/skills/dbgx-skills/SKILL.md` 作为唯一通用入口
- [x] 1.2 移除旧的单场景技能目录，避免与 `dbgx-skills` 重复

## 2. 技能内容落地

- [x] 2.1 在 `dbgx-skills` 中编写 `dbgx-mcp` 命令执行策略（先轻量后高风险、分步执行）
- [x] 2.2 在 `dbgx-skills` 中编写 TTD 跳转标准流程（`TimeStart` 查询 + `!tt` 跳转 + `!tt` 校验）

## 3. 变更收口

- [x] 3.1 确认 OpenSpec 产出物（proposal/design/specs/tasks）与 capability `dbgx-skills` 对齐
- [x] 3.2 明确本变更为文档型沉淀，**无需测试**（不执行自动化与手工测试）
