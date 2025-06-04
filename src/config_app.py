import sys
from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QFormLayout,
    QLabel, QLineEdit, QPushButton, QScrollArea, QGroupBox, QCheckBox,
    QMessageBox, QTextEdit, QTabWidget, QTextBrowser
)
from PyQt5.QtCore import Qt, QUrl
from PyQt5.QtGui import QIntValidator, QDoubleValidator, QTextDocument
from config_manager import ConfigManager
import copy
import os
import markdown

class ConfigApp(QWidget):
    TRANSLATIONS = {
        "api": "API 设置",
        "model": "模型(model)",
        "prompt_text": "提示文本(prompt_text)",
        "api_key": "API 密钥(api_key)",
        "base_url": "API 服务地址(base_url)",
        "proxy": "代理地址",
        "app_settings": "应用设置",
        "max_windows": "最大窗口数量",
        "zoom_sensitivity": "缩放敏感度",
        "screenshot_hotkey": "截图快捷键",
        "debug_mode": "调试模式",
        "initial_font_size": "初始字体大小"
    }

    def __init__(self):
        super().__init__()
        self.config_manager = ConfigManager()
        self.config_widgets = {}
        self.init_ui()
        self.load_config_to_ui()
        self.load_hint_content()

    def init_ui(self):
        self.setWindowTitle("配置编辑器")
        self.setGeometry(100, 100, 800, 700)

        main_layout = QVBoxLayout()

        self.tab_widget = QTabWidget()
        main_layout.addWidget(self.tab_widget)

        # ------------------- 配置页 -------------------
        self.config_page = QWidget()
        config_page_layout = QVBoxLayout(self.config_page)

        scroll_area = QScrollArea()
        scroll_area.setWidgetResizable(True)
        self.content_widget = QWidget()
        self.content_layout = QVBoxLayout(self.content_widget)
        self.content_layout.setAlignment(Qt.AlignTop)
        scroll_area.setWidget(self.content_widget)

        config_page_layout.addWidget(scroll_area)

        save_button = QPushButton("保存配置")
        save_button.clicked.connect(self.save_config_from_ui)
        config_page_layout.addWidget(save_button)

        self.tab_widget.addTab(self.config_page, "配置")

        # ------------------- 提示页 -------------------
        self.hint_page = QWidget()
        hint_page_layout = QVBoxLayout(self.hint_page)

        self.hint_text_edit = QTextBrowser()
        # 允许 QTextBrowser 处理图像资源，Markdown 转换的 HTML 仍然会使用相对路径
        self.hint_text_edit.document().setBaseUrl(QUrl.fromLocalFile(os.path.dirname(__file__) + os.sep))
        hint_page_layout.addWidget(self.hint_text_edit)

        self.tab_widget.addTab(self.hint_page, "提示")

        self.setLayout(main_layout)

    def get_translated_text(self, key):
        return self.TRANSLATIONS.get(key, key)

    def create_input_field(self, form_layout, key, value, parent_key_path=""):
        full_key_path = f"{parent_key_path}.{key}" if parent_key_path else key

        label = QLabel(f"{self.get_translated_text(key)}:")
        input_widget = None

        if isinstance(value, bool):
            input_widget = QCheckBox()
            input_widget.setChecked(value)
        elif isinstance(value, (int, float)):
            input_widget = QLineEdit(str(value))
            if isinstance(value, int):
                input_widget.setValidator(QIntValidator())
            elif isinstance(value, float):
                input_widget.setValidator(QDoubleValidator())
        elif isinstance(value, str):
            if key == "prompt_text":
                input_widget = QTextEdit(value)
                input_widget.setFixedHeight(80)
            else:
                input_widget = QLineEdit(value)
        else:
            input_widget = QLineEdit(str(value))
            input_widget.setReadOnly(True)
            QMessageBox.warning(self, "警告", f"不支持的配置类型：{full_key_path} ({type(value).__name__})。将显示为只读文本。")

        if input_widget:
            self.config_widgets[full_key_path] = input_widget
            form_layout.addRow(label, input_widget)


    def build_gui_from_config(self, config_dict, parent_layout, parent_key_path=""):
        current_form_layout = QFormLayout()
        current_form_layout.setLabelAlignment(Qt.AlignRight)
        current_form_layout.setFormAlignment(Qt.AlignLeft)
        current_form_layout.setVerticalSpacing(10)

        for key, value in config_dict.items():
            current_key_path = f"{parent_key_path}.{key}" if parent_key_path else key

            if isinstance(value, dict):
                group_box = QGroupBox(self.get_translated_text(key))
                group_v_layout = QVBoxLayout()
                group_box.setLayout(group_v_layout)
                parent_layout.addWidget(group_box)
                self.build_gui_from_config(value, group_v_layout, current_key_path)
            else:
                self.create_input_field(current_form_layout, key, value, parent_key_path)

        if current_form_layout.rowCount() > 0:
            parent_layout.addLayout(current_form_layout)


    def load_config_to_ui(self):
        config_data = self.config_manager.get_config()

        self.clear_layout(self.content_layout)
        self.config_widgets.clear()

        self.build_gui_from_config(config_data, self.content_layout)

    def save_config_from_ui(self):
        current_config = self.config_manager.get_config()
        if current_config is None:
            current_config = self.config_manager.get_default_config()

        config_to_save = copy.deepcopy(current_config)
        self._update_config_from_gui_widgets(config_to_save)

        try:
            if self.config_manager.save_config(config_to_save):
                QMessageBox.information(self, "成功", "配置已成功保存！")
            else:
                QMessageBox.critical(self, "错误", "保存配置失败，请检查文件权限。")
        except Exception as e:
            QMessageBox.critical(self, "错误", f"保存时发生未知错误：\n{e}")

    def _update_config_from_gui_widgets(self, config_dict, parent_key_path=""):
        for key, value in config_dict.items():
            full_key_path = f"{parent_key_path}.{key}" if parent_key_path else key

            if isinstance(value, dict):
                self._update_config_from_gui_widgets(value, full_key_path)
            else:
                if full_key_path in self.config_widgets:
                    widget = self.config_widgets[full_key_path]
                    try:
                        if isinstance(widget, QLineEdit):
                            # 根据原始值的类型进行转换
                            if isinstance(value, int):
                                config_dict[key] = int(widget.text())
                            elif isinstance(value, float):
                                config_dict[key] = float(widget.text())
                            else: # 字符串类型
                                config_dict[key] = widget.text()
                        elif isinstance(widget, QCheckBox):
                            config_dict[key] = widget.isChecked()
                        elif isinstance(widget, QTextEdit):
                            config_dict[key] = widget.toPlainText()
                    except ValueError:
                        QMessageBox.warning(self, "输入错误", f"键 '{self.get_translated_text(key)}' 的值 '{widget.text()}' 类型不匹配，将保留原值。")


    def clear_layout(self, layout):
        if layout is not None:
            while layout.count():
                item = layout.takeAt(0)
                if item.widget():
                    item.widget().deleteLater()
                if item.layout():
                    self.clear_layout(item.layout())

    def load_hint_content(self):
        hint_file_path = os.path.join(os.path.dirname(__file__), "./assets/hint.md")
        if os.path.exists(hint_file_path):
            try:
                with open(hint_file_path, "r", encoding="utf-8") as f:
                    markdown_content = f.read()
                    # 将 Markdown 转换为 HTML
                    html_content = markdown.markdown(markdown_content)

                    doc = self.hint_text_edit.document()
                    doc.setBaseUrl(QUrl.fromLocalFile(os.path.dirname(__file__) + os.sep))

                    self.hint_text_edit.setHtml(html_content)

            except Exception as e:
                self.hint_text_edit.setPlainText(f"无法加载提示文件 './assets/hint.md'：\n{e}")
                QMessageBox.warning(self, "错误", f"无法加载提示文件 './assets/hint.md'：\n{e}")
        else:
            self.hint_text_edit.setPlainText("未找到提示文件 './assets/hint.md'。请确保它与应用程序在同一目录下。")
            QMessageBox.information(self, "提示", "未找到提示文件 './assets/hint.md'。")