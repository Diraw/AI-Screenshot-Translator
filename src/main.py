import sys
import os
from PyQt5.QtWidgets import (
    QApplication,
    QWidget,
    QVBoxLayout,
    QPushButton,
    QMessageBox,
    QSystemTrayIcon,
    QMenu,
    QAction,
    QStyle,
)
from PyQt5.QtGui import QPixmap, QCursor, QIcon
from PyQt5.QtCore import (
    Qt,
    QByteArray,
    QBuffer,
    QIODevice,
    QRect,
    QThreadPool,
    QRunnable,
    pyqtSignal,
    QObject,
    pyqtSlot,
    QMetaObject,
)
import base64
import yaml
import keyboard
import traceback
import datetime

from api_client import APIClient
from html_viewer import HTMLViewer, HTMLWindow
from screenshot import ScreenshotTool, ScreenshotPreviewCard


# --- AIWorker 和 WorkerSignals 类 ---
class WorkerSignals(QObject):
    result = pyqtSignal(str, int, object, str, str)
    error = pyqtSignal(str, int)


class AIWorker(QRunnable):
    def __init__(
        self, api_client, base64_image_data, prompt_text, group_id, screenshot_card, debug_mode
    ):
        super().__init__()
        self.api_client = api_client
        self.base64_image_data = base64_image_data
        self.prompt_text = prompt_text
        self.group_id = group_id
        self.screenshot_card = screenshot_card
        self.debug_mode = debug_mode
        self._is_stopped = False # 用于外部停止信号

        self.signals = WorkerSignals()

    def stop(self):
        """设置停止标志，尝试优雅地停止任务"""
        self._is_stopped = True
        if self.debug_mode:
            print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [AIWorker] 组 {self.group_id}: 收到停止信号。")


    @pyqtSlot()
    def run(self):
        try:
            if self._is_stopped:
                if self.debug_mode:
                    print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [AIWorker] 组 {self.group_id}: 任务在开始前被停止。")
                return

            if self.debug_mode:
                print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [AIWorker] 组 {self.group_id}: AIWorker 线程开始处理。")
            model_response_markdown = self.api_client.process_image(
                self.base64_image_data, self.prompt_text
            )

            if self._is_stopped: # 检查任务是否在处理期间被停止
                if self.debug_mode:
                    print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [AIWorker] 组 {self.group_id}: 任务在处理后被停止，不发出结果。")
                return

            if self.debug_mode:
                print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [AIWorker] 组 {self.group_id}: AIWorker 线程完成处理。")
            self.signals.result.emit(
                model_response_markdown,
                self.group_id,
                self.screenshot_card,
                self.base64_image_data,
                self.prompt_text
            )
        except Exception as e:
            if self.debug_mode:
                print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [AIWorker] 组 {self.group_id}: AIWorker 线程发生错误: {e}")
                traceback.print_exc()
            self.signals.error.emit(f"[AIWorker] AI模型处理失败: {e}", self.group_id)


