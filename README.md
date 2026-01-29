# AI 截图翻译工具 (AI Screenshot Translator)

[![Qt6](https://img.shields.io/badge/Qt-6.x-41CD52)](https://www.qt.io/) [![WebView2](https://img.shields.io/badge/WebView2-Microsoft-0078D4?logo=microsoftedge&logoColor=white)](https://developer.microsoft.com/en-us/microsoft-edge/webview2/) <img alt="GitHub release (latest by date)" src="https://img.shields.io/github/v/release/Diraw/AI-Screenshot-Translator"> <img alt="GitHub issues" src="https://img.shields.io/github/issues/Diraw/AI-Screenshot-Translator?color=green"> [![Downloads](https://img.shields.io/github/downloads/Diraw/AI-Screenshot-Translator/total?color=yellow)](https://github.com/Diraw/AI-Screenshot-Translator/releases/)

一个基于 **Qt 6 + WebView2** 的截图翻译/归档工具：

- 截图 → AI 翻译 → 富文本结果（Markdown/代码高亮/LaTeX）
- 结果窗口支持快捷键翻页/编辑/查看原图/标签
- 归档窗口支持筛选、批量操作与键盘导航
- 多配置文件、代理、连通性测试

> 注：目前本项目仅支持 Windows，将在未来支持跨平台

## 适用场景（想解决的痛点）

1. 不想用“整篇文档翻译”那种笨重工作流
2. PDF 复制文本后公式（LaTeX）经常乱或不可用
3. 扫描版 PDF 无法复制文本
4. 划词翻译/截图翻译很多无法正确识别和渲染公式

## 功能亮点

- 全局快捷键：截图、打开归档、打开设置（可自定义）
- 截图工作流：框选区域 → 预览卡片（拖拽/缩放/可选边框） → 结果窗口
- AI Provider：OpenAI / Gemini / Claude（可扩展），提示词可自定义，支持代理
- 结果窗口：锁定置顶、翻页、编辑 Markdown、查看原图、标签弹窗、快捷键导航
- 归档窗口：日期/标签筛选，批量选择删除/加减标签，键盘切换查看/编辑/截图预览
- 本地历史：`storage/history.json` + `storage/images/*`（删除图片会同步删除条目）
- 多配置文件：`%AppData%/AI-Screenshot-Translator-Cpp/profiles/*.json`，支持导入/导出/复制/重命名
- 调试日志：设置里开启 Debug Mode，输出到工作目录 `debug.log`

## 演示视频

## 快速上手

1. 启动程序（Release 包或自行构建）。
2. 首次启动会自动弹出设置窗口：
   - 选择 Provider
   - 填写 API Key
   - （可选）设置 Base URL / Endpoint / Proxy
3. 使用托盘菜单或快捷键开始截图翻译。

### 默认快捷键

- 截图：`ctrl+alt+s`
- 打开归档：`alt+s`
- 结果窗口：上一页 `z` / 下一页 `x`；标签 `t`
- 归档窗口：编辑 `e`、查看切换 `r`、截图预览 `s`
- 编辑辅助：加粗/下划线/高亮：`ctrl+b / ctrl+u / ctrl+h`

## API 配置说明

### Base URL + Endpoint 是如何拼接的？

- 请求地址统一按 `Base URL + Endpoint` 组合。
- 默认 Endpoint（可自行改）：
  - OpenAI：`/chat/completions`
  - Gemini：`/v1beta`
  - Claude：`/v1/messages`

> 注意：如果你在 Base URL 里已经包含了版本路径（例如 `.../v1`），Endpoint 就不要再写 `/v1/...`，避免重复路径

### 测试连通性

设置页提供“测试”按钮，用于快速验证：

- 代理是否可连接（若配置了 Proxy）
- API 端点是否可访问（基于当前 Base URL + Endpoint）

## 目录结构

- `src/` 核心代码：App、配置、服务（API/历史/翻译）、UI 窗口与小部件、平台封装（GlobalHotkey）、WebView 封装
- `assets/` 静态资源与前端库（marked/highlight/katex、字体、图标、模板等）
- `webview2_pkg/` WebView2 SDK 依赖
- `build/` 构建输出（本地生成）

## 环境与依赖

- Windows + MSVC（推荐 VS 2022 工具链）
- CMake ≥ 3.16
- Qt 6（Widgets、Network、Gui、Core）
- WebView2 Runtime（Windows 11 默认有，Windows 10 可能需要单独安装）
- 仓库内置 WebView2 SDK（`webview2_pkg`）以及前端库（`assets/libs`）

### WebView2 包说明

- 仓库已内置官方 NuGet 包 **Microsoft.Web.WebView2 1.0.2903.40**（已精简，保留 `build/native/x64/WebView2Loader.dll(.lib)`、`WebView2.h/.idl/.tlb` 和 `LICENSE/NOTICE`），克隆后可直接编译。
- 若仓库未包含 `webview2_pkg/`，可执行：
  ```powershell
  nuget install Microsoft.Web.WebView2 -Version 1.0.2903.40 -OutputDirectory third_party
  ```
  然后将生成的 `.nupkg` 解压到仓库根目录的 `webview2_pkg/`。

## 本地构建

1. 安装 Qt 6，并设置 `CMAKE_PREFIX_PATH`（或在 VS 开发者命令行中使用 `-DCMAKE_PREFIX_PATH=".../Qt/6.x/msvc2019_64/lib/cmake"`）。
2. 生成并构建：
   ```powershell
   cmake -S . -B build -G "Visual Studio 17 2022" -A x64
   cmake --build build --config Release
   ```
3. 运行：
   ```powershell
   .\build\Release\AI-Screenshot-Translator-Cpp.exe
   ```

## 打包

构建后可将运行所需依赖统一放到 `build/Release/`：

```powershell
# 1) 构建
cmake --build build --config Release

# 2) 部署 Qt 运行时（将 QTDIR 替换为你的 Qt 安装路径；--compiler-runtime 会把 MSVC 运行库也拷进来）
& "$Env:QTDIR\bin\windeployqt.exe" --release --compiler-runtime --no-translations build\Release\AI-Screenshot-Translator-Cpp.exe

# 如果看到 windeployqt 提示找不到 Visual Studio（如 VCINSTALLDIR 未设置），请在
# “x64 Native Tools Command Prompt/Developer PowerShell for VS 2022” 中运行上述命令，
# 这样才能把 MSVC 运行库 DLL 一并复制到 Release 目录。

# 3) 复制资产与 WebView2 Loader
cmake -E copy_directory assets build/Release/assets
cmake -E copy_if_different webview2_pkg/build/native/x64/WebView2Loader.dll build/Release/
```

完成后 `build/Release/` 下的 exe + DLL + assets 即为可分发包。

## 常见问题（FAQ）

### 1) 测试连通但实际请求失败？

- 重点检查 Base URL 和 Endpoint 是否重复包含了版本路径（例如 `/v1`）。
- 如果你配置的是“兼容 OpenAI”的第三方服务，Endpoint 可能不是 `/chat/completions`，请按服务文档调整。

### 2) WebView2 相关问题

- Windows 10 可能需要单独安装 WebView2 Runtime。
- 如果窗口空白/崩溃，先确认系统 WebView2 Runtime 正常，再检查打包时 `WebView2Loader.dll` 是否拷贝到 exe 同目录。

## 许可

项目包含的 `webview.h` 遵循 MIT 许可，其余代码请根据仓库实际许可使用。

## 其他

喜欢本项目不妨点个 star 支持一下。

[![Star History Chart](https://api.star-history.com/svg?repos=Diraw/AI-Screenshot-Translator&type=Date)](https://www.star-history.com/#Diraw/AI-Screenshot-Translator&Date)
