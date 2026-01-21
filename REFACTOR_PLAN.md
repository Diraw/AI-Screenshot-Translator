# Refactor Roadmap

Goals: sustainability, extensibility, easy debugging, and solid performance.

## Target layout
```
assets/
cmake/                  # optional: custom cmake modules
src/
  app/                  # App/main, tray, lifecycle, hotkey wiring
  config/               # ConfigManager, ConfigDialog, config models
  services/             # ApiClient, HistoryManager, TranslationManager, logging
  ui/
    windows/            # ResultWindow, SummaryWindow, TagDialog, etc.
    widgets/            # PreviewCard, HotkeyEdit, ScreenshotTool, etc.
    web/                # EmbedWebView wrapper, html/js templating
  platform/             # platform helpers (GlobalHotkey, Win32 pieces)
  models/               # data structs: TranslationEntry, AppConfig
  utils/                # helpers (color parsing, paths, serialization)
tests/
docs/
```

## Layering principles
- App shell only hosts lifecycle, dependency wiring, and hotkey registration.
- Services own logic (network, storage, translations, logging) and are UI-agnostic.
- UI consumes service interfaces only; cross-window data flows through services.
- Platform-specific code is isolated in platform/ and injected via interfaces.
- Models are data-only DTOs.

## Module guidance
- ApiClient: split per-provider strategies (OpenAI/Gemini/Claude) behind an interface to remove large switches.
- HistoryManager: define an IHistoryStore so JSON/filesystem can be swapped (future DB); move I/O to a worker queue to avoid UI stalls.
- WebView assembly: move HTML/JS into template files; C++ only fills placeholders. Keep per-window templates small and testable.
- GlobalHotkey: platform impl lives in platform/; UI calls an interface.
- ConfigManager: expose a read-only view plus change signals; hide file layout details.
- TranslationManager: load translations from JSON/TS resources instead of hardcoded maps.

## CMake/build
- Create libraries: app, services, ui; the executable links them. Keep root CMakeLists small.
- Put WebView2/Qt find scripts and asset copy logic in cmake/Modules.
- Asset copy/gen scripts as dedicated targets (e.g., copy_assets).

## Debuggability and observability
- Standardize logging with QLoggingCategory per module (api, history, ui, hotkey).
- Configurable log level/output path; guard file logging behind config.
- Add a lightweight debug panel (dev mode) showing config snapshot, hotkey status, WebView state, storage paths.
- Centralize error surfacing via a helper (notifyError) instead of ad-hoc QMessageBox calls.

## Performance notes
- Move disk I/O (history save/delete/load) to a background queue; batch writes when possible.
- WebView: cache embedded assets; avoid rebuilding full HTML on every update, use small JS patches or diff.
- Images: persist to disk; keep in-memory QImage/QPixmap, avoid duplicate base64 blobs.
- Hotkey callbacks should be thin; dispatch heavy work to the main loop or workers.

## Extensibility
- New provider: implement IProvider::buildRequest/parseResponse and register a factory.
- New storage backend: implement IHistoryStore and plug into HistoryManager.
- New windows/features: UI depends on interfaces; services emit signals/slots or observer hooks.
- Themes/i18n: externalize strings and style tokens; no hardcoded text in widgets.

## Testing and quality
- Add unit tests (QtTest or GoogleTest) for ConfigManager, HistoryManager, ApiClient providers.
- Test HTML/JS template generators as pure functions (input entries -> snippet).
- Add local CI script: format check (clang-format/clang-tidy optional), unit tests, build.

## Migration steps (phased)
1) Directory move only: relocate files to the target layout; fix includes/CMake; no behavior change.
2) Introduce interfaces: ApiClient providers, History store, WebView wrapper contracts.
3) Logging and error unification: add categories and replace scattered message boxes.
4) WebView templating: split long HTML/JS into files; inject data via small helpers.
5) Background I/O: queue history operations; make retry/backoff configurable for API calls.
6) Tests and scripts: add core unit tests; add dev scripts for build/run; document debugging.

## Developer ergonomics
- Provide scripts/dev_env.ps1 to set Qt/WV2 env vars, create build dir, and launch the app.
- Add docs/DEBUGGING.md: how to enable logs, common failures (WV2 init, hotkey registration), and checklist.
- In debug mode, add shortcuts for reload-config/reload-history to avoid restarts.
