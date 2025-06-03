import sys
from pathlib import Path

# 添加父目录到系统路径，以便导入src中的模块
current_path = Path(__file__).parent
parent_path = current_path.parent
sys.path.append(str(parent_path))

try:
    from src.html_viewer import HTMLViewer
except ImportError:
    print("无法导入HTMLViewer模块，请检查路径")
    sys.exit(1)


def read_markdown_file(file_path):
    """读取Markdown文件内容"""
    try:
        with open(file_path, "r", encoding="utf-8") as f:
            content = f.read()
        return content
    except Exception as e:
        print(f"读取Markdown文件时出错: {e}")
        return None


def create_latex_html(content):
    """直接创建包含LaTeX公式的HTML，不进行Markdown转换"""
    # 将内容封装在一个div中，保留原始格式
    html_content = f"""
    <div class="markdown-content">
        {content}
    </div>
    """
    return html_content


def create_full_html(html_content, raw_content):
    """创建完整的HTML，使用模板"""
    template_path = Path(parent_path) / "src" / "assets" / "template.html"

    try:
        with open(template_path, "r", encoding="utf-8") as f:
            template = f.read()
    except Exception as e:
        print(f"读取模板文件时出错: {e}")
        return None

    # 替换模板中的占位符
    template = template.replace("< !-- RENDERED_CONTENT_PLACEHOLDER -->", html_content)
    template = template.replace("< !-- RAW_CONTENT_PLACEHOLDER -->", raw_content)
    template = template.replace("< !-- FONT_SIZE_PLACEHOLDER -->", "20")  # 设置字体大小


    return template


def main():
    """主函数"""
    print("开始测试LaTeX公式渲染...")
    
    # 获取Markdown文件路径
    md_file_path = Path(current_path) / "test.md"
    
    if not md_file_path.exists():
        print(f"错误: 找不到测试文件 {md_file_path}")
        return
    
    # 读取Markdown内容
    md_content = read_markdown_file(md_file_path)
    if md_content is None:
        return
    
    # 创建带有LaTeX格式的HTML（不使用markdown转换，保留原始格式）
    html_content = create_latex_html(md_content)
    
    # 转义原始内容用于显示原文
    raw_content = md_content.replace("<", "&lt;").replace(">", "&gt;")
    
    # 创建完整HTML
    full_html = create_full_html(html_content, raw_content)
    if full_html is None:
        return
    
    # 使用HTMLViewer显示结果
    viewer = HTMLViewer()
    app = viewer.create_application()
    
    print("显示渲染结果...")
    window = viewer.show_html(
        html_content=full_html, title="LaTeX公式渲染测试", width=800, height=600
    )
    
    # 连接窗口关闭信号到应用程序退出
    window.destroyed.connect(lambda: app.quit())
    
    print("测试程序运行中，请查看渲染窗口。关闭窗口将退出程序。")
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()