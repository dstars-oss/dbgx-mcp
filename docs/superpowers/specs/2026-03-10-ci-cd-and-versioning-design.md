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
- DbgEng SDK（`DbgEng.h` + `dbgeng.lib`）属于 Windows SDK 的 `um` 路径，在标准 Windows SDK 安装中可用。`windows-latest` runner 预装了完整 Windows SDK。
- **验证措施：** CI workflow 在 CMake 配置步骤前增加一个诊断步骤，检查 `DbgEng.h` 是否可被编译器找到。如果缺失则提前失败并给出明确错误信息。
- **回退方案：** 如果未来 runner 镜像不再包含 DbgEng SDK，可通过 `vs_BuildTools.exe --add Microsoft.VisualStudio.Component.Windows10SDK.Debugging` 显式安装调试工具组件。

第三方 Actions 依赖：
- `ilammy/msvc-dev-cmd@v1` — 配置 MSVC 环境变量，锁定 `v1` 大版本

步骤：
1. Checkout 代码
2. 配置 MSVC 环境（`ilammy/msvc-dev-cmd@v1`）
3. 验证 DbgEng SDK 可用性（编译器 include 路径检查）
4. `cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Debug` 配置
5. `cmake --build build` 编译
6. `ctest --test-dir build --output-on-failure` 运行全部测试（含 unit_tests + export 验证）
7. （仅 tag 触发时）从 tag 提取版本号，用 `gh release create v<version> --title "v<version>" build/dbgx-mcp.dll` 上传 DLL

**构建输出路径：** Ninja 是单配置生成器，DLL 输出在 `build/dbgx-mcp.dll`（不含 `Debug/` 子目录），通过 `-DCMAKE_BUILD_TYPE=Debug` 控制构建配置。

产物命名：`dbgx-mcp.dll`

### 变更 2：版本号管理

**版本来源：** Git tag（如 `v0.1.0`）作为唯一版本真相源。CMake 通过 `-DDBGX_VERSION` 接收版本号，作为唯一的版本输入变量，不使用 `project(VERSION ...)`，避免双来源混淆。

**本地开发：** 不传 `-DDBGX_VERSION` 时，CMake 默认设为 `0.0.0-dev`。

**版本注入链路：**

```
Git tag (v0.1.0)
  → CI 提取版本号 → cmake -DDBGX_VERSION=0.1.0
    → CMake:
      → set(DBGX_VERSION "0.0.0-dev" CACHE STRING "...")  # 默认值
      → 解析 MAJOR.MINOR.PATCH
      → target_compile_definitions(dbgx-mcp PRIVATE DBGX_VERSION_STRING="${DBGX_VERSION}")
      → target_compile_definitions(unit_tests PRIVATE DBGX_VERSION_STRING="${DBGX_VERSION}")
      → configure_file(src/version.rc.in ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
      → set_target_properties(dbgx-mcp PROPERTIES VERSION ${DBGX_VERSION})
```

**版本注入机制：** 使用预处理器字符串宏 `DBGX_VERSION_STRING`，通过 `target_compile_definitions` 同时注入到 `dbgx-mcp` 和 `unit_tests` 两个构建目标。

代码中使用方式：
```cpp
// json_rpc.cpp 中 serverInfo 的版本字段
"\"version\":\"" DBGX_VERSION_STRING "\""
```

**版本体现位置（三处同步）：**

1. **MCP `initialize` 响应** — `serverInfo.version` 返回 `DBGX_VERSION_STRING`
2. **Windows DLL 版本资源** — `src/version.rc.in` 模板经 `configure_file` 生成 `${CMAKE_CURRENT_BINARY_DIR}/version.rc`（即 `build/version.rc`），结构如下：

```rc
// version.rc.in 模板轮廓
#include <winver.h>

VS_VERSION_INFO VERSIONINFO
  FILEVERSION    @DBGX_VERSION_MAJOR@,@DBGX_VERSION_MINOR@,@DBGX_VERSION_PATCH@,0
  PRODUCTVERSION @DBGX_VERSION_MAJOR@,@DBGX_VERSION_MINOR@,@DBGX_VERSION_PATCH@,0
BEGIN
  BLOCK "StringFileInfo"
  BEGIN
    BLOCK "040904b0"
    BEGIN
      VALUE "FileDescription",  "dbgx-mcp WinDbg Extension"
      VALUE "FileVersion",      "@DBGX_VERSION@"
      VALUE "InternalName",     "dbgx-mcp"
      VALUE "OriginalFilename", "dbgx-mcp.dll"
      VALUE "ProductName",      "dbgx-mcp"
      VALUE "ProductVersion",   "@DBGX_VERSION@"
    END
  END
  BLOCK "VarFileInfo"
  BEGIN
    VALUE "Translation", 0x0409, 1200
  END
END
```

3. **CMake `VERSION` 属性** — `set_target_properties(dbgx-mcp PROPERTIES VERSION ${DBGX_VERSION})`

**约束：** 严格零第三方依赖，仅使用 Windows SDK / WinDbg SDK。CI 管线中的 GitHub Actions（`ilammy/msvc-dev-cmd@v1`）不属于 C++ 代码依赖。
