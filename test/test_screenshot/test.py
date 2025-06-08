import sys
from PyQt5.QtWidgets import (
    QApplication,
    QWidget,
    QVBoxLayout,
    QPushButton,
)
from PyQt5.QtCore import (
    Qt,
    QMetaObject,
    pyqtSlot,
)
from PyQt5.QtGui import QColor # 导入 QColor
import keyboard
import yaml
import traceback


from pathlib import Path
current_path = Path(__file__).parent  # test_screenshot目录
parent_path = current_path.parent  # test目录
project_root = parent_path.parent  # python项目根目录
sys.path.insert(0, str(project_root))  # 将项目根目录添加到搜索路径最前面

try:
    from src.screenshot import ScreenshotTool, ScreenshotPreviewCard
except ImportError as e:
    print(f"无法导入Screenshot模块，请检查路径: {e}")
    sys.exit(1)

class MainApp(QWidget):
    def __init__(self, config_path="config.yaml"):
        super().__init__()
        self.setWindowTitle("快速截图工具")
        self.setGeometry(100, 100, 200, 100)

        self.config = self._load_config(config_path)
        self.zoom_sensitivity = self.config.get("app_settings", {}).get(
            "zoom_sensitivity", 500.0
        )
        self.screenshot_hotkey = self.config.get("app_settings", {}).get(
            "screenshot_hotkey", "ctrl+alt+s"
        )
        # 新增：从配置文件加载边框颜色
        self.preview_card_border_color = self._parse_border_color(
            self.config.get("app_settings", {}).get("preview_card_border_color", "100,100,100")
        )


        layout = QVBoxLayout(self)
        self.screenshot_button = QPushButton("点击截屏", self)
        self.screenshot_button.clicked.connect(self._start_screenshot_safe)
        layout.addWidget(self.screenshot_button)
        self.setLayout(layout)

        self.screenshot_card = None
        self.screenshot_tool = None

        self._register_hotkey()
        print("[MainApp] 主应用程序初始化完成。")

    def _load_config(self, config_path):
        try:
            with open(config_path, "r", encoding="utf-8") as f:
                config_data = yaml.safe_load(f)
                print(f"[MainApp] 配置文件 '{config_path}' 已加载。")
                return config_data
        except FileNotFoundError:
            print(f"[MainApp] 配置文件 '{config_path}' 未找到，使用默认设置。")
            return {}
        except yaml.YAMLError as e:
            print(f"[MainApp] 解析配置文件 '{config_path}' 失败: {e}，使用默认设置。")
            return {}

    def _parse_border_color(self, color_str):
        """
        解析颜色字符串为 QColor 对象。
        支持 'R,G,B' 格式或 Qt 颜色名称。
        """
        try:
            # 尝试解析 RGB 字符串
            rgb_values = [int(x.strip()) for x in color_str.split(',')]
            if len(rgb_values) == 3:
                return QColor(rgb_values[0], rgb_values[1], rgb_values[2])
            else:
                # 如果不是 RGB 格式，尝试作为颜色名称
                color = QColor(color_str)
                if color.isValid():
                    return color
                else:
                    print(f"[MainApp] 警告: 无法解析颜色名称 '{color_str}'，使用默认灰色。")
                    return QColor(100, 100, 100) # 默认灰色
        except Exception:
            print(f"[MainApp] 警告: 无法解析边框颜色 '{color_str}'，使用默认灰色。")
            return QColor(100, 100, 100) # 默认灰色


    def _register_hotkey(self):
        try:
            keyboard.add_hotkey(self.screenshot_hotkey, self._start_screenshot_safe)
            print(f"[MainApp] 已注册截图快捷键: {self.screenshot_hotkey}")
        except Exception as e:
            print(f"[MainApp] 注册快捷键 '{self.screenshot_hotkey}' 失败: {e}")
            print("[MainApp] 请确保快捷键组合有效，且没有被其他程序占用。")

    def _start_screenshot_safe(self):
        QMetaObject.invokeMethod(self, "start_screenshot", Qt.QueuedConnection)
        print("[MainApp] 调度 start_screenshot 到主线程。")

    @pyqtSlot()
    def start_screenshot(self):
        print("[MainApp] (主线程) 快捷键被触发或按钮被点击，开始截图流程...")
        self.hide()
        print("[MainApp] 主窗口已隐藏。")

        if self.screenshot_tool is not None and self._is_valid_qobject(
            self.screenshot_tool
        ):
            print("[MainApp] 检测到旧的 ScreenshotTool 实例，尝试关闭。")
            self.screenshot_tool.close()
            QApplication.processEvents()
        elif self.screenshot_tool is not None:
            print("[MainApp] 旧的 ScreenshotTool 实例已无效，直接清除引用。")
            self.screenshot_tool = None

        try:
            self.screenshot_tool = ScreenshotTool()
            print(f"[MainApp] ScreenshotTool 实例已创建: {self.screenshot_tool}")
            self.screenshot_tool.screenshot_finished.connect(
                self.handle_screenshot_result
            )
            self.screenshot_tool.destroyed.connect(self._clear_screenshot_tool_ref)
            print(
                "[MainApp] ScreenshotTool 窗口已创建并尝试显示 (通过其自身 __init__ 中的 show/activate)。"
            )
        except Exception as e:
            print(f"[MainApp] 创建 ScreenshotTool 实例时发生异常: {e}")
            traceback.print_exc()
            self.show()
            return

    def _is_valid_qobject(self, obj):
        try:
            _ = obj.objectName()
            return True
        except RuntimeError:
            return False
        except Exception as e:
            print(f"[MainApp] _is_valid_qobject 检查时发生意外错误: {e}")
            return False

    def _clear_screenshot_tool_ref(self):
        if self.sender() is not None and self.sender() == self.screenshot_tool:
            print("[MainApp] ScreenshotTool 实例被销毁，清除引用。")
            self.screenshot_tool = None

    def handle_screenshot_result(self, pixmap, capture_rect):
        print("[MainApp] 收到截图结果信号。")
        if not pixmap.isNull():
            print(f"[MainApp] 接收到有效截图，尺寸: {pixmap.size()}")

            if self.screenshot_card is not None and self._is_valid_qobject(
                self.screenshot_card
            ):
                print("[MainApp] 关闭旧的截图预览卡片。")
                self.screenshot_card.close()
                QApplication.processEvents()
            elif self.screenshot_card is not None:
                print("[MainApp] 旧的 ScreenshotPreviewCard 实例已无效，直接清除引用。")
                self.screenshot_card = None

            # 传递解析后的边框颜色给 ScreenshotPreviewCard
            self.screenshot_card = ScreenshotPreviewCard(
                pixmap,
                zoom_sensitivity=self.zoom_sensitivity,
                border_color=self.preview_card_border_color # 传递边框颜色
            )
            self.screenshot_card.show()
            self.screenshot_card.destroyed.connect(self._clear_screenshot_card_ref)
            print("[MainApp] 截图预览卡片已创建并尝试显示。")

        else:
            print("[MainApp] 接收到空截图或截图失败。")

        self.show()
        print("[MainApp] 主窗口已重新显示。")

    def _clear_screenshot_card_ref(self):
        if self.sender() is not None and self.sender() == self.screenshot_card:
            print("[MainApp] 截图卡片被销毁，清除引用。")
            self.screenshot_card = None

    def closeEvent(self, event):
        keyboard.unhook_all()
        print("[MainApp] 应用程序关闭，所有热键已取消注册。")
        super().closeEvent(event)


if __name__ == "__main__":
    app = QApplication(sys.argv)
    main_app = MainApp("config.yaml") # 确保 config.yaml 在正确路径
    main_app.show()
    sys.exit(app.exec_())