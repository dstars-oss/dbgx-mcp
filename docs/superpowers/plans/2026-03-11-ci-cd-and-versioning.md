# CI/CD + 版本管理 Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 dbgx-mcp 添加 GitHub Actions CI/CD 管线和版本号管理，让每次 push 自动构建测试，tag 推送时自动发布预编译 DLL。

**Architecture:** CI workflow 在 `windows-latest` runner 上使用 MSVC + Ninja 构建。版本号由 Git tag 通过 `-DDBGX_VERSION` 注入 CMake，驱动预处理器宏 `DBGX_VERSION_STRING`（代码引用）、`version.rc`（DLL 文件属性）和 CMake `VERSION` 属性三处同步。

**Tech Stack:** GitHub Actions, CMake 3.20+, MSVC, Ninja, Windows SDK (DbgEng), Windows RC 资源编译器

**Spec:** `docs/superpowers/specs/2026-03-10-ci-cd-and-versioning-design.md`

---

## File Structure

| Action | Path | Responsibility |
|--------|------|----------------|
| Create | `.github/workflows/ci.yml` | CI/CD workflow：构建、测试、发布 |
| Create | `src/version.rc.in` | DLL 版本资源模板 |
| Modify | `CMakeLists.txt` | 版本变量、compile definitions、configure_file、rc 链接 |
| Modify | `src/mcp/json_rpc.cpp:53` | `serverInfo.version` 改用 `DBGX_VERSION_STRING` 宏 |
| Modify | `tests/unit_tests.cpp:175` | 添加版本号测试 |

---

## Chunk 1: 版本号管理

### Task 1: CMake 版本变量基础设施

**Files:**
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 在 CMakeLists.txt 中添加版本变量和解析逻辑**

在 `set(CMAKE_CXX_EXTENSIONS OFF)` 行之后添加：

```cmake
# --- Version management ---
set(DBGX_VERSION "0.0.0-dev" CACHE STRING "Project version (injected by CI from git tag)")

# Parse MAJOR.MINOR.PATCH for version.rc
string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" _version_match "${DBGX_VERSION}")
if(_version_match)
  set(DBGX_VERSION_MAJOR "${CMAKE_MATCH_1}")
  set(DBGX_VERSION_MINOR "${CMAKE_MATCH_2}")
  set(DBGX_VERSION_PATCH "${CMAKE_MATCH_3}")
else()
  set(DBGX_VERSION_MAJOR 0)
  set(DBGX_VERSION_MINOR 0)
  set(DBGX_VERSION_PATCH 0)
endif()
```

- [ ] **Step 2: 为 dbgx-mcp 和 unit_tests 目标添加版本宏定义**

在现有的 `target_compile_definitions(dbgx-mcp PRIVATE ...)` 块中追加 `DBGX_VERSION_STRING`：

```cmake
target_compile_definitions(dbgx-mcp PRIVATE
  WIN32_LEAN_AND_MEAN
  NOMINMAX
  DBGX_VERSION_STRING="${DBGX_VERSION}"
)
```

在 `target_link_libraries(unit_tests PRIVATE ws2_32)` 行之后添加：

```cmake
target_compile_definitions(unit_tests PRIVATE
  DBGX_VERSION_STRING="${DBGX_VERSION}"
)
```

- [ ] **Step 3: 为 dbgx-mcp 目标添加 VERSION 属性**

在现有的 `set_target_properties(dbgx-mcp PROPERTIES ...)` 块中追加 `VERSION`：

```cmake
set_target_properties(dbgx-mcp PROPERTIES
  OUTPUT_NAME ${WINDBG_EXTENSION_OUTPUT_NAME}
  OUTPUT_NAME_DEBUG ${WINDBG_EXTENSION_OUTPUT_NAME}
  OUTPUT_NAME_RELEASE ${WINDBG_EXTENSION_OUTPUT_NAME}
  VERSION ${DBGX_VERSION}
)
```

- [ ] **Step 4: 验证 CMake 配置不报错**

Run: `cmake -S . -B build-test -G "Ninja" -DDBGX_VERSION=1.2.3 2>&1 | head -20`（macOS 上只验证配置语法，不需要构建成功）

Expected: CMake 配置步骤不产生错误（实际构建在 Windows 上才能完成）

- [ ] **Step 5: 提交**

```bash
git add CMakeLists.txt
git commit -m "build: add DBGX_VERSION variable and compile definitions"
```

---

### Task 2: DLL 版本资源模板

**Files:**
- Create: `src/version.rc.in`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 创建 `src/version.rc.in` 模板**

```rc
#include <winver.h>

VS_VERSION_INFO VERSIONINFO
  FILEVERSION    @DBGX_VERSION_MAJOR@,@DBGX_VERSION_MINOR@,@DBGX_VERSION_PATCH@,0
  PRODUCTVERSION @DBGX_VERSION_MAJOR@,@DBGX_VERSION_MINOR@,@DBGX_VERSION_PATCH@,0
  FILEFLAGSMASK  VS_FFI_FILEFLAGSMASK
  FILEFLAGS      0x0L
  FILEOS         VOS_NT_WINDOWS32
  FILETYPE       VFT_DLL
  FILESUBTYPE    VFT2_UNKNOWN
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

- [ ] **Step 2: 在 CMakeLists.txt 中添加 configure_file 和 RC 源文件**

在版本解析逻辑之后（Task 1 Step 1 添加的代码块之后）添加：

```cmake
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/src/version.rc.in"
  "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
  @ONLY
)
```

在 `add_library(dbgx-mcp SHARED ...)` 的源文件列表末尾追加生成的 RC 文件：

```cmake
add_library(dbgx-mcp SHARED
  src/mcp/http_server.cpp
  src/mcp/io_echo.cpp
  src/mcp/json.cpp
  src/mcp/json_rpc.cpp
  src/windbg/dbgeng_command_executor.cpp
  src/dbgx-mcp.cpp
  src/dbgx-mcp.def
  "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
)
```

- [ ] **Step 3: 提交**

```bash
git add src/version.rc.in CMakeLists.txt
git commit -m "build: add DLL version resource template"
```

---

### Task 3: 版本号单元测试（TDD：先写测试）

**Files:**
- Modify: `tests/unit_tests.cpp`

- [ ] **Step 1: 在 `TestInitialize` 中添加版本号断言**

在 `TestInitialize` 函数中，`"initialize should mention windbg.eval capability"` 断言之后添加：

```cpp
  Expect(Contains(result.body, "\"version\":\"" DBGX_VERSION_STRING "\""),
         "initialize should return DBGX_VERSION_STRING in serverInfo", failures);
