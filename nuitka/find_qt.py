import PyQt5.QtCore
import os

# Qt 库的根目录，通常是 resources 和 translations 的父目录
qt_data_path = PyQt5.QtCore.QLibraryInfo.location(PyQt5.QtCore.QLibraryInfo.DataPath)
qt_translations_path = PyQt5.QtCore.QLibraryInfo.location(PyQt5.QtCore.QLibraryInfo.TranslationsPath)
qt_bin_path = PyQt5.QtCore.QLibraryInfo.location(PyQt5.QtCore.QLibraryInfo.LibrariesPath) # 尝试获取库路径，通常dll在这里

print(f"Qt Data Path: {qt_data_path}")
print(f"Qt Translations Path: {qt_translations_path}")
print(f"Qt Bin Path (Libraries): {qt_bin_path}")

# 尝试构建 Qt WebEngine 资源路径
webengine_resources_path = os.path.join(qt_data_path, 'resources')
webengine_locales_path = os.path.join(qt_translations_path, 'qtwebengine_locales')

print(f"Expected WebEngine Resources Path: {webengine_resources_path}")
print(f"Expected WebEngine Locales Path: {webengine_locales_path}")

# 查找 ICU DLLs
print("\nSearching for ICU DLLs in common locations...")
icu_dlls = []
# 检查 bin 目录
if os.path.exists(qt_bin_path):
    for f in os.listdir(qt_bin_path):
        if f.startswith('icu') and f.endswith('.dll'):
            icu_dlls.append(os.path.join(qt_bin_path, f))
# 检查 DataPath 目录
if os.path.exists(qt_data_path):
    for f in os.listdir(qt_data_path):
        if f.startswith('icu') and f.endswith('.dll'):
            icu_dlls.append(os.path.join(qt_data_path, f))

if icu_dlls:
    print("Found ICU DLLs:")
    for dll in icu_dlls:
        print(f"  {dll}")
else:
    print("No ICU DLLs found in common Qt paths. You might need to search manually.")