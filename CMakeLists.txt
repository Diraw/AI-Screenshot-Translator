cmake_minimum_required(VERSION 3.16)

project(AI-Screenshot-Translator-Cpp VERSION 1.0.4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Trigger re-config for AUTOMOC
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Gui Core Concurrent Sql)

# Ensure assets are copied to the build directory


# Source files
set(PROJECT_SOURCES
    src/main.cpp
    src/App.cpp
    src/App_Tray.cpp
    src/App_Screenshot.cpp
    src/App_Api.cpp
    src/App_Windows.cpp
    src/App_Config.cpp
    src/App_Theme.cpp
    src/App.h
    src/ColorUtils.cpp
    src/ColorUtils.h
    src/WinKeyForwarder.cpp
    src/WinKeyForwarder.h
    src/ConfigManager.cpp
    src/ConfigManager.h
    src/GlobalHotkey.cpp
    src/GlobalHotkey.h
    src/ScreenshotTool.cpp
    src/ScreenshotTool.h
    src/PreviewCard.cpp
    src/PreviewCard.h
    src/HotkeyEdit.cpp
    src/HotkeyEdit.h
    src/ResultWindow.cpp
    src/ResultWindow_Html.cpp
    src/ResultWindow_StateHotkeys.cpp
    src/ResultWindow_Theme.cpp
    src/ResultWindow.h
    src/ConfigDialog.cpp
    src/ConfigDialog_Ui.cpp
    src/ConfigDialog_UiSections.cpp
    src/ConfigDialog_ColorPreview.cpp
    src/ConfigDialog_GeneralTab.cpp
    src/ConfigDialog_TranslationTab.cpp
    src/ConfigDialog_ArchiveTab.cpp
    src/ConfigDialog_OtherTab.cpp
    src/ConfigDialog_Retranslate.cpp
    src/ConfigDialog_ProfileWatcher.cpp
    src/ConfigDialog_ProfileCrud.cpp
    src/ConfigDialog_Config.cpp
    src/ConfigDialog.h
    src/TranslationManager.cpp
    src/ApiClient.cpp
    src/ApiClient.h
    src/SummaryWindow.cpp
    src/SummaryWindow_Entries.cpp
    src/SummaryWindow_StateHotkeys.cpp
    src/SummaryWindow_Html.cpp
    src/SummaryWindow_Filters.cpp
    src/SummaryWindow.h
    src/TagDialog.h
    src/TagDialog.cpp
    src/StartupWindow.h
    src/StartupWindow.cpp
    src/StartupWindow_Ui.cpp
    src/HintPopup.h
    src/HintPopup.cpp
    src/HistoryManager.cpp
    src/HistoryManager_Paths.cpp
    src/HistoryManager_Entries.cpp
    src/HistoryManager_Tags.cpp
    src/HistoryManager_Watcher.cpp
    src/HistoryManager.h
    src/EmbedWebView.cpp
    src/EmbedWebView_Actions.cpp
    src/DotEnv.cpp
    src/DotEnv.h
    src/UmamiClient.cpp
    src/UmamiClient.h
    src/AnalyticsManager.cpp
    src/AnalyticsManager.h
    assets.qrc
)

if(WIN32)
    enable_language(RC)
    list(APPEND PROJECT_SOURCES resources.rc)
    add_definitions(-DNOMINMAX)
endif()

add_executable(AI-Screenshot-Translator-Cpp ${PROJECT_SOURCES})

target_compile_definitions(AI-Screenshot-Translator-Cpp PRIVATE
    APP_NAME="AI Screenshot Translator"
    APP_VERSION="${PROJECT_VERSION}"
    APP_COPYRIGHT="Copyright 2026 Diraw"
    APP_REPO_URL="https://github.com/Diraw/AI-Screenshot-Translator"
)

target_link_libraries(AI-Screenshot-Translator-Cpp PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::Gui
    Qt6::Core
    Qt6::Concurrent
    Qt6::Sql
)

get_filename_component(QT6_ROOT_DIR "${Qt6_DIR}/../../.." ABSOLUTE)

if(WIN32)
    target_link_libraries(AI-Screenshot-Translator-Cpp PRIVATE
        shlwapi
        ole32
        user32
        version
        advapi32
        shell32
        dwmapi
    )
endif()

# Handle Windows subsystem (GUI vs Console)
if(WIN32)
    set_target_properties(AI-Screenshot-Translator-Cpp PROPERTIES WIN32_EXECUTABLE ON)

    # Copy WebView2Loader.dll to output directory
    add_custom_command(TARGET AI-Screenshot-Translator-Cpp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/webview2_pkg/build/native/x64/WebView2Loader.dll"
        $<TARGET_FILE_DIR:AI-Screenshot-Translator-Cpp>
    )
endif()

# Ensure assets are copied to the build directory (Post-build to handle Multi-Config generators like VS)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    COMMENT "Copying assets to output directory"
)

# Copy runtime DLL dependencies (including Qt6Sql.dll).
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy -t "$<TARGET_FILE_DIR:${PROJECT_NAME}>" $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>
    COMMAND_EXPAND_LISTS
    COMMENT "Copying runtime DLLs"
)

# Ensure SQLite driver plugin is deployed.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/sqldrivers"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${QT6_ROOT_DIR}/plugins/sqldrivers/qsqlite.dll"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/sqldrivers/qsqlite.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${QT6_ROOT_DIR}/plugins/sqldrivers/qsqlited.dll"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/sqldrivers/qsqlited.dll"
    COMMENT "Copying qsqlite driver plugin"
)
