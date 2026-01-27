# AI-Screenshot-Translator-Cpp

一个基于 Qt 6 + WebView2 的截图翻译/归档工具，支持全局快捷键、标签管理、富文本渲染（Markdown/代码高亮/LaTeX）、多配置文件和代理。

## 功能亮点
- 全局快捷键：截图、打开归档、打开设置，可在设置中自定义。
- 截图工作流：框选区域 → 预览卡片（可拖拽、滚轮/拖动缩放、可选边框） → 可选结果窗口。
- AI 处理：支持 OpenAI / Gemini / Claude，提示词可自定义，支持代理。
- 结果视图：结果窗口可锁定置顶、翻页、编辑 Markdown、查看原图、标签弹窗、快捷键导航。
- 归档视图：筛选（日期、标签），批量选择删除/加减标签，键盘切换查看/编辑/截图预览。
- 历史存储：本地 `storage/history.json` + `storage/images/*`，删除图片会同步删除条目。
- 配置与多配置文件：配置保存在 `%AppData%/AI-Screenshot-Translator-Cpp/profiles/*.json`，支持导入/导出/复制/重命名。
- 日志：可在设置里开启 debug 模式，输出到工作目录 `debug.log`。

## 目录结构（当前主要目录）
- `src/` 核心代码：App、配置、服务（API/历史/翻译）、UI 窗口与小部件、平台封装（GlobalHotkey）、WebView 封装。
- `assets/` 静态资源与前端库（marked/highlight/katex、字体、图标）。
- `webview2_pkg/` WebView2 SDK 依赖。
- `build/` 构建输出（本地生成）。
- `REFACTOR_PLAN.md` 重构路线图。

## 环境与依赖
- Windows + MSVC（推荐 VS 2022 工具链）。
- CMake ≥ 3.16。
- Qt 6（Widgets、Network、Gui、Core 组件）。
- WebView2 Runtime（Windows 11 默认有，或单独安装）。
- 已自带 WebView2 SDK 包（`webview2_pkg`）和前端库。

### WebView2 包说明
- 仓库已内置官方 NuGet 包 **Microsoft.Web.WebView2 1.0.2903.40**（原样解压，保留 LICENSE/NOTICE），克隆后可直接编译。
- 需要精简仓库且只支持 x64 时，可只保留 `build/native/x64/WebView2Loader.dll(.lib)` 与 `WebView2.h/.idl/.tlb`；否则保持整包最稳妥。
- 若读者拿到的仓库未包含 `webview2_pkg/`，请执行 `nuget install Microsoft.Web.WebView2 -Version 1.0.2903.40 -OutputDirectory third_party`，然后将生成的 `.nupkg` 解压到仓库根目录的 `webview2_pkg/`。

## 本地构建
1. 确认已安装 Qt 6（并设置好 `CMAKE_PREFIX_PATH` 或在 VS 开发者命令行里使用 `-DCMAKE_PREFIX_PATH=".../Qt/6.x/msvc2019_64/lib/cmake"`）。
2. 生成并构建：
   ```powershell
   cmake -S . -B build -G "Visual Studio 17 2022" -A x64
   cmake --build build --config Release
   ```
3. 运行可执行文件：
   ```powershell
   .\build\Release\AI-Screenshot-Translator-Cpp.exe
   ```

## 运行与配置
- 首次启动若未配置 API Key 会自动弹出设置窗口。
- 托盘菜单：截图、归档、设置、退出。
- 默认存储：`./storage`（可在设置里改为绝对路径），图片保存在 `images/`，历史为 `history.json`。
- 默认快捷键（可在设置里修改）：
  - 截图：`ctrl+alt+s`
  - 归档：`alt+s`
  - 结果窗口导航：上一页 `z` / 下一页 `x`
  - 结果窗口标签：`t`
  - 归档视图：编辑 `e`、查看切换 `r`、截图预览 `s`，加粗/下划线/高亮分别为 `ctrl+b / ctrl+u / ctrl+h`

## 调试与日志
- 设置中开启 “Enable Debug Mode” 后，会记录日志到工作目录 `debug.log`，App 启动/异常/网络错误会写入。
- WebView 模板和资源已本地打包，无需联网；网络请求仅用于调用选定的 AI Provider。

## 可选：使用 Umami 统计（DAU/MAU/使用时长）
程序支持在启动 **5 秒后** 异步上报 Umami 事件（不会阻塞 UI）。你可以在仓库根目录放一个 `.env`（不要提交到 GitHub）：

```dotenv
# Umami 网站 ID（必填）
website_uuid=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx

# Umami 服务地址（可选，默认 https://umami.diraw.top）
umami_host=https://umami.example.com

# 客户端固定 ID（可选；不填则会在 %AppData% 下自动生成并持久化）
client_uuid=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
```

上报事件包括：`session_start`、`heartbeat`（每 60 秒）、`session_end`，并发送一次 pageview 以便在 Umami 中直接看到会话/访客指标。

## 打包到 Release 目录（收敛依赖）
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

# 4) 可选：附带默认存储目录
cmake -E copy_directory storage build/Release/storage
```
完成后 `build/Release/` 下的 exe + DLL + assets 即为可分发包。

## 已知注意点
- 全局热键仅在 Windows 下实现；注册失败会在日志中提示。
- WebView2 初始化失败时，请确认已安装 WebView2 Runtime，并保持 `webview2_pkg` 完整。
- 删除 `storage/images/*` 会同步删除对应 JSON 条目（HistoryManager 行为）。

## 相关文档
- 重构计划：`REFACTOR_PLAN.md`
- （建议补充）调试指南：`docs/DEBUGGING.md`、开发环境脚本 `scripts/dev_env.ps1`（可按需添加）。

## 许可
项目包含的 `webview.h` 遵循 MIT 许可，其余代码请根据仓库实际许可使用。

