# windows

python -m nuitka --standalone --onefile --output-dir=. --output-filename=main --windows-icon-from-ico=src/assets/icon.ico --windows-console-mode=disable --lto=yes --enable-plugin=pyqt5 --include-data-dir=src/assets=assets --include-data-dir="D:/anaconda3/envs/app/lib/site-packages/PyQt5/Qt5/resources=PyQt5/Qt5/resources" --include-data-dir="D:/anaconda3/envs/app/lib/site-packages/PyQt5/Qt5/translations/qtwebengine_locales=PyQt5/Qt5/translations/qtwebengine_locales" src/main.py
