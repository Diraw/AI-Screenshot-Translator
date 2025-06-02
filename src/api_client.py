from openai import OpenAI
import base64
import markdown
import os

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
                        "image_url": f"data:image/png;base64,{image_data_base64}",  # 直接使用传入的base64数据
                    },
                    {"type": "text", "text": prompt_text},
                ],
            },
        ],
    )
    return completion.choices[0].message.content

def create_unified_html(markdown_text, template_path="./assets/template.html", font_size: int = 16):
    """从模板文件加载HTML，并用渲染内容和原始文本填充占位符"""
    try:
        # 确保 template_path 是一个有效的文件路径
        if not os.path.exists(template_path):
            print(f"错误: 模板文件 '{template_path}' 未找到。")
            return "<h1>错误: HTML 模板文件未找到。</h1><p>请确保 ./assets/template.html 文件存在于正确的位置。</p>"

        with open(template_path, "r", encoding="utf-8") as f:
            html_template = f.read()
    except Exception as e: # 捕获更广泛的异常
        print(f"读取模板文件 '{template_path}' 时发生错误: {e}")
        return f"<h1>错误: 读取 HTML 模板文件失败。</h1><p>详细信息: {e}</p>"

    # 渲染后的 Markdown/LaTeX 内容
    rendered_html_content = markdown.markdown(markdown_text, extensions=["fenced_code"])

    # 原始文本，需要进行 HTML 转义
    escaped_raw_text = (
        markdown_text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
    )

    # 替换占位符
    full_html = html_template.replace(
        "<!-- RENDERED_CONTENT_PLACEHOLDER -->", rendered_html_content
    )
    full_html = full_html.replace("< !-- RAW_CONTENT_PLACEHOLDER -->", escaped_raw_text)
    full_html = full_html.replace("< !-- FONT_SIZE_PLACEHOLDER -->", str(font_size))

    return full_html


class APIClient:
    """API 客户端类，封装所有 API 相关操作"""

    def __init__(self, api_key=None, base_url=None, model_name=None):
        """初始化 API 客户端"""
        self.api_key = api_key
        self.base_url = base_url
        self.model_name = model_name
        self.html_template_path = "./assets/template.html" # 默认值

    def set_html_template_path(self, path):
        """设置 HTML 模板文件的路径"""
        self.html_template_path = path

    def process_image(self, image_data_base64, prompt_text):
        """处理图片并返回模型响应"""
        return get_model_response(
            image_data_base64, prompt_text, self.api_key, self.base_url, self.model_name
        )

    def create_html_content(self, markdown_text, initial_font_size: int = 16):
        """创建 HTML 内容，使用内部存储的模板路径"""
        return create_unified_html(markdown_text, self.html_template_path, initial_font_size)