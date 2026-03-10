# CI/CD + 版本管理 设计文档

## 背景

dbgx-mcp 当前缺少自动化构建管线和版本管理。用户必须从源码手动构建，无法直接获取预编译二进制。DLL 和 MCP 响应中也没有版本标识。

## 范围

本次改进包含两个变更：

1. CI/CD + GitHub Release 管线
2. 版本号管理

### 不在范围内

- Streamable HTTP / SSE 支持
- stdio 传输支持
- Changelog 自动生成
- 语义版本号自动递增
- 多配置发布（Release + Debug）— MVP 先只发 Debug

## 设计

### 变更 1：CI/CD + GitHub Release 管线

**GitHub Actions workflow（`.github/workflows/ci.yml`）：**

触发条件：
- `push` 到 `main` → 构建 + 测试（不发布）
- `push` tag `v*` → 构建 + 测试 + 创建 Release 并上传 DLL

构建环境：
- `windows-latest` runner（自带 MSVC 工具链、CMake、Ninja）
- Windows SDK 和 DbgEng SDK 在 GitHub Actions Windows runner 上已预装

步骤：
1. Checkout 代码
2. 配置 MSVC 环境（`ilammy/msvc-dev-cmd`）
3. `cmake -S . -B build -G "Ninja"` 配置
4. `cmake --build build` 编译
5. `ctest --test-dir build -C Debug --output-on-failure` 运行全部测试
6. （仅 tag 触发时）用 `gh release create` 上传 `build/Debug/dbgx-mcp.dll`

产物命名：`dbgx-mcp.dll`

### 变更 2：版本号管理

**版本来源：** Git tag（如 `v0.1.0`）作为唯一版本真相源。

**版本注入链路：**

```
Git tag (v0.1.0)
  → CI 提取版本号 → cmake -DDBGX_VERSION=0.1.0
    → CMake project(dbgx_mcp VERSION 0.1.0)
      → target_compile_definitions: DBGX_VERSION_STRING
      → configure_file: src/version.rc
      → set_target_properties: VERSION
```

**版本体现位置（三处同步）：**

1. MCP `initialize` 响应 — `serverInfo.version` 返回版本号字符串
2. Windows DLL 版本资源 — `.rc` 文件中的 `FILEVERSION` / `PRODUCTVERSION`，文件属性可见
3. CMake `VERSION` 属性 — `set_target_properties(dbgx-mcp PROPERTIES VERSION ...)`

**本地开发：** 不传 `-DDBGX_VERSION` 时，默认值为 `0.0.0-dev`。

**约束：** 严格零第三方依赖，仅使用 Windows SDK / WinDbg SDK。
