# 灾后重构执行单（明天给 AI 用）

## 项目现状速记
- 构建已挂：`ResultWindow` 析构函数和 `openTagDialog` 未定义导致 LNK2019/LNK1120。
- 代码集中在少数大文件（`SummaryWindow.cpp` ~34k、`App.cpp` ~17k 等），耦合高、上下文缺失，易被 AI 重写时破坏。
- 现有重构方向见 `REFACTOR_PLAN.md`，但尚未落地目录拆分、接口化、日志/测试。

## AI 交付的硬约束（必须在提示中声明）
- **单一改动点**：每轮只允许修改列出的函数/类，禁止整文件重写；保持未提及代码不动。
- **头/源同步**：凡新增/改函数，必须同时给出 `.h` 与 `.cpp`，并确认签名一致；带信号/槽的类必须包含 `Q_OBJECT`。
- **契约优先**：先声明/锁定 DTO 和接口（如 `TranslationEntry`, `AppConfig`, `IApiProvider`, `IHistoryStore`），字段名不可改；UI 只依赖接口，不直连实现。
- **信号槽优先**：跨模块通信一律用 signal/slot 或接口回调，不要在 UI 内直接调用服务实现；构造函数内逻辑 <20 行，超出拆成私有方法。
- **分层隔离**：UI 只管展示/交互；服务层处理 I/O、网络、存储；平台特定逻辑放 `platform/`；WebView 资源从模板读取，C++ 仅填数据。
- **增量提交**：每完成一个步骤（构建通过/功能跑通）即 git commit，避免回退困难。

## 明日执行步骤（顺序不可跳）
1) **先救火（构建通过）**
   - 在 `ResultWindow.cpp` 实现析构函数和 `openTagDialog`，与 `.h` 签名一致；确认类含 `Q_OBJECT`。
   - 本地编译一次（Release/Debug均可），记录新的构建日志。

2) **仅做目录重排（不改逻辑）**
   - 按 `REFACTOR_PLAN.md` 的目标布局创建目录：`src/app`, `src/services`, `src/ui/windows|widgets|web`, `src/platform`, `src/models`, `src/utils`, `src/config`.
   - 逐文件移动并修正包含路径/CMake 引用；确认资源/rc 仍能被 AUTOMOC/RC 找到。

3) **锁定核心契约**
   - 提炼/集中数据结构到 `src/models/`（`TranslationEntry`, `AppConfig`, `Tag`, `HistoryItem`）。
   - 定义接口：`IApiProvider`（buildRequest/parseResponse）、`IHistoryStore`（load/save/delete/batch）、`IHotkeyService`、`IConfigStore`。
   - 生成一份接口/DTO 对照表，写入 `docs/contracts.md`（字段名/类型/职责），后续修改需更新该表。

4) **服务层下沉与解耦**
   - 将 API/历史/翻译逻辑移入 `src/services/`，UI 通过接口使用；平台相关（全局热键、Win32）放 `src/platform/`。
   - History I/O 加队列包装（Qt Concurrent/单线程队列皆可），避免 UI 线程直接写盘。

5) **UI 分拆与信号槽粘合**
   - 将 `ResultWindow`, `SummaryWindow`, `ConfigDialog`, `PreviewCard`, `ScreenshotTool` 拆分成：纯 UI（窗口/控件）+ 数据适配器（连接接口的桥接类）。
   - 所有跨窗口动作改为：UI 发信号 -> 服务层处理 -> 服务层发信号 -> UI 刷新；避免窗口之间直接互调。

6) **WebView 与资源模板化**
   - 将 HTML/JS/CSS 拆到 `assets/templates/...`，C++ 只做占位符替换或小片段注入；禁止在 C++ 拼长字符串。
   - 缩小单个模板体积，按窗口/功能拆分；WebView 包装类放 `ui/web/`。

7) **日志与测试兜底**
   - 引入 `QLoggingCategory`（api/history/ui/hotkey/webview），配置 debug 模式写文件且可调级别。
   - 生成烟雾测试清单：构建、启动、截图、翻译、结果窗口翻页/标签、归档筛选/删除、配置保存/切换、热键注册失败提示。
   - 若时间允许，先写 2-3 个 QtTest/GoogleTest（ConfigManager、History store、Api provider stub）。

## 每个功能的提示模板（给 AI 用，防“打地鼠”）
- 先声明范围：“只改 {文件/函数}，其他保持不变；不可改接口/字段名；必须同步 .h/.cpp。”
- 提供契约：“使用 `TranslationEntry` 字段 {..}，不可新增/删除。”
- 明确通信方式：“事件通过信号 X -> 槽 Y，禁止直接调用 Z。”
- 完成后要求：“给出改动 diff 摘要 + 待运行的最小验证步骤（命令/手动操作）。”

## 明日优先级清单
- [ ] 修复 ResultWindow 链接错误并确认构建通过。
- [ ] 完成目录搬迁与 CMake 修正（无逻辑变动）。
- [ ] 输出 `docs/contracts.md`（DTO/接口对照表）。
- [ ] 抽离 API/History 服务到 `services/`，UI 通过接口访问。
- [ ] 为主要窗口建立信号槽桥接，移除窗口间直接调用。
- [ ] 将 WebView 模板拆分到 `assets/templates`，C++ 仅填数据。
