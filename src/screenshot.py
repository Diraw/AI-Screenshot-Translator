from PyQt5.QtWidgets import (
    QApplication,
    QWidget,
    QRubberBand,
    QVBoxLayout,
    QPushButton,
    QLabel,
    QHBoxLayout,
)
from PyQt5.QtGui import QPixmap, QCursor, QPainter, QPen, QColor
from PyQt5.QtCore import (
    Qt,
    QRect,
    QPoint,
    QTimer,
    pyqtSignal,
)
import traceback

class ScreenshotTool(QWidget):
    screenshot_finished = pyqtSignal(QPixmap, QRect)

    def __init__(self):
        print("[ScreenshotTool] 构造函数开始执行。")
        try:
            print("[ScreenshotTool] 尝试调用 super().__init__()")
            super().__init__()
            print("[ScreenshotTool] super().__init__() 调用成功。")

            self.setWindowTitle("截图工具")
            # 设置窗口标志，使其置顶、无边框、工具窗口
            self.setWindowFlags(
                Qt.WindowStaysOnTopHint | Qt.FramelessWindowHint | Qt.Tool
            )
            # 设置窗口全屏以覆盖所有屏幕
            self.setWindowState(Qt.WindowFullScreen)
            # 设置鼠标光标为十字形
            self.setCursor(Qt.CrossCursor)
            # 设置窗口透明度，以便看到下面的内容。
            self.setWindowOpacity(0.5)

            self.begin = QPoint()  # 框选开始点
            self.end = QPoint()  # 框选结束点
            # 创建一个橡皮筋选择框
            self.rubberBand = QRubberBand(QRubberBand.Rectangle, self)
            self.capture_rect = QRect()  # 最终捕获的矩形区域

            print("[ScreenshotTool] 尝试获取桌面几何信息。")
            # 获取整个虚拟桌面的几何信息（多显示器环境）
            self.virtual_desktop_rect = QApplication.desktop().screenGeometry()
            print("[ScreenshotTool] 桌面几何信息获取成功。")
            # 将窗口设置为与虚拟桌面相同的几何形状
            self.setGeometry(self.virtual_desktop_rect)

            print("[ScreenshotTool] 尝试获取主屏幕 DPI。")
            # 获取主屏幕的设备像素比，用于处理高DPI显示器
            self.primary_screen_dpi = QApplication.primaryScreen().devicePixelRatio()
            print("[ScreenshotTool] 主屏幕 DPI 获取成功。")
            print(f"[ScreenshotTool] 主屏幕设备像素比 (DPI): {self.primary_screen_dpi}")
            print(
                f"[ScreenshotTool] 虚拟桌面逻辑尺寸: {self.virtual_desktop_rect.width()}x{self.virtual_desktop_rect.height()}"
            )
            print("[ScreenshotTool] 窗口已初始化完成所有属性设置。")

            # 显示窗口并激活它，确保它在最前面
            self.show()
            self.activateWindow()
            print("[ScreenshotTool] 窗口已强制显示和激活。")
            # 延迟再次激活，有时可以解决焦点问题
            QTimer.singleShot(50, self.activateWindow)
            print("[ScreenshotTool] 安排了延迟激活。")

            # 设置窗口在关闭时自动删除
            self.setAttribute(Qt.WA_DeleteOnClose)

        except Exception as e:
            print(f"[ScreenshotTool] 构造函数中发生异常: {e}")
            traceback.print_exc()
            # 如果构造失败，发出一个空的信号并关闭窗口
            self.screenshot_finished.emit(QPixmap(), QRect())
            self.close()
            return

    def mousePressEvent(self, event):
        if event.button() == Qt.LeftButton:
            self.begin = event.pos()
            self.end = self.begin
            self.rubberBand.setGeometry(QRect(self.begin, self.end).normalized())
            self.rubberBand.show()
            print(
                f"[ScreenshotTool] 鼠标按下事件：开始点 {self.begin.x()},{self.begin.y()}"
            )

    def mouseMoveEvent(self, event):
        if event.buttons() & Qt.LeftButton:
            self.end = event.pos()
            self.rubberBand.setGeometry(QRect(self.begin, self.end).normalized())

    def mouseReleaseEvent(self, event):
        if event.button() == Qt.LeftButton:
            self.end = event.pos()
            self.rubberBand.hide()

            self.capture_rect = QRect(self.begin, self.end).normalized()

            if (
                self.capture_rect.isEmpty()
                or self.capture_rect.width() < 10
                or self.capture_rect.height() < 10
            ):
                print("[ScreenshotTool] 未选择有效截图区域或区域过小，关闭截图工具。")
                self.screenshot_finished.emit(QPixmap(), QRect())  # 发送空信号表示取消
                self.close()
                return

            print(
                f"[ScreenshotTool] 鼠标释放事件：结束点 {self.end.x()},{self.end.y()}，选择区域 {self.capture_rect}"
            )

            self.hide()  # 隐藏截图工具窗口
            QApplication.processEvents()  # 强制处理事件，确保窗口隐藏

            QTimer.singleShot(100, self.perform_screenshot)  # 延迟执行截图操作
            print("[ScreenshotTool] 延迟截图任务已安排。")

    # --- 新增：键盘按下事件处理 ---
    def keyPressEvent(self, event):
        if event.key() == Qt.Key_Escape:
            print("[ScreenshotTool] Esc 键按下，取消截图。")
            self.rubberBand.hide()  # 隐藏橡皮筋
            self.screenshot_finished.emit(QPixmap(), QRect())  # 发送空信号表示取消
            self.close()  # 关闭截图工具窗口
            event.accept()  # 接受事件，阻止其传播
        else:
            super().keyPressEvent(event)  # 调用父类的事件处理，以防有其他需要处理的键

    def perform_screenshot(self):
        print("[ScreenshotTool] 开始执行截图操作...")
        final_pixmap = QPixmap()

        # 计算虚拟桌面的物理像素尺寸
        virtual_desktop_physical_width = int(
            self.virtual_desktop_rect.width() * self.primary_screen_dpi
        )
        virtual_desktop_physical_height = int(
            self.virtual_desktop_rect.height() * self.primary_screen_dpi
        )

        # 创建一个 QPixmap 来绘制整个虚拟桌面
        full_virtual_desktop_pixmap = QPixmap(
            virtual_desktop_physical_width, virtual_desktop_physical_height
        )
        full_virtual_desktop_pixmap.fill(Qt.transparent)  # 填充透明背景

        painter = QPainter(full_virtual_desktop_pixmap)

        screens = QApplication.screens()  # 获取所有屏幕
        print(f"[ScreenshotTool] 发现 {len(screens)} 个屏幕。")
        for screen in screens:
            # 抓取每个屏幕的内容
            screen_pixmap = screen.grabWindow(0)  # 抓取整个屏幕

            if not screen_pixmap.isNull():
                screen_geometry_logical = screen.geometry()  # 获取屏幕的逻辑几何信息

                # 计算屏幕在虚拟桌面上的物理像素位置
                screen_physical_x = int(
                    screen_geometry_logical.x() * self.primary_screen_dpi
                )
                screen_physical_y = int(
                    screen_geometry_logical.y() * self.primary_screen_dpi
                )

                # 将抓取的屏幕内容绘制到大的虚拟桌面 QPixmap 上
                painter.drawPixmap(screen_physical_x, screen_physical_y, screen_pixmap)
                print(
                    f"[ScreenshotTool] 已抓取屏幕 '{screen.name()}' ({screen_pixmap.width()}x{screen_pixmap.height()}) 并绘制到 ({screen_physical_x},{screen_physical_y})"
                )
            else:
                print(f"[ScreenshotTool] 警告: 无法抓取屏幕 '{screen.name()}' 的内容。")

        painter.end()  # 结束绘制

        if not full_virtual_desktop_pixmap.isNull():
            print(
                f"[ScreenshotTool] 成功拼接整个虚拟桌面，物理像素尺寸: {full_virtual_desktop_pixmap.size()}"
            )

            # 根据用户选择的逻辑区域，计算出在物理像素上的裁剪区域
            crop_rect_physical = QRect(
                int(self.capture_rect.x() * self.primary_screen_dpi),
                int(self.capture_rect.y() * self.primary_screen_dpi),
                int(self.capture_rect.width() * self.primary_screen_dpi),
                int(self.capture_rect.height() * self.primary_screen_dpi),
            )

            # 确保裁剪区域在拼接后的虚拟桌面图像范围内
            crop_rect_physical = crop_rect_physical.intersected(
                full_virtual_desktop_pixmap.rect()
            )

            if not crop_rect_physical.isEmpty():
                # 裁剪出最终的截图
                final_pixmap = full_virtual_desktop_pixmap.copy(crop_rect_physical)
                print(f"[ScreenshotTool] 裁剪后的最终截图尺寸: {final_pixmap.size()}")
            else:
                print("[ScreenshotTool] 警告: 计算出的裁剪区域为空或无效。")
        else:
            print("[ScreenshotTool] 错误: 无法创建完整的虚拟桌面截图。")

        print(
            f"[ScreenshotTool] 截图完成，pixmap.isNull(): {final_pixmap.isNull()}, size: {final_pixmap.size()}"
        )

        self.screenshot_finished.emit(
            final_pixmap, self.capture_rect
        )  # 发送最终截图和区域
        self.close()  # 关闭截图工具窗口
        print("[ScreenshotTool] 截图工具窗口已关闭。")


