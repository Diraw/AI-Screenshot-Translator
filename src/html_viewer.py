import sys
import os
from PyQt5.QtWidgets import (
    QApplication,
    QMainWindow,
    QVBoxLayout,
    QWidget,
    QPushButton,
    QHBoxLayout,
)
from PyQt5.QtWebEngineWidgets import (
    QWebEngineView,
    QWebEngineScript,
)
from PyQt5.QtCore import Qt, QTimer, pyqtSlot, QPoint
from PyQt5.QtWebChannel import QWebChannel

class HTMLWindow(QMainWindow):
    """HTML 显示窗口类"""

    def __init__(self, html_content, title="模型返回结果", width=400, height=300):
        super().__init__()
        self.setWindowTitle(title)
        self.setGeometry(100, 100, width, height)

        self.setWindowFlags(Qt.WindowStaysOnTopHint | Qt.FramelessWindowHint | Qt.Tool)
        self.setWindowOpacity(1)

        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout(central_widget)
        main_layout.setContentsMargins(0, 0, 0, 0)

        self.web_view = QWebEngineView()
        self.web_view.setHtml(html_content)
        main_layout.addWidget(self.web_view)

        # --- 控制按钮布局 ---
        control_layout = QHBoxLayout()
        control_layout.setContentsMargins(5, 5, 5, 5)

        self.toggle_view_button = QPushButton("T", self)
        self.toggle_view_button.setFixedSize(20, 20)
        self.toggle_view_button.setStyleSheet(
            "background-color: #4CAF50; color: white; border-radius: 10px;"
        )
        self.toggle_view_button.clicked.connect(self.toggle_view_mode)

        self.resize_button = QPushButton("⤡", self)
        self.resize_button.setFixedSize(20, 20)
        self.resize_button.setStyleSheet(
            "background-color: #FFC107; color: white; border-radius: 10px;"
        )
        self.resize_button.mousePressEvent = self._resize_button_mouse_press
        self.resize_button.mouseMoveEvent = self._resize_button_mouse_move
        self.resize_button.mouseReleaseEvent = self._resize_button_mouse_release

        self.close_button = QPushButton("X", self)
        self.close_button.setFixedSize(20, 20)
        self.close_button.setStyleSheet(
            "background-color: red; color: white; border-radius: 10px;"
        )
        self.close_button.clicked.connect(self.close)

        # 调整布局顺序：
        # 先添加一个 stretch，它会占据所有可用空间，将后面的按钮推到最右边
        control_layout.addStretch()
        control_layout.addWidget(self.toggle_view_button)
        control_layout.addWidget(self.resize_button)
        control_layout.addWidget(self.close_button)

        main_layout.addLayout(control_layout)
        # --- 控制按钮布局结束 ---

        self.drag_position = None
        self.is_rendered_mode = True

        # 初始化 QWebChannel
        self.channel = QWebChannel()
        self.web_view.page().setWebChannel(self.channel)
        self.channel.registerObject("qt_webchannel", self)

        self.web_view.loadFinished.connect(self._on_web_view_load_finished)

        # 确保控件在计算高度前已经有尺寸
        self.toggle_view_button.show()
        self.resize_button.show()
        self.close_button.show()
        QApplication.processEvents()
        self.control_layout_height = (
            self.toggle_view_button.height()
            + control_layout.contentsMargins().top()
            + control_layout.contentsMargins().bottom()
            + 10
        )
        self.toggle_view_button.hide()
        self.resize_button.hide()
        self.close_button.hide()

        # 用于调整窗口边缘大小的变量
        self.resizing = False
        self.resize_edge = Qt.Edges()
        self.min_width = 200
        self.min_height = self.control_layout_height + 50

        # 按钮缩放相关的变量
        self.is_resizing_by_button = False
        self.resize_start_pos = QPoint()
        self.resize_start_width = 0
        self.resize_start_height = 0
        self.resize_sensitivity = 1.0

    def _inject_js_update_height_function(self):
        """注入一个JavaScript函数到页面，供HTML调用以更新窗口高度"""
        script = QWebEngineScript()
        script.setSourceCode(
            """
            window.updateHeight = function() {
                const body = document.body;
                const html = document.documentElement;
                const contentHeight = Math.max(
                    body.scrollHeight,
                    body.offsetHeight,
                    html.clientHeight,
                    html.scrollHeight,
                    html.offsetHeight
                );
                if (typeof qt_webchannel !== 'undefined' && qt_webchannel.updateWindowHeight) {
                    qt_webchannel.updateWindowHeight(contentHeight);
                } else {
                    console.warn("qt_webchannel.updateWindowHeight not ready yet.");
                }
            };
            setTimeout(window.updateHeight, 100);
        """
        )
        script.setInjectionPoint(QWebEngineScript.DocumentReady)
        script.setRunsOnSubFrames(True)
        self.web_view.page().scripts().insert(script)

    @pyqtSlot(float)
    def updateWindowHeight(self, height):
        """
        从JavaScript接收到的内容高度，并调整窗口大小
        这个方法会被JavaScript调用
        """
        if height <= 0:
            return

        if self.resizing or self.is_resizing_by_button:
            return

        current_width = self.width()
        new_height = height + self.control_layout_height + 40

        new_height = max(new_height, self.min_height)

        if self.size().height() != new_height or self.size().width() != current_width:
            self.resize(current_width, new_height)

    def _on_web_view_load_finished(self, ok):
        """当QWebEngineView加载HTML内容完成时调用"""
        if ok:
            print("WebEngineView 加载完成。")
            self._inject_js_update_height_function()

            self.toggle_view_button.show()
            self.resize_button.show()
            self.close_button.show()

            QTimer.singleShot(
                200,
                lambda: self.web_view.page().runJavaScript("showRendered();"),
            )
            QTimer.singleShot(
                500,
                lambda: self.web_view.page().runJavaScript(
                    "if (window.updateHeight) window.updateHeight();"
                ),
            )
        else:
            print("WebEngineView 加载失败。")

    def toggle_view_mode(self):
        """切换显示模式（渲染结果 / 原始文本）"""
        if self.is_rendered_mode:
            self.web_view.page().runJavaScript("showRaw();")
            self.is_rendered_mode = False
            self.toggle_view_button.setText("R")
            self.toggle_view_button.setStyleSheet(
                "background-color: #2196F3; color: white; border-radius: 10px;"
            )
        else:
            self.web_view.page().runJavaScript("showRendered();")
            self.is_rendered_mode = True
            self.toggle_view_button.setText("T")
            self.toggle_view_button.setStyleSheet(
                "background-color: #4CAF50; color: white; border-radius: 10px;"
            )
        QTimer.singleShot(
            300,
            lambda: self.web_view.page().runJavaScript(
                "if (window.updateHeight) window.updateHeight();"
            ),
        )

    # --- 缩放按钮的鼠标事件处理 ---
    def _resize_button_mouse_press(self, event):
        if event.button() == Qt.LeftButton:
            self.is_resizing_by_button = True
            self.resize_start_pos = event.globalPos()
            self.resize_start_width = self.width()
            self.resize_start_height = self.height()
            self.setCursor(Qt.SizeFDiagCursor)
            event.accept()

    def _resize_button_mouse_move(self, event):
        if self.is_resizing_by_button and event.buttons() == Qt.LeftButton:
            delta_x = event.globalPos().x() - self.resize_start_pos.x()
            delta_y = event.globalPos().y() - self.resize_start_pos.y()

            new_width = self.resize_start_width + int(delta_x * self.resize_sensitivity)
            new_height = self.resize_start_height + int(
                delta_y * self.resize_sensitivity
            )

            new_width = max(self.min_width, new_width)
            new_height = max(self.min_height, new_height)

            self.resize(new_width, new_height)

            QTimer.singleShot(
                50,
                lambda: self.web_view.page().runJavaScript(
                    "if (window.updateHeight) window.updateHeight();"
                ),
            )
            event.accept()

    def _resize_button_mouse_release(self, event):
        self.is_resizing_by_button = False
        self.setCursor(Qt.ArrowCursor)
        event.accept()
        QTimer.singleShot(
            100,
            lambda: self.web_view.page().runJavaScript(
                "if (window.updateHeight) window.updateHeight();"
            ),
        )

    # --- 窗口拖拽和边缘调整大小的鼠标事件处理 (需要修改以排除新按钮) ---
    def mousePressEvent(self, event):
        if event.button() == Qt.LeftButton:
            if (
                self.close_button.geometry().contains(event.pos())
                or self.toggle_view_button.geometry().contains(event.pos())
                or self.resize_button.geometry().contains(event.pos())
            ):
                return

            self.resizing = False
            self.resize_edge = self.get_resize_edge(event.pos())
            if self.resize_edge != Qt.Edges():
                self.resizing = True
                event.accept()
                return

            self.drag_position = event.globalPos() - self.frameGeometry().topLeft()
            event.accept()

    def mouseMoveEvent(self, event):
        if self.is_resizing_by_button:
            return

        if self.resizing:
            new_rect = self.geometry()
            global_pos = event.globalPos()

            if self.resize_edge & Qt.LeftEdge:
                new_rect.setLeft(global_pos.x())
                if new_rect.width() < self.min_width:
                    new_rect.setLeft(new_rect.right() - self.min_width)
            if self.resize_edge & Qt.RightEdge:
                new_rect.setRight(global_pos.x())
                if new_rect.width() < self.min_width:
                    new_rect.setRight(new_rect.left() + self.min_width)
            if self.resize_edge & Qt.TopEdge:
                new_rect.setTop(global_pos.y())
                if new_rect.height() < self.min_height:
                    new_rect.setTop(new_rect.bottom() - self.min_height)
            if self.resize_edge & Qt.BottomEdge:
                new_rect.setBottom(global_pos.y())
                if new_rect.height() < self.min_height:
                    new_rect.setBottom(new_rect.top() + self.min_height)

            self.setGeometry(new_rect)
            event.accept()

            QTimer.singleShot(
                50,
                lambda: self.web_view.page().runJavaScript(
                    "if (window.updateHeight) window.updateHeight();"
                ),
            )
            return

        if event.buttons() == Qt.LeftButton and self.drag_position:
            self.move(event.globalPos() - self.drag_position)
            event.accept()

        self.setCursor(self.get_resize_cursor(event.pos()))

    def mouseReleaseEvent(self, event):
        if self.is_resizing_by_button:
            return

        self.drag_position = None
        self.resizing = False
        self.resize_edge = Qt.Edges()
        self.setCursor(Qt.ArrowCursor)
        event.accept()

        QTimer.singleShot(
            100,
            lambda: self.web_view.page().runJavaScript(
                "if (window.updateHeight) window.updateHeight();"
            ),
        )

    def get_resize_edge(self, pos):
        """根据鼠标位置判断是否在窗口边缘，并返回边缘类型"""
        margin = 5
        rect = self.rect()
        edge = Qt.Edges()

        if pos.x() < rect.left() + margin:
            edge |= Qt.LeftEdge
        if pos.x() > rect.right() - margin:
            edge |= Qt.RightEdge
        if pos.y() < rect.top() + margin:
            edge |= Qt.TopEdge
        if pos.y() > rect.bottom() - margin:
            edge |= Qt.BottomEdge
        return edge

    def get_resize_cursor(self, pos):
        """根据边缘类型返回对应的鼠标指针"""
        edge = self.get_resize_edge(pos)
        if edge == (Qt.LeftEdge | Qt.TopEdge):
            return Qt.SizeFDiagCursor
        elif edge == (Qt.RightEdge | Qt.BottomEdge):
            return Qt.SizeFDiagCursor
        elif edge == (Qt.LeftEdge | Qt.BottomEdge):
            return Qt.SizeBDiagCursor
        elif edge == (Qt.RightEdge | Qt.TopEdge):
            return Qt.SizeBDiagCursor
        elif edge == Qt.LeftEdge or edge == Qt.RightEdge:
            return Qt.SizeHorCursor
        elif edge == Qt.TopEdge or edge == Qt.BottomEdge:
            return Qt.SizeVerCursor
        else:
            return Qt.ArrowCursor

    def set_opacity(self, opacity):
        """设置窗口透明度"""
        self.setWindowOpacity(opacity)

    def set_always_on_top(self, on_top=True):
        """设置是否始终置顶"""
        current_flags = self.windowFlags()
        if on_top:
            self.setWindowFlags(current_flags | Qt.WindowStaysOnTopHint)
        else:
            self.setWindowFlags(current_flags & ~Qt.WindowStaysOnTopHint)
        self.show()