# --- IntegratedApp 类 ---
class IntegratedApp(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("AI截图翻译工具")
        self.setGeometry(100, 100, 250, 150)

        # 在加载配置之前初始化一些默认值，以防配置加载失败
        self.api_key = None
        self.base_url = None
        self.model_name = None
        self.prompt_text = None
        self.max_windows = 3 # 默认值
        self.zoom_sensitivity = 500.0 # 默认值
        self.screenshot_hotkey = "ctrl+alt+s" # 默认值
        self.icon_path = None # 默认值，将在 _load_config 中设置
        self.debug_mode = False # 默认值
        self.initial_font_size = 16 # 默认值

        # 用于保存原始的 stdout/stderr
        self._original_stdout = sys.stdout
        self._original_stderr = sys.stderr

        self._load_config() # 加载配置

        layout = QVBoxLayout(self)
        self.screenshot_button = QPushButton("点击截屏并翻译", self)
        self.screenshot_button.clicked.connect(self._start_screenshot_safe)
        layout.addWidget(self.screenshot_button)
        self.setLayout(layout)

        # 确保 api_key 和 base_url 不为 None
        self.api_client = APIClient(api_key=self.api_key or "", base_url=self.base_url or "", model_name=self.model_name or "")
        self.html_viewer = HTMLViewer()

        self.active_window_groups = []
        self.next_group_id = 1

        # 初始化线程池
        self.thread_pool = QThreadPool()
        if self.debug_mode:
            print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 最大线程数: {self.thread_pool.maxThreadCount()}")

        self.app = QApplication.instance() # 获取已存在的 QApplication 实例

        self.screenshot_tool = None
        self._register_hotkey()

        # --- 系统托盘相关初始化 ---
        self.tray_icon = None
        self._setup_tray_icon()
        # --- 系统托盘初始化结束 ---

        # 允许最小化到托盘
        self.setWindowFlags(self.windowFlags() | Qt.WindowMinimizeButtonHint)


    def _get_resource_path(self, relative_path):
        """
        获取资源文件的绝对路径，区分开发环境和打包环境。
        """
        if getattr(sys, 'frozen', False) and hasattr(sys, '_MEIPASS'):
            # 打包环境：资源路径指向临时解压目录
            return os.path.join(sys._MEIPASS, relative_path)
        else:
            # 开发环境：资源路径指向当前脚本所在的目录
            return os.path.join(os.path.dirname(__file__), relative_path)

    def _load_config(self):
        # 仅从应用程序的当前工作目录加载 config.yaml
        config_path = os.path.join(os.getcwd(), "config.yaml")
        debug_config_path=os.path.join(os.getcwd(), "my.yaml") # 用于开发阶段调试

        config = {}
        config_loaded_from = None

        try:
            if os.path.exists(debug_config_path):
                with open(debug_config_path, "r", encoding="utf-8") as f:
                    config = yaml.safe_load(f)
                config_loaded_from = debug_config_path

            elif os.path.exists(config_path):
                with open(config_path, "r", encoding="utf-8") as f:
                    config = yaml.safe_load(f)
                config_loaded_from = config_path
            else:
                # 如果 config.yaml 文件不存在，则弹出警告并退出
                QMessageBox.critical(
                    self,
                    "配置错误",
                    f"config.yaml 文件未找到。\n请确保 '{config_path}' 存在，并已正确配置您的API Key和其他设置。",
                )
                sys.exit(1) # 强制退出应用程序

            # 从加载的配置中获取设置，使用 .get() 确保健壮性
            self.api_key = config.get("api", {}).get("api_key")
            self.base_url = config.get("api", {}).get("base_url")
            self.model_name = config.get("api", {}).get("model")
            self.prompt_text = config.get("api", {}).get("prompt_text")
            self.max_windows = config.get("app_settings", {}).get("max_windows", 3) # 提供默认值
            self.zoom_sensitivity = float(
                config.get("app_settings", {}).get("zoom_sensitivity", 500.0) # 提供默认值
            )
            self.screenshot_hotkey = config.get("app_settings", {}).get(
                "screenshot_hotkey", "ctrl+alt+s" # 提供默认值
            )
            # 托盘图标路径配置：始终使用 _get_resource_path 获取内部资源
            icon_filename = config.get("app_settings", {}).get("icon_path", "./assets/icon.ico")
            self.icon_path = self._get_resource_path(icon_filename)
            # 读取 debug_mode
            self.debug_mode = config.get("app_settings", {}).get("debug_mode", False) # 提供默认值
            self.initial_font_size = config.get("app_settings", {}).get("initial_font_size", 16) # 提供默认值
            # 确保它是整数
            try:
                self.initial_font_size = int(self.initial_font_size)
            except ValueError:
                self.initial_font_size = 16 # 如果转换失败，使用默认值
                if self.debug_mode: # 这里仍然使用 print，因为日志重定向可能还没设置
                    print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 警告: initial_font_size 配置无效，使用默认值 16。")

            if not self.api_key or self.api_key == "YOUR_API_KEY_HERE":
                QMessageBox.warning(
                    self, "API Key 警告", "请在 config.yaml 中设置您的实际 API Key。"
                )

        except KeyError as e:
            QMessageBox.critical(
                self,
                "配置错误",
                f"config.yaml 中缺少必要的键: {e}。请检查配置文件格式。",
            )
            sys.exit(1)
        except yaml.YAMLError as e:
            QMessageBox.critical(
                self, "配置解析错误", f"config.yaml 文件格式错误: {e}"
            )
            sys.exit(1)
        except Exception as e:
            QMessageBox.critical(
                self, "加载配置失败", f"加载 config.yaml 时发生错误: {e}"
            )
            sys.exit(1)

        # --- 日志重定向和初始调试信息打印 ---
        if self.debug_mode:
            # 确保日志文件路径在可写的位置
            log_dir = os.path.join(os.getcwd(), "logs")
            os.makedirs(log_dir, exist_ok=True)
            timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
            log_file_path = os.path.join(log_dir, f"debug_log_{timestamp}.txt")

            try:
                # 将 stdout 和 stderr 重定向到文件
                sys.stdout = open(log_file_path, "a", encoding="utf-8")
                sys.stderr = open(log_file_path, "a", encoding="utf-8")

                # 打印初始信息，确认重定向成功
                print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 调试模式已开启，日志输出到: {log_file_path}")
                print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 配置加载自: {config_loaded_from}")
                print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 调试模式: {'开启' if self.debug_mode else '关闭'}")
                print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] API Key: {'已设置' if self.api_key and self.api_key != 'YOUR_API_KEY_HERE' else '未设置或为默认值'}")
                print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] Base URL: {self.base_url}")
                print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] Model Name: {self.model_name}")
                print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] Prompt Text: {self.prompt_text}")
                print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] Max Windows: {self.max_windows}")
                print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] Zoom Sensitivity: {self.zoom_sensitivity}")
                print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] Screenshot Hotkey: {self.screenshot_hotkey}")
                print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] Icon Path: {self.icon_path}")
                print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] Initial Font Size: {self.initial_font_size}")

            except Exception as e:
                # 如果重定向失败，恢复原始 stdout/stderr 并弹出警告
                sys.stdout = self._original_stdout
                sys.stderr = self._original_stderr
                QMessageBox.warning(self, "日志重定向失败", f"无法创建或写入日志文件: {e}\n调试信息将不可见。")
        # --- 日志重定向和初始调试信息打印结束 ---

    # ... (其他方法，将所有 print 语句替换为带有时间戳的 print) ...
    def _register_hotkey(self):
        try:
            keyboard.add_hotkey(self.screenshot_hotkey, self._start_screenshot_safe)
            if self.debug_mode:
                print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 已注册截图快捷键: {self.screenshot_hotkey}")
        except Exception as e:
            if self.debug_mode:
                print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 注册快捷键 '{self.screenshot_hotkey}' 失败: {e}")
                print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 请确保快捷键组合有效，且没有被其他程序占用。")
            QMessageBox.warning(
                self,
                "快捷键注册失败",
                f"无法注册快捷键 '{self.screenshot_hotkey}'：{e}\n请检查是否与其他程序冲突或权限问题。",
            )

    def _start_screenshot_safe(self):
        QMetaObject.invokeMethod(
            self, "start_screenshot_and_process", Qt.QueuedConnection
        )
        if self.debug_mode:
            print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 调度 start_screenshot_and_process 到主线程。")

    @pyqtSlot()
    def start_screenshot_and_process(self):
        if self.debug_mode:
            print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] (主线程) 快捷键被触发或按钮被点击，开始截图流程...")
        self.hide()  # 隐藏主窗口

        if self.max_windows != 0 and len(self.active_window_groups) >= self.max_windows:
            oldest_group = self.active_window_groups.pop(0)
            if self.debug_mode:
                print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] 超出最大窗口数量，关闭最旧的窗口组 (ID: {oldest_group['id']})")
            if oldest_group["screenshot_card"] and self._is_valid_qobject(
                oldest_group["screenshot_card"]
            ):
                oldest_group["screenshot_card"].close()
            if oldest_group["html_result_window"] and self._is_valid_qobject(
                oldest_group["html_result_window"]
            ):
                oldest_group["html_result_window"].close()

        if self.screenshot_tool is not None and self._is_valid_qobject(
            self.screenshot_tool
        ):
            if self.debug_mode:
                print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 检测到旧的 ScreenshotTool 实例，尝试关闭。")
            self.screenshot_tool.close()
            QApplication.processEvents()
        elif self.screenshot_tool is not None:
            if self.debug_mode:
                print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 旧的 ScreenshotTool 实例已无效，直接清除引用。")
            self.screenshot_tool = None

        try:
            self.screenshot_tool = ScreenshotTool()
            if self.debug_mode:
                print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] ScreenshotTool 实例已创建: {self.screenshot_tool}")
            self.screenshot_tool.screenshot_finished.connect(
                self.handle_screenshot_result
            )
            self.screenshot_tool.destroyed.connect(self._clear_screenshot_tool_ref)
            self.screenshot_tool.show()
            self.screenshot_tool.setAttribute(Qt.WA_DeleteOnClose)
            if self.debug_mode:
                print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] ScreenshotTool 窗口已创建并尝试显示。")
        except Exception as e:
            if self.debug_mode:
                print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 创建 ScreenshotTool 实例时发生异常: {e}")
                traceback.print_exc()
            self.show()  # 重新显示主窗口
            QMessageBox.critical(self, "截图工具错误", f"启动截图工具失败: {e}")
            return

    def handle_screenshot_result(self, pixmap: QPixmap, capture_rect_from_tool: QRect):
        if self.debug_mode:
            print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 收到截图结果信号。")
        if pixmap.isNull():
            if self.debug_mode:
                print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] 接收到空截图或截图失败，不进行处理。")
            return

        if self.debug_mode:
            print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 接收到有效截图，大小: {pixmap.size()}")

        current_group_id = self.next_group_id
        self.next_group_id += 1

        new_group = {
            "id": current_group_id,
            "screenshot_card": None,
            "html_result_window": None,
            "base64_image_data": None,
            "prompt_text": None,
            "ai_worker": None
        }
        self.active_window_groups.append(new_group)

        screenshot_card = ScreenshotPreviewCard(
            pixmap, zoom_sensitivity=self.zoom_sensitivity
        )
        screenshot_card.setWindowTitle(f"截图预览 - 组 {current_group_id}")

        initial_pos = (
            capture_rect_from_tool.topLeft()
            if not capture_rect_from_tool.isEmpty()
            else QCursor.pos()
        )

        screenshot_card.move(initial_pos)

        screenshot_card.show()
        screenshot_card.setAttribute(Qt.WA_DeleteOnClose)
        screenshot_card.destroyed.connect(
            lambda: self._clear_window_ref(current_group_id, "screenshot_card")
        )
        new_group["screenshot_card"] = screenshot_card

        byte_array = QByteArray()
        buffer = QBuffer(byte_array)
        buffer.open(QIODevice.WriteOnly)
        pixmap.save(buffer, "PNG")
        base64_image_data = base64.b64encode(byte_array.data()).decode("utf-8")

        new_group["base64_image_data"] = base64_image_data
        new_group["prompt_text"] = self.prompt_text

        if self.debug_mode:
            print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 组 {current_group_id}: 截图已转换为 base64 数据。")
            print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 组 {current_group_id}: 正在发送截图给AI模型进行翻译...")
        self._process_image_with_ai(
            base64_image_data, current_group_id, screenshot_card
        )

    def _process_image_with_ai(
        self,
        base64_image_data: str,
        group_id: int,
        screenshot_card: ScreenshotPreviewCard,
    ):
        if self.debug_mode:
            print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 组 {group_id}: 正在将 AI 任务提交到线程池...")

        # 查找对应的 group，如果存在，先停止旧的 AIWorker
        for group in self.active_window_groups:
            if group["id"] == group_id:
                if group["ai_worker"] and self._is_valid_qobject(group["ai_worker"]):
                    group["ai_worker"].stop() # 尝试停止旧的任务
                    if self.debug_mode:
                        print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 组 {group_id}: 停止了旧的 AIWorker 实例。")
                break


        worker = AIWorker(
            self.api_client,
            base64_image_data,
            self.prompt_text,
            group_id,
            screenshot_card,
            self.debug_mode
        )
        worker.signals.result.connect(self._handle_ai_success)
        worker.signals.error.connect(self._handle_ai_error)
        self.thread_pool.start(worker)

        # 将 worker 实例存储在 group 中，以便后续管理
        for group in self.active_window_groups:
            if group["id"] == group_id:
                group["ai_worker"] = worker
                break


    @pyqtSlot(str, int, object, str, str)
    def _handle_ai_success(
        self,
        model_response_markdown: str,
        group_id: int,
        screenshot_card: ScreenshotPreviewCard,
        original_base64_image_data: str,
        original_prompt_text: str
    ):
        if self.debug_mode:
            print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 组 {group_id}: 收到 AI 成功响应信号。")
        try:
            html_template_path = self._get_resource_path("./assets/template.html")
            self.api_client.set_html_template_path(html_template_path)

            html_content = self.api_client.create_html_content(model_response_markdown,initial_font_size=self.initial_font_size)

            # 查找现有窗口
            existing_html_window = None
            for group in self.active_window_groups:
                if group["id"] == group_id and group["html_result_window"] and self._is_valid_qobject(group["html_result_window"]):
                    existing_html_window = group["html_result_window"]
                    break

            if existing_html_window:
                # 更新现有窗口的内容
                if self.debug_mode:
                    print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 组 {group_id}: 更新现有 HTML 窗口内容。")
                existing_html_window.web_view.setHtml(html_content)
                # 确保原始数据也更新，以防再次重新翻译
                existing_html_window.original_base64_image_data = original_base64_image_data
                existing_html_window.original_prompt_text = original_prompt_text
                existing_html_window.original_screeenshot_card = screenshot_card # 确保截图卡片引用也更新
                # 重新显示（如果隐藏了）并激活
                existing_html_window.showNormal()
                existing_html_window.activateWindow()
                existing_html_window.raise_()
            else:
                # 创建新窗口
                if self.debug_mode:
                    print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 组 {group_id}: 创建新的 HTML 窗口。")

                initial_width = 400
                initial_pos = QCursor.pos()

                if (
                    screenshot_card
                    and self._is_valid_qobject(screenshot_card)
                    and not screenshot_card.isHidden()
                ):
                    initial_width = screenshot_card.width()
                    initial_pos = screenshot_card.pos()

                html_result_window = HTMLWindow(
                    html_content,
                    title=f"翻译结果 - 组 {group_id}",
                    width=initial_width,
                    height=300,
                    base64_image_data=original_base64_image_data,
                    prompt_text=original_prompt_text,
                    group_id=group_id,
                    screenshot_card=screenshot_card
                )
                html_result_window.signals.retranslate_requested.connect(self._handle_retranslate_request)

                if (
                    screenshot_card
                    and self._is_valid_qobject(screenshot_card)
                    and not screenshot_card.isHidden()
                ):
                    target_x = initial_pos.x() + initial_width + 10
                    target_y = initial_pos.y()

                    current_screen = QApplication.screenAt(initial_pos)
                    if current_screen:
                        current_screen_rect = current_screen.geometry()
                    else:
                        current_screen_rect = QApplication.primaryScreen().geometry()

                    if target_x + html_result_window.width() > current_screen_rect.right():
                        target_x = initial_pos.x()
                        target_y = initial_pos.y() + screenshot_card.height() + 10
                        if (
                            target_y + html_result_window.height()
                            > current_screen_rect.bottom()
                        ):
                            target_x = current_screen_rect.left()
                            target_y = current_screen_rect.top()

                    html_result_window.move(target_x, target_y)
                else:
                    html_result_window.move(QCursor.pos())

                html_result_window.show()
                html_result_window.setAttribute(Qt.WA_DeleteOnClose)
                html_result_window.destroyed.connect(
                    lambda: self._clear_window_ref(group_id, "html_result_window")
                )

                for group in self.active_window_groups:
                    if group["id"] == group_id:
                        group["html_result_window"] = html_result_window
                        break

        except Exception as e:
            if self.debug_mode:
                print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 组 {group_id}: 在主线程处理 AI 结果并显示 HTML 时失败: {e}")
                traceback.print_exc()
            self.html_viewer.show_error(
                f"[IntegratedApp] 显示翻译结果失败: {e}", f"显示错误 - 组 {group_id}"
            )

    @pyqtSlot(str, int)
    def _handle_ai_error(self, error_message: str, group_id: int):
        if self.debug_mode:
            print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 组 {group_id}: 收到 AI 错误响应信号: {error_message}")

        # 尝试在原窗口显示错误信息
        target_html_window = None
        for group in self.active_window_groups:
            if group["id"] == group_id and group["html_result_window"] and self._is_valid_qobject(group["html_result_window"]):
                target_html_window = group["html_result_window"]
                break

        if target_html_window:
            error_html = f"<html><body><h1>翻译错误</h1><p style='color: red;'>{error_message}</p></body></html>"
            target_html_window.web_view.setHtml(error_html)
            if self.debug_mode:
                print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 组 {group_id}: HTML窗口已更新为错误信息。")
        else:
            # 如果找不到原窗口，则创建新窗口显示错误
            self.html_viewer.show_error(error_message, f"翻译错误 - 组 {group_id}")


    @pyqtSlot(str, str, int, object)
    def _handle_retranslate_request(self, base64_image_data: str, prompt_text: str, group_id: int, screenshot_card: ScreenshotPreviewCard):
        """处理来自 HTMLWindow 的重新翻译请求"""
        if self.debug_mode:
            print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 组 {group_id}: 收到重新翻译请求。")

        # 找到对应的 HTMLWindow 实例，并更新其内容为“正在翻译”
        target_html_window = None
        for group in self.active_window_groups:
            if group["id"] == group_id and group["html_result_window"] and self._is_valid_qobject(group["html_result_window"]):
                target_html_window = group["html_result_window"]
                break

        if target_html_window:
            target_html_window.web_view.setHtml("<html><body><h1>正在重新翻译...</h1><p>请稍候。</p></body></html>")
            if self.debug_mode:
                print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 组 {group_id}: HTML窗口已更新为加载状态。")
        else:
            if self.debug_mode:
                print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 组 {group_id}: 未找到对应的 HTML 结果窗口，无法更新加载状态。")
            # 理论上重新翻译请求应该总能找到原窗口，这里可以考虑抛出警告或错误

        # 重新提交 AI 任务
        self._process_image_with_ai(base64_image_data, group_id, screenshot_card)

    def _is_valid_qobject(self, obj):
        try:
            # 检查 QObject 是否仍然有效（未被销毁）
            # 注意：对于非 QObject 实例，这会抛出 AttributeError 或 TypeError
            # 对于已销毁的 QObject，会抛出 RuntimeError
            if isinstance(obj, QObject):
                return not obj.isWindow() or obj.isVisible() or obj.isHidden() # 简单检查 QObject 状态
            return False
        except RuntimeError:
            return False
        except Exception as e:
            # 捕获其他可能的异常，例如 obj 不是 QObject 的情况
            if self.debug_mode:
                print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] _is_valid_qobject 检查时发生意外错误: {e}")
            return False


    def _clear_screenshot_tool_ref(self):
        if self.sender() is not None and self.sender() == self.screenshot_tool:
            if self.debug_mode:
                print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] ScreenshotTool 实例被销毁，清除引用。")
            self.screenshot_tool = None

    def _clear_window_ref(self, group_id: int, window_type: str):
        for group in list(self.active_window_groups):
            if group["id"] == group_id:
                if window_type == "screenshot_card":
                    if self.debug_mode:
                        print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 组 {group_id}: 截图预览卡片被销毁，清除引用。")
                    group["screenshot_card"] = None
                elif window_type == "html_result_window":
                    if self.debug_mode:
                        print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 组 {group_id}: HTML结果窗口被销毁，清除引用。")
                    group["html_result_window"] = None

                # 如果两个窗口都已关闭，且 AIWorker 也已完成，则从列表中移除组
                if (
                    group["screenshot_card"] is None
                    and group["html_result_window"] is None
                    and (group["ai_worker"] is None or not self._is_valid_qobject(group["ai_worker"])) # 确保 worker 也已完成或销毁
                ):
                    self.active_window_groups.remove(group)
                    if self.debug_mode:
                        print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 组 {group_id}: 所有窗口和worker引用已清理，从活动列表中移除。")
                break

    # --- 系统托盘相关方法 ---
    def _setup_tray_icon(self):
        # 检查系统是否支持托盘图标
        if not QSystemTrayIcon.isSystemTrayAvailable():
            QMessageBox.warning(self, "系统托盘", "您的系统不支持系统托盘图标。")
            return

        self.tray_icon = QSystemTrayIcon(self)
        # 确保图标文件存在
        if self.icon_path and os.path.exists(self.icon_path):
            self.tray_icon.setIcon(QIcon(self.icon_path))
        else:
            if self.debug_mode:
                print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 警告: 托盘图标文件未找到或路径无效: {self.icon_path}，使用默认图标。")
            self.tray_icon.setIcon(
                self.style().standardIcon(QStyle.SP_ComputerIcon)
            )  # 使用一个默认图标

        self.tray_icon.setToolTip("智能截图翻译工具")

        # 创建托盘菜单
        tray_menu = QMenu()

        show_action = QAction("显示主窗口", self)
        show_action.triggered.connect(self.show_main_window)
        tray_menu.addAction(show_action)

        screenshot_action = QAction("截屏并翻译", self)
        screenshot_action.triggered.connect(
            self._start_screenshot_safe
        )  # 托盘菜单也触发截图
        tray_menu.addAction(screenshot_action)

        exit_action = QAction("退出", self)
        exit_action.triggered.connect(self.app.quit)  # 连接到 QApplication 的 quit 方法
        tray_menu.addAction(exit_action)

        self.tray_icon.setContextMenu(tray_menu)

        # 连接托盘图标的激活信号（点击事件）
        self.tray_icon.activated.connect(self._on_tray_icon_activated)

        self.tray_icon.show()
        if self.debug_mode:
            print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 系统托盘图标已设置并显示。")

    @pyqtSlot(QSystemTrayIcon.ActivationReason)
    def _on_tray_icon_activated(self, reason):
        """处理托盘图标的点击事件"""
        if reason == QSystemTrayIcon.Trigger or reason == QSystemTrayIcon.DoubleClick:
            # 单击或双击时显示主窗口
            self.show_main_window()

    def show_main_window(self):
        """显示主窗口并将其置于前台"""
        self.showNormal()  # 恢复正常大小（如果最小化了）
        self.activateWindow()  # 激活窗口，使其获得焦点
        self.raise_()  # 将窗口置于顶部
        if self.debug_mode:
            print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 主窗口已显示。")

    def changeEvent(self, event):
        """
        处理窗口状态变化的事件，用于最小化到托盘。
        """
        if event.type() == event.WindowStateChange:
            if self.isMinimized():
                if self.debug_mode:
                    print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 主窗口最小化，隐藏到系统托盘。")
                self.hide()
                event.ignore() # 忽略事件，阻止窗口真正最小化到任务栏
        super().changeEvent(event)


    def closeEvent(self, event):
        """
        重写 closeEvent，当用户点击关闭按钮时，直接退出。
        """
        if self.debug_mode:
            print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 主窗口关闭事件触发，准备退出应用程序。")

        # 取消注册所有热键
        keyboard.unhook_all()
        if self.debug_mode:
            print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 所有热键已取消注册。")

        # 停止并清理所有 AIWorker 任务
        for group in list(self.active_window_groups):
            if group["ai_worker"] and self._is_valid_qobject(group["ai_worker"]):
                group["ai_worker"].stop() # 尝试停止正在进行的任务
                if self.debug_mode:
                    print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 组 {group['id']}: 停止了 AIWorker 实例。")
            # 关闭所有活动的截图预览卡片和HTML结果窗口
            if group["screenshot_card"] and self._is_valid_qobject(
                group["screenshot_card"]
            ):
                group["screenshot_card"].close()
            if group["html_result_window"] and self._is_valid_qobject(
                group["html_result_window"]
            ):
                group["html_result_window"].close()
        self.active_window_groups.clear()
        if self.debug_mode:
            print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 所有子窗口已关闭。")

        # 等待线程池中的任务完成 (或在 stop() 信号发出后尽快完成)
        # 增加一个超时，防止无限等待
        if self.debug_mode:
            print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 等待线程池任务完成...")
        self.thread_pool.waitForDone(5000) # 等待最多5秒
        if self.debug_mode:
            if self.thread_pool.activeThreadCount() > 0:
                 print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 警告: 线程池中仍有 {self.thread_pool.activeThreadCount()} 个活动线程。")
            else:
                 print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 线程池已关闭。")


        # 隐藏托盘图标
        if self.tray_icon:
            self.tray_icon.hide()
            # 立即销毁托盘图标，避免其在事件循环结束后仍存在
            self.tray_icon.deleteLater()
            self.tray_icon = None
            if self.debug_mode:
                print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 系统托盘图标已隐藏并销毁。")

        # 恢复 stdout/stderr 并关闭日志文件
        if self.debug_mode:
            if sys.stdout != self._original_stdout:
                sys.stdout.close()
                sys.stdout = self._original_stdout
            if sys.stderr != self._original_stderr:
                sys.stderr.close()
                sys.stderr = self._original_stderr
            print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] [IntegratedApp] 日志重定向已恢复。")


        # 告诉 QApplication 退出事件循环
        self.app.quit() # 显式调用 quit()

        # 允许窗口真正关闭
        event.accept()
        super().closeEvent(event)


if __name__ == "__main__":
    # 确保 QApplication 实例只创建一次
    if QApplication.instance() is None:
        app = QApplication(sys.argv)
    else:
        app = QApplication.instance()

    # 禁用当最后一个窗口关闭时退出应用程序，因为我们希望通过主窗口的关闭或托盘菜单来统一管理退出
    app.setQuitOnLastWindowClosed(False)

    main_app = IntegratedApp()
    main_app.show()

    # app.exec_() 会阻塞直到 QApplication.quit() 被调用
    exit_code = app.exec_()

    sys.exit(exit_code)