class ScreenshotPreviewCard(QWidget):
    # 构造函数添加 border_color 参数
    def __init__(self, pixmap, parent=None, zoom_sensitivity=500.0, border_color=None):
        super().__init__(parent)
        self.setWindowFlags(Qt.WindowStaysOnTopHint | Qt.FramelessWindowHint | Qt.Tool)
        # 保持透明背景，但我们会在 paintEvent 中明确绘制边框
        self.setAttribute(Qt.WA_TranslucentBackground)
        print("[PreviewCard] 窗口已初始化。")

        if pixmap.isNull():
            print("[PreviewCard] 尝试显示空图片，卡片将不显示或显示异常。")
            QTimer.singleShot(0, self.close)
            return

        self.original_pixmap = pixmap
        self.zoom_sensitivity = zoom_sensitivity

        current_screen = QApplication.screenAt(QCursor.pos())
        if not current_screen:
            current_screen = QApplication.primaryScreen()

        self.device_pixel_ratio = current_screen.devicePixelRatio()

        self.current_logical_width = int(
            self.original_pixmap.width() / self.device_pixel_ratio
        )
        self.current_logical_height = int(
            self.current_logical_width
            * (self.original_pixmap.height() / self.original_pixmap.width())
        )

        # 定义边框宽度
        self.border_width = 1
        # 使用传入的边框颜色，如果未传入则使用默认值
        self.border_color = border_color if border_color else QColor(100, 100, 100)

        # 调整窗口大小以容纳边框和底部控件
        self.setFixedSize(
            self.current_logical_width + 2 * self.border_width,
            self.current_logical_height + 30 + 2 * self.border_width, # 30 是底部控制栏的高度
        )

        main_layout = QVBoxLayout(self)
        main_layout.setContentsMargins(
            self.border_width, self.border_width, self.border_width, self.border_width
        )
        # 确保布局内部控件之间没有额外的间距
        main_layout.setSpacing(0)

        self.image_label = QLabel(self)
        self.image_label.setPixmap(self.original_pixmap)
        self.image_label.setScaledContents(True)
        # 设置 image_label 的大小策略，确保它能填充可用空间
        self.image_label.setSizePolicy(
            QApplication.desktop().sizePolicy().horizontalPolicy(),
            QApplication.desktop().sizePolicy().verticalPolicy(),
        )
        main_layout.addWidget(self.image_label)

        control_layout = QHBoxLayout()
        control_layout.setContentsMargins(5, 5, 5, 5) # 底部按钮的内边距
        control_layout.setSpacing(5) # 按钮之间的间距

        self.zoom_button = QPushButton("⤡", self)
        self.zoom_button.setFixedSize(20, 20)
        self.zoom_button.setStyleSheet(
            "background-color: #FFC107; color: white; border-radius: 10px;"
        )
        self.zoom_button.mousePressEvent = self._zoom_button_mouse_press
        self.zoom_button.mouseMoveEvent = self._zoom_button_mouse_move
        self.zoom_button.mouseReleaseEvent = self._zoom_button_mouse_release

        control_layout.addStretch()
        control_layout.addWidget(self.zoom_button)

        self.close_button = QPushButton("X", self)
        self.close_button.setFixedSize(20, 20)
        self.close_button.setStyleSheet(
            "background-color: red; color: white; border-radius: 10px;"
        )
        self.close_button.clicked.connect(self.close)

        control_layout.addWidget(self.close_button)
        main_layout.addLayout(control_layout)

        self.oldPos = None
        self.is_resizing_by_zoom_button = False
        self.zoom_start_pos = QPoint()
        self.zoom_start_logical_width = 0
        self.zoom_start_logical_height = 0

        self.min_zoom_width = 100
        self.max_zoom_width = 2000

        self.setAttribute(Qt.WA_DeleteOnClose)

    def _zoom_button_mouse_press(self, event):
        if event.button() == Qt.LeftButton:
            self.is_resizing_by_zoom_button = True
            self.zoom_start_pos = event.globalPos()
            self.zoom_start_logical_width = self.current_logical_width
            self.zoom_start_logical_height = self.current_logical_height
            self.setCursor(Qt.SizeFDiagCursor)
            event.accept()
            print("[PreviewCard] 缩放按钮鼠标按下。")

    def _zoom_button_mouse_move(self, event):
        if self.is_resizing_by_zoom_button and event.buttons() == Qt.LeftButton:
            delta_x = event.globalPos().x() - self.zoom_start_pos.x()
            scale_factor = delta_x / self.zoom_sensitivity
            new_logical_width = self.zoom_start_logical_width + int(
                self.zoom_start_logical_width * scale_factor
            )
            new_logical_width = max(
                self.min_zoom_width, min(new_logical_width, self.max_zoom_width)
            )
            new_logical_height = int(
                new_logical_width
                * (self.original_pixmap.height() / self.original_pixmap.width())
            )
            self.current_logical_width = new_logical_width
            self.current_logical_height = new_logical_height

            # 调整窗口大小以容纳边框
            new_window_width = self.current_logical_width + 2 * self.border_width
            new_window_height = (
                self.current_logical_height + 30 + 2 * self.border_width
            )
            self.setFixedSize(new_window_width, new_window_height)
            event.accept()

    def _zoom_button_mouse_release(self, event):
        self.is_resizing_by_zoom_button = False
        self.setCursor(Qt.ArrowCursor)
        event.accept()
        print("[PreviewCard] 缩放按钮鼠标释放。")

    def mousePressEvent(self, event):
        if not self.is_resizing_by_zoom_button and event.button() == Qt.LeftButton:
            # 检查点击是否在按钮区域内
            if self.close_button.geometry().contains(
                event.pos()
            ) or self.zoom_button.geometry().contains(event.pos()):
                return
            self.oldPos = event.globalPos()
            print("[PreviewCard] 窗口拖拽鼠标按下。")

    def mouseMoveEvent(self, event):
        if (
            not self.is_resizing_by_zoom_button
            and event.buttons() == Qt.LeftButton
            and self.oldPos is not None
        ):
            delta = QPoint(event.globalPos() - self.oldPos)
            self.move(self.x() + delta.x(), self.y() + delta.y())
            self.oldPos = event.globalPos()

    def mouseReleaseEvent(self, event):
        if not self.is_resizing_by_zoom_button:
            self.oldPos = None
            print("[PreviewCard] 窗口拖拽鼠标释放。")

    # --- 预览卡片也添加 Esc 键关闭功能，虽然不是截图框选，但用户习惯可能一致 ---
    def keyPressEvent(self, event):
        if event.key() == Qt.Key_Escape:
            self.close()
            print("[PreviewCard] Esc 键按下，关闭预览卡片。")
        else:
            super().keyPressEvent(event)

    # --- 绘制事件，用于绘制边框 ---
    def paintEvent(self, event):
        super().paintEvent(event)

        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)  # 启用抗锯齿，使边框更平滑

        try:
            pen = QPen(self.border_color)
            pen.setWidth(self.border_width)
            painter.setPen(pen)
            painter.setBrush(Qt.NoBrush)

            image_label_rect_in_parent = self.image_label.geometry()

            border_rect = QRect(
                image_label_rect_in_parent.x() - self.border_width,
                image_label_rect_in_parent.y() - self.border_width,
                image_label_rect_in_parent.width() + 2 * self.border_width,
                image_label_rect_in_parent.height() + 2 * self.border_width,
            )

            painter.drawRect(border_rect)
        finally:
            painter.end()
