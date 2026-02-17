# build-artifact-name 规范

## ADDED Requirements

### 需求:构建产物必须统一命名为 dbgx-mcp.dll
系统必须将 WinDbg MCP 插件的主 DLL 产物命名为 `dbgx-mcp.dll`，并保持构建与分发版本一致。

#### 场景: Release 构建产物命名一致
- **当** 开发者执行 Release 构建
- **那么** 输出 DLL 文件名必须为 `dbgx-mcp.dll`。

#### 场景: Debug 构建产物命名一致
- **当** 开发者执行 Debug 构建
- **那么** 输出 DLL 文件名必须为 `dbgx-mcp.dll`（或明确映射为同一主要发布名）。

### 需求:构建交付文档必须引用 dbgx-mcp.dll
系统必须在构建与部署文档中使用 `dbgx-mcp.dll` 作为扩展加载文件名。

#### 场景:文档示例命名一致
- **当** 用户查看发布文档中的示例
- **那么** 文档中 `.load` 命令、路径示例与实际产物名必须一致。

## MODIFIED Requirements

## REMOVED Requirements