```

此时 `json_rpc.cpp` 仍使用硬编码 `"0.1.0"`，而 `DBGX_VERSION_STRING` 默认为 `"0.0.0-dev"`，所以该测试预期失败（TDD red phase）。

- [ ] **Step 2: 提交**

```bash
git add tests/unit_tests.cpp
git commit -m "test: verify serverInfo.version uses DBGX_VERSION_STRING (red)"
```

---

### Task 4: json_rpc.cpp 使用版本宏（TDD：让测试通过）

**Files:**
- Modify: `src/mcp/json_rpc.cpp`

- [ ] **Step 1: 将硬编码版本替换为宏**

在 `src/mcp/json_rpc.cpp` 的 `HandleInitialize()` 函数中，找到：

```cpp
"\"serverInfo\":{\"name\":\"dbgx-mcp\",\"version\":\"0.1.0\"}"
```

替换为：

```cpp
"\"serverInfo\":{\"name\":\"dbgx-mcp\",\"version\":\"" DBGX_VERSION_STRING "\"}"
```

此时 Task 3 的测试将通过（TDD green phase）。

- [ ] **Step 2: 提交**

```bash
git add src/mcp/json_rpc.cpp
git commit -m "feat: use DBGX_VERSION_STRING in MCP serverInfo response"
```

---

## Chunk 2: CI/CD + GitHub Release 管线

### Task 5: GitHub Actions CI workflow

**Files:**
- Create: `.github/workflows/ci.yml`

- [ ] **Step 1: 创建 `.github/workflows/` 目录**

```bash
mkdir -p .github/workflows
```

- [ ] **Step 2: 创建 CI workflow 文件**

```yaml
name: CI

on:
  push:
    branches: [main]
    tags: ["v*"]

jobs:
  build:
    runs-on: windows-latest

    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Setup MSVC
        uses: ilammy/msvc-dev-cmd@v1

      - name: Verify DbgEng SDK
        shell: pwsh
        run: |
          $found = $false
          foreach ($dir in ($env:INCLUDE -split ';')) {
            if ($dir -and (Test-Path (Join-Path $dir 'DbgEng.h'))) {
              Write-Host "Found DbgEng.h in $dir"
              $found = $true
              break
            }
          }
          if (-not $found) {
            Write-Error "DbgEng.h not found in INCLUDE paths. DbgEng SDK may not be installed."
            exit 1
          }

      - name: Extract version from tag
        id: version
        shell: pwsh
        run: |
          if ($env:GITHUB_REF -match '^refs/tags/v(.+)$') {
            $ver = $Matches[1]
          } else {
            $ver = "0.0.0-dev"
          }
          Write-Host "Version: $ver"
          echo "value=$ver" >> $env:GITHUB_OUTPUT

      - name: Configure
        run: cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Debug "-DDBGX_VERSION=${{ steps.version.outputs.value }}"

      - name: Build
        run: cmake --build build

      - name: Test
        run: ctest --test-dir build --output-on-failure

      - name: Upload DLL artifact
        if: startsWith(github.ref, 'refs/tags/v')
        uses: actions/upload-artifact@v4
        with:
          name: dbgx-mcp-dll
          path: build/dbgx-mcp.dll

      - name: Create GitHub Release
        if: startsWith(github.ref, 'refs/tags/v')
        env:
          GH_TOKEN: ${{ github.token }}
        run: gh release create "${{ github.ref_name }}" build/dbgx-mcp.dll --title "${{ github.ref_name }}" --generate-notes
```

- [ ] **Step 3: 提交**

```bash
git add .github/workflows/ci.yml
git commit -m "ci: add GitHub Actions build, test, and release workflow"
```

---

### Task 6: 验证整体集成

- [ ] **Step 1: 在本地验证 CMake 配置语法**

```bash
cmake -S . -B build-verify -G "Ninja" -DDBGX_VERSION=0.2.0 2>&1 | head -20
```

Expected: 无 CMake 错误（构建本身需要 Windows + MSVC）

- [ ] **Step 2: 检查生成的 version.rc 内容**

```bash
cat build-verify/version.rc
```

Expected: `FILEVERSION 0,2,0,0`、`"FileVersion", "0.2.0"`、`"ProductVersion", "0.2.0"` 均正确替换

- [ ] **Step 3: 验证不传版本号时的默认值**

```bash
cmake -S . -B build-default -G "Ninja" 2>&1 | head -20
cat build-default/version.rc
```

Expected: `FILEVERSION 0,0,0,0`、`"FileVersion", "0.0.0-dev"` 使用默认值

- [ ] **Step 4: 最终提交（如有调整）**

```bash
git add -A
git commit -m "chore: finalize CI/CD and versioning integration"
```
