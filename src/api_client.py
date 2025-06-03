from openai import OpenAI
import base64
import markdown
import os
import re

def encode_image(image_path):
    """将图片编码为 base64 格式"""
    with open(image_path, "rb") as image_file:
        return base64.b64encode(image_file.read()).decode("utf-8")


def get_model_response(image_data_base64, prompt_text, api_key=None, base_url=None, model_name=None):
    """获取模型响应"""
    if api_key is None:
        raise ValueError("API key must be provided.")

    if base_url is None:
        raise ValueError("Base URL must be provided.")
    
    if model_name is None:
        raise ValueError("Model name must be provided.")

    client = OpenAI(
        api_key=api_key,
        base_url=base_url,
    )

    completion = client.chat.completions.create(
        model=model_name,
        messages=[
            {
                "role": "system",
                "content": [{"type": "text", "text": "You are a helpful assistant."}],
            },
            {
                "role": "user",
                "content": [
                    {
                        "type": "image_url",
                        "image_url": f"data:image/png;base64,{image_data_base64}",
                    },
                    {"type": "text", "text": prompt_text},
                ],
            },
        ],
    )
    return completion.choices[0].message.content


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


def convert_markdown_to_html(markdown_text):
    """将Markdown转换为HTML，保留数学公式"""
    # 预处理LaTeX公式，统一格式
    markdown_text = preprocess_latex_formulas(markdown_text)
    
    # 第1步：保存数学表达式以避免被markdown处理
    # 正则表达式匹配内联公式和块级公式 - 增强版本
    inline_math = re.compile(r'(?<!\\)(\\\(.*?\\\)|\$[^\$\n]+?\$)', re.DOTALL)
    block_math = re.compile(r'(?<!\\)(\\\[.*?\\\]|\$\$.+?\$\$)', re.DOTALL)
    
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


def create_unified_html(markdown_text, template_path="./assets/template.html", font_size: int = 16):
    """从模板文件加载HTML，并用渲染内容和原始文本填充占位符"""
    try:
        # 确保 template_path 是一个有效的文件路径
        if not os.path.exists(template_path):
            print(f"错误: 模板文件 '{template_path}' 未找到。")
            return "<h1>错误: HTML 模板文件未找到。</h1><p>请确保 template.html 文件存在于正确的位置。</p>"

        with open(template_path, "r", encoding="utf-8") as f:
            html_template = f.read()
    except Exception as e: # 捕获更广泛的异常
        print(f"读取模板文件 '{template_path}' 时发生错误: {e}")
        return f"<h1>错误: 读取 HTML 模板文件失败。</h1><p>详细信息: {e}</p>"

    # 将Markdown转换为HTML，保留数学公式
    html_content = convert_markdown_to_html(markdown_text)
    
    # 转义原始文本以便在原始视图中显示
    raw_content = markdown_text.replace("<", "&lt;").replace(">", "&gt;")
    
    # 添加语法高亮支持
    prism_css = """
    <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/prismjs@1.29.0/themes/prism.min.css">
    """
    prism_js = """
    <script src="https://cdn.jsdelivr.net/npm/prismjs@1.29.0/components/prism-core.min.js"></script>
    <script src="https://cdn.jsdelivr.net/npm/prismjs@1.29.0/plugins/autoloader/prism-autoloader.min.js"></script>
    """
    
    # 替换占位符
    full_html = html_template.replace("<!-- RENDERED_CONTENT_PLACEHOLDER -->", html_content)
    full_html = full_html.replace("<!-- RAW_CONTENT_PLACEHOLDER -->", raw_content)
    full_html = full_html.replace("<!-- FONT_SIZE_PLACEHOLDER -->", str(font_size))
    
    # 添加语法高亮支持
    full_html = full_html.replace("</head>", f"{prism_css}</head>")
    full_html = full_html.replace("</body>", f"{prism_js}</body>")
    
    return full_html


class APIClient:
    """API 客户端类，封装所有 API 相关操作"""

    def __init__(self, api_key=None, base_url=None, model_name=None):
        """初始化 API 客户端"""
        self.api_key = api_key
        self.base_url = base_url
        self.model_name = model_name
        self.html_template_path = "./assets/template.html"

    def set_html_template_path(self, path):
        """设置 HTML 模板文件的路径"""
        if os.path.exists(path):
            self.html_template_path = path
        else:
            print(f"警告: 模板路径 '{path}' 不存在，继续使用当前路径: {self.html_template_path}")

    def process_image(self, image_data_base64, prompt_text):
        """处理图片并返回模型响应"""
        return get_model_response(
            image_data_base64, prompt_text, self.api_key, self.base_url, self.model_name
        )

    def create_html_content(self, markdown_text, initial_font_size: int = 16):
        """创建 HTML 内容，使用内部存储的模板路径"""
        return create_unified_html(markdown_text, self.html_template_path, initial_font_size)