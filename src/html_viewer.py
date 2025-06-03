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
from PyQt5.QtCore import (
    Qt,
    QTimer,
    pyqtSlot,
    QPoint,
    pyqtSignal,
    QObject,
)
from PyQt5.QtWebChannel import QWebChannel

class HTMLWindowSignals(QObject):
    """HTMLWindow 的信号类"""
    retranslate_requested = pyqtSignal(str, str, int, object)

class HTMLWindow(QMainWindow):
    """HTML 显示窗口类"""

    js_ready_signal = pyqtSignal()

    def __init__(
        self,
        html_content,
        title="模型返回结果",
        width=400,
        height=300,
        base64_image_data=None,
        prompt_text=None,
        group_id=None,
        screenshot_card=None,
    ):
        super().__init__()
        self.setWindowTitle(title)
        self.setGeometry(100, 100, width, height)

        self.setWindowFlags(Qt.WindowStaysOnTopHint | Qt.FramelessWindowHint | Qt.Tool)
        self.setWindowOpacity(1)

        self.original_base64_image_data = base64_image_data
        self.original_prompt_text = prompt_text
        self.original_group_id = group_id
        self.original_screeenshot_card = screenshot_card

        self.signals = HTMLWindowSignals()

        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout(central_widget)
        main_layout.setContentsMargins(0, 0, 0, 0)

        self.web_view = QWebEngineView()
        self.channel = QWebChannel()
        self.web_view.page().setWebChannel(self.channel)
        self.channel.registerObject("qt_webchannel", self)

        # 预处理 HTML 内容，注入 CSS 确保滚动行为
        injected_css = """
        <style>
            /* 重置默认样式，确保一致性 */
            html, body {
                margin: 0;
                padding: 0;
                width: 100%;
                height: 100%;
                box-sizing: border-box; /* 确保 padding 和 border 不会增加元素总尺寸 */
            }

            /* 强制内容区域使用外部滚动条 */
            body {
                overflow: auto !important; /* 重要：覆盖任何内联样式 */
            }

            /* 确保内容不会溢出到滚动条之外 */
            * {
                box-sizing: border-box;
            }
        </style>
        """
        
        # 将 CSS 注入到 HTML 内容的 <head> 标签中
        if "<head>" in html_content:
            html_content = html_content.replace("<head>", "<head>" + injected_css)
        else:
            # 如果没有 <head> 标签，就加在 <html> 标签之后
            html_content = html_content.replace("<html>", "<html>" + injected_css)


        self.web_view.setHtml(html_content)
        main_layout.addWidget(self.web_view)

        self.js_ready_signal.connect(self._on_js_ready)

        # --- 控制按钮布局 ---
        control_layout = QHBoxLayout()
        control_layout.setContentsMargins(5, 5, 5, 5)

        self.toggle_view_button = QPushButton("T", self)
        self.toggle_view_button.setFixedSize(20, 20)
        self.toggle_view_button.setStyleSheet(
            "background-color: #4CAF50; color: white; border-radius: 10px;"
        )
        self.toggle_view_button.clicked.connect(self.toggle_view_mode)

        self.retranslate_button = QPushButton("↺", self)
        self.retranslate_button.setFixedSize(20, 20)
        self.retranslate_button.setStyleSheet(
            "background-color: #007BFF; color: white; border-radius: 10px;"
        )
        self.retranslate_button.clicked.connect(self._on_retranslate_button_clicked)

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

        control_layout.addStretch()
        control_layout.addWidget(self.toggle_view_button)
        control_layout.addWidget(self.retranslate_button)
        control_layout.addWidget(self.resize_button)
        control_layout.addWidget(self.close_button)

        main_layout.addLayout(control_layout)
        # --- 控制按钮布局结束 ---

        self.drag_position = None
        self.is_rendered_mode = True

        self.web_view.loadFinished.connect(self._on_web_view_load_finished)

        self.toggle_view_button.show()
        self.retranslate_button.show()
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
        self.retranslate_button.hide()
        self.resize_button.hide()
        self.close_button.hide()

        self.resizing = False
        self.resize_edge = Qt.Edges()
        self.min_width = 200
        self.min_height = self.control_layout_height + 50

        self.is_resizing_by_button = False
        self.resize_start_pos = QPoint()
        self.resize_start_width = 0
        self.resize_start_height = 0
        self.resize_sensitivity = 1.0

    @pyqtSlot()
    def _on_retranslate_button_clicked(self):
        if (
            self.original_base64_image_data
            and self.original_prompt_text
            and self.original_group_id
        ):
            self.signals.retranslate_requested.emit(
                self.original_base64_image_data,
                self.original_prompt_text,
                self.original_group_id,
                self.original_screeenshot_card,
            )
            self.web_view.setHtml(
                "<html><body><h1>正在重新翻译...</h1><p>请稍候。</p></body></html>"
            )
        else:
            print("[HTMLWindow] 无法重新翻译：缺少必要的原始数据。")
            self.web_view.setHtml(
                "<html><body><h1>重新翻译失败</h1><p>缺少原始图片或提示信息。</p></body></html>"
            )

    @pyqtSlot()
    def _on_js_ready(self):
        print("[HTMLWindow] JavaScript 环境已就绪。")
        QTimer.singleShot(100, self._try_update_height)

    def _inject_qwebchannel_init_script(self):
        script = QWebEngineScript()
        script_content = """
            console.log("Initializing QWebChannel.");
            
            if (typeof QWebChannel !== 'undefined') {
                new QWebChannel(qt.webChannelTransport, function(channel) {
                    window.qt_webchannel = channel.objects.qt_webchannel;
                    console.log("QWebChannel initialized. qt_webchannel object acquired.");
                    
                    if (typeof window.qt_webchannel !== 'undefined' && typeof window.qt_webchannel.js_ready_signal === 'function') {
                        window.qt_webchannel.js_ready_signal();
                        console.log("js_ready_signal sent to Python.");
                        
                        window.updateHeight = function() {
                            console.log("updateHeight called from JS.");
                            const body = document.body;
                            const html = document.documentElement;
                            
                            // 获取内容的实际高度，包括溢出部分
                            const contentHeight = Math.max(
                                body.scrollHeight,
                                body.offsetHeight,
                                html.clientHeight,
                                html.scrollHeight,
                                html.offsetHeight
                            );
                            // 额外获取宽度，虽然目前只用于调试，但可以帮助理解横向溢出
                            const contentWidth = Math.max(
                                body.scrollWidth,
                                body.offsetWidth,
                                html.clientWidth,
                                html.scrollWidth,
                                html.offsetWidth
                            );

                            console.log("Calculated content height: " + contentHeight + ", width: " + contentWidth);

                            if (typeof window.qt_webchannel !== 'undefined' && typeof window.qt_webchannel.updateWindowHeight === 'function') {
                                window.qt_webchannel.updateWindowHeight(contentHeight);
                                console.log("Height updated to: " + contentHeight);
                            } else {
                                console.warn("qt_webchannel or updateWindowHeight not ready during updateHeight call, this should not happen.");
                            }
                        };

                        if (typeof MathJax !== 'undefined') {
                            console.log("MathJax found, setting up observer for updateHeight.");
                            MathJax.Hub.Queue(function() {
                                MathJax.Hub.Register.MessageHook("End Typeset", function() {
                                    console.log("MathJax finished rendering, triggering updateHeight.");
                                    if (window.updateHeight) {
                                        window.updateHeight();
                                    }
                                });
                            });
                        }
                        
                        setTimeout(function() {
                            if (window.updateHeight) {
                                window.updateHeight();
                            }
                        }, 50);

                        const observer = new MutationObserver(function(mutationsList, observer) {
                            for(let mutation of mutationsList) {
                                if (mutation.type === 'childList' || mutation.type === 'characterData' || 
                                    (mutation.type === 'attributes' && (mutation.attributeName === 'style' || mutation.attributeName === 'class'))) {
                                    console.log("DOM changed, triggering updateHeight.");
                                    if (window.updateHeight) {
                                        window.updateHeight();
                                    }
                                    break;
                                }
                            }
                        });
                        observer.observe(document.body, { childList: true, subtree: true, attributes: true, characterData: true });
                        console.log("MutationObserver initialized.");

                    } else {
                        console.error("qt_webchannel object or js_ready_signal method not found after QWebChannel init.");
                    }
                });
            } else {
                console.error("QWebChannel is not defined. Make sure qwebchannel.js is loaded.");
            }
        """
        script.setSourceCode(script_content)
        script.setInjectionPoint(QWebEngineScript.DocumentReady)
        script.setRunsOnSubFrames(True)
        script.setWorldId(QWebEngineScript.MainWorld)
        self.web_view.page().scripts().insert(script)

    @pyqtSlot(float)
    def updateWindowHeight(self, content_height_from_js):
        if self.resizing or self.is_resizing_by_button:
            return

        if content_height_from_js <= 0:
            return

        current_width = self.width()
        new_total_height = content_height_from_js + self.control_layout_height + 40

        new_total_height = max(new_total_height, self.min_height)

        if (
            self.size().height() != new_total_height
            or self.size().width() != current_width
        ):
            print(
                f"[HTMLWindow] 调整窗口大小：从 ({current_width}, {self.height()}) 到 ({current_width}, {new_total_height})"
            )
            self.resize(current_width, new_total_height)
        else:
            print(
                f"[HTMLWindow] 窗口大小已匹配，无需调整。当前: ({current_width}, {self.height()}), 目标: ({current_width}, {new_total_height})"
            )

    def _on_web_view_load_finished(self, ok):
        if ok:
            print("[HTMLWindow] WebEngineView 加载完成。")
            QTimer.singleShot(100, self._inject_qwebchannel_init_script)

            self.toggle_view_button.show()
            self.retranslate_button.show()
            self.resize_button.show()
            self.close_button.show()

        else:
            print("[HTMLWindow] WebEngineView 加载失败。")

    def _check_and_call_js(self, js_code):
        try:
            self.web_view.page().runJavaScript(js_code)
        except Exception as e:
            print(f"[HTMLWindow] 执行JavaScript时出错: {e}")

    def _try_update_height(self):
        js_code = """
        try {
            if (typeof window.updateHeight === 'function') {
                window.updateHeight();
                console.log("updateHeight executed successfully by _try_update_height from Python");
            } else {
                console.warn("updateHeight is not defined when _try_update_height called from Python. QWebChannel init might still be in progress.");
            }
        } catch (e) {
            console.error("Error executing updateHeight from Python:", e);
        }
        """
        self._check_and_call_js(js_code)

    def toggle_view_mode(self):
        if self.is_rendered_mode:
            js_code = """
            try {
                if (typeof showRaw === 'function') {
                    showRaw();
                    console.log("showRaw executed successfully");
                } else {
                    console.error("showRaw is not defined");
                }
            } catch (e) {
                console.error("Error executing showRaw:", e);
            }
            """
            self._check_and_call_js(js_code)
            self.is_rendered_mode = False
            self.toggle_view_button.setText("R")
            self.toggle_view_button.setStyleSheet(
                "background-color: #4CAF50; color: white; border-radius: 10px;"
            )
        else:
            js_code = """
            try {
                if (typeof showRendered === 'function') {
                    showRendered();
                    console.log("showRendered executed successfully");
                } else {
                    console.error("showRendered is not defined");
                }
            } catch (e) {
                console.error("Error executing showRendered:", e);
            }
            """
            self._check_and_call_js(js_code)
            self.is_rendered_mode = True
            self.toggle_view_button.setText("T")
            self.toggle_view_button.setStyleSheet(
                "background-color: #4CAF50; color: white; border-radius: 10px;"
            )
        QTimer.singleShot(300, self._try_update_height)

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

    # --- 窗口拖拽和边缘调整大小的鼠标事件处理---
    def mousePressEvent(self, event):
        if event.button() == Qt.LeftButton:
            if (
                self.close_button.geometry().contains(event.pos())
                or self.toggle_view_button.geometry().contains(event.pos())
                or self.resize_button.geometry().contains(event.pos())
                or self.retranslate_button.geometry().contains(event.pos())
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
        # 底部边缘需要避开控制按钮区域
        if (
            pos.y() > rect.bottom() - margin
            and pos.y() < self.height() - self.control_layout_height
        ):
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
            print(f"[HTMLViewer] 错误: HTML 模板文件 '{template_path}' 未找到。")
            return "<html><body><h1>错误：HTML 模板文件未找到！</h1></body></html>"
        except Exception as e:
            print(f"[HTMLViewer] 加载 HTML 模板文件 '{template_path}' 时出错: {e}")
            return f"<html><body><h1>错误：加载 HTML 模板失败！</h1><p>{e}</p></body></html>"

    def show_html(
        self,
        html_content,
        title="HTML 查看器",
        width=400,
        height=300,
        base64_image_data=None,
        prompt_text=None,
        group_id=None,
        screenshot_card=None,
    ):
        if self.app is None:
            self.create_application()
        window = HTMLWindow(
            html_content,
            title,
            width,
            height,
            base64_image_data,
            prompt_text,
            group_id,
            screenshot_card,
        )
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