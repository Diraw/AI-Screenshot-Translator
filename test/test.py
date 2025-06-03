from pathlib import Path
import sys
import os
import re
import markdown

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

def convert_markdown_to_html(markdown_text):
    """将Markdown转换为HTML，保留数学公式"""
    # 第1步：保存数学表达式以避免被markdown处理
    # 正则表达式匹配内联公式和块级公式
    inline_math = re.compile(r'(?<!\$)\$(?!\$)(.*?)(?<!\$)\$(?!\$)', re.DOTALL)
    block_math = re.compile(r'\$\$(.*?)\$\$', re.DOTALL)
    
    # 临时替代文本
    inline_placeholders = {}
    block_placeholders = {}
    
    # 替换内联公式
    i = 0
    def replace_inline(match):
        nonlocal i
        placeholder = f"INLINE_MATH_{i}"
        inline_placeholders[placeholder] = match.group(0)
        i += 1
        return placeholder
    
    # 替换块级公式
    j = 0
    def replace_block(match):
        nonlocal j
        placeholder = f"BLOCK_MATH_{j}"
        block_placeholders[placeholder] = match.group(0)
        j += 1
        return placeholder
    
    # 应用替换
    text = inline_math.sub(replace_inline, markdown_text)
    text = block_math.sub(replace_block, text)
    
    # 第2步：处理代码块，避免代码块中的内容被误处理
    # 匹配代码块 ```lang ... ```
    code_block_pattern = re.compile(r'```(.+?)\n(.*?)```', re.DOTALL)
    code_placeholders = {}
    
    # 替换代码块
    k = 0
    def replace_code_block(match):
        nonlocal k
        lang = match.group(1).strip()
        code = match.group(2)
        placeholder = f"CODE_BLOCK_{k}"
        code_placeholders[placeholder] = (lang, code)
        k += 1
        return placeholder
    
    text = code_block_pattern.sub(replace_code_block, text)
    
    # 第3步：转换markdown为HTML
    try:
        html = markdown.markdown(text, extensions=['extra', 'tables', 'nl2br'])
    except Exception as e:
        print(f"Markdown转换错误: {e}")
        html = f"<p>Markdown转换错误: {e}</p><pre>{markdown_text}</pre>"
    
    # 第4步：恢复数学表达式
    for placeholder, formula in inline_placeholders.items():
        html = html.replace(placeholder, formula)
    
    for placeholder, formula in block_placeholders.items():
        html = html.replace(placeholder, formula)
    
    # 第5步：恢复代码块并添加语法高亮
    for placeholder, (lang, code) in code_placeholders.items():
        # 创建带有语法高亮的代码块
        highlighted_code = f'<pre><code class="language-{lang}">{code.replace("<", "&lt;").replace(">", "&gt;")}</code></pre>'
        html = html.replace(placeholder, highlighted_code)
    
    return html

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
    template = template.replace("<!-- RENDERED_CONTENT_PLACEHOLDER -->", html_content)
    template = template.replace("<!-- RAW_CONTENT_PLACEHOLDER -->", raw_content)
    
    # 添加语法高亮支持
    prism_css = """
    <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/prismjs@1.29.0/themes/prism.min.css">
    """
    prism_js = """
    <script src="https://cdn.jsdelivr.net/npm/prismjs@1.29.0/components/prism-core.min.js"></script>
    <script src="https://cdn.jsdelivr.net/npm/prismjs@1.29.0/plugins/autoloader/prism-autoloader.min.js"></script>
    """
    
    # 添加到head和body
    template = template.replace("</head>", f"{prism_css}</head>")
    template = template.replace("</body>", f"{prism_js}</body>")
    
    # 设置字体大小
    template = template.replace("<!-- FONT_SIZE_PLACEHOLDER -->", "16")
    
    return template

def preprocess_latex_formulas(markdown_text):
    """预处理LaTeX公式，确保它们能被正确识别并渲染"""
    # 统一替换 \(...\) 为 $...$ 和 \[...\] 为 $$...$$
    text = markdown_text
    
    # 处理 \(...\) 格式
    pattern_inline = re.compile(r'\\\((.*?)\\\)', re.DOTALL)
    text = pattern_inline.sub(r'$\1$', text)
    
    # 处理 \[...\] 格式
    pattern_block = re.compile(r'\\\[(.*?)\\\]', re.DOTALL)
    text = pattern_block.sub(r'$$\1$$', text)
    
    return text

def main():
    """主函数"""
    print("开始测试Markdown渲染...")
    
    # 获取Markdown文件路径
    md_file_path = Path(current_path) / "test.md"
    
    if not md_file_path.exists():
        print(f"错误: 找不到测试文件 {md_file_path}")
        return
    
    # 读取Markdown内容
    md_content = read_markdown_file(md_file_path)
    if md_content is None:
        return
    
    # 预处理LaTeX公式
    md_content = preprocess_latex_formulas(md_content)
    
    # 转换Markdown为HTML
    html_content = convert_markdown_to_html(md_content)
    
    # 转义原始内容
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
        html_content=full_html,
        title="Markdown渲染测试",
        width=900,
        height=700
    )
    
    # 关闭窗口后退出程序
    window.destroyed.connect(app.quit)
    
    print("测试程序运行中，请查看渲染窗口。关闭窗口将退出程序。")
    sys.exit(app.exec_())

if __name__ == "__main__":
    main()