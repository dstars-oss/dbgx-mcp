## 1. 准备

- [x] 1.1 检查当前 CMake 与构建脚本中的 DLL 输出命名配置（Release/Debug）
- [x] 1.2 识别并列出需要同步替换的文档与示例中旧的 DLL 文件名引用

## 2. 实现

- [x] 2.1 将 DLL 输出主名改为 `dbgx-mcp.dll`，并确保各配置共享统一命名
- [x] 2.2 更新发布/打包脚本与示例命令中的产物路径为 `dbgx-mcp.dll`
- [x] 2.3 若存在历史兼容需求，评估并记录兼容或迁移说明

- 已更新 `README.md` / `README.zh-CN.md` 的 `.load` 与 `dumpbin` 示例，并新增迁移说明：旧名称 `windbg_mcp_extension.dll` 已不再推荐使用。

## 3. 验证

- [x] 3.1 验证 Release 与 Debug 构建产物均为 `dbgx-mcp.dll`
- [x] 3.2 运行与加载相关文档场景验证，确认 `.load` 示例可直接使用新文件名
- [x] 3.3 将关键场景映射到规范，确认 `build-artifact-name` 与 `windbg-mcp-debug-observability` 需求可验证

- 3.3 映射:
  - `build-artifact-name`:
    - 需求 `构建产物必须统一命名为 dbgx-mcp.dll` -> 实现 `CMakeLists.txt` 的 `OUTPUT_NAME*` 设置
    - 需求 `构建交付文档必须引用 dbgx-mcp.dll` -> 文档更新为 `README.md` / `README.zh-CN.md`
  - `windbg-mcp-debug-observability`:
    - `请求与日志可关联加载文件名` 场景 -> `.load` 示例与迁移说明统一到 `dbgx-mcp.dll`

- 证据：构建输出显示 `Linking CXX shared library dbgx-mcp.dll`。`WinDbg` 加载验证成功：`.load D:/Repos/Project/AI-Native/dbgx-mcp/.out/dbgx-mcp.dll` 后输出
  `[windbg-mcp] HTTP MCP server listening on http://127.0.0.1:5678/mcp`。