class HTMLViewer:
    def __init__(self):
        self.app = None
        self.windows = []

    def create_application(self):
        if self.app is None:
            self.app = QApplication(sys.argv)
        return self.app

    def _get_resource_path(self, relative_path):
        """
        获取打包后或开发环境中的资源文件路径。
        """
        if getattr(sys, "frozen", False):
            base_path = sys._MEIPASS
        else:
            base_path = os.path.dirname(os.path.abspath(__file__))
        return os.path.join(base_path, relative_path)

    def _load_html_template(self, template_filename="./assets/template.html"):
        """
        加载指定的 HTML 模板文件。
        """
        template_path = self._get_resource_path(template_filename)
        try:
            with open(template_path, "r", encoding="utf-8") as f:
                return f.read()
        except FileNotFoundError:
            print(f"错误: HTML 模板文件 '{template_path}' 未找到。")
            return "<html><body><h1>错误：HTML 模板文件未找到！</h1></body></html>"
        except Exception as e:
            print(f"加载 HTML 模板文件 '{template_path}' 时出错: {e}")
            return f"<html><body><h1>错误：加载 HTML 模板失败！</h1><p>{e}</p></body></html>"

    def show_html(self, html_content, title="HTML 查看器", width=400, height=300):
        if self.app is None:
            self.create_application()
        window = HTMLWindow(html_content, title, width, height)
        self.windows.append(window)
        window.setAttribute(Qt.WA_DeleteOnClose)
        window.destroyed.connect(
            lambda: self.windows.remove(window) if window in self.windows else None
        )
        window.show()
        return window

    def show_error(self, error_message, title="错误"):
        error_html = f"<h1>错误</h1><p style='color: red;'>{error_message}</p>"
        return self.show_html(error_html, title, 400, 200)

    def run(self):
        if self.app:
            sys.exit(self.app.exec_())

    def close_all_windows(self):
        for window in list(self.windows):
            window.close()
