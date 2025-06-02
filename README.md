# AI 截图翻译工具 (AI Screenshot Translator)

[![Python Version](https://img.shields.io/badge/python-3.8+-blue.svg)](https://www.python.org/downloads/) [![Nuitka](https://img.shields.io/badge/Nuitka-Compiled-purple)](https://nuitka.net/) [![Release Version](https://img.shields.io/badge/Release-v0.2-red)](https://github.com/Diraw/AI-Screenshot-Translator/releases/tag/v0.2-test)

# 简介

本工具通过**简单的截图操作**，将图片发送给 AI 模型进行文本识别和翻译，并将翻译结果以可交互的 HTML 格式显示在独立的窗口中。

本工具支持**自定义快捷键触发、多窗口结果管理以及系统托盘运行**，极大提升了日常工作和学习中的翻译效率。

**工具特点**：1、截图翻译，快捷键启动；2、贴片截图和翻译，可随意拖动、缩放，可创建多组翻译贴片；3、公式可以切换原始文本方便复制；4、自定义api接口

**想要解决的痛点**：1、目前市面上主流的整篇文档翻译的臃肿；2、若选择pdf复制文本粘贴翻译，有时候公式块是乱的或者无法复制到；3、对于图像扫描的pdf，无法复制文本

# 演示

![](./img/0.1.gif)

# 安装

您可以选择下载源码运行，或者前往 [Releases](https://github.com/Diraw/AI-Screenshot-Translator/releases) 界面下载可执行文件

### 1. 克隆仓库

```bash
git clone https://github.com/YourUsername/AI-Screenshot-Translator.git
cd AI-Screenshot-Translator/src
```

### 2. 修改配置信息

您可以通过编辑 `config.yaml` 文件自定义应用程序的行为：

```yaml
api:
  model: "qwen-vl-ocr-latest"        # 使用的AI模型
  prompt_text: "请将图中的英文翻译成中文后以中文回复文本，如果包含数学公式请用tex格式输出。" # 发送给模型的提示文本
  api_key: "YOUR_API_KEY_HERE"       # API密钥
  base_url: "https://dashscope.aliyuncs.com/compatible-mode/v1" # API服务地址

app_settings:
  max_windows: 0                     # 最大窗口数量，0表示无限制
  zoom_sensitivity: 500              # 缩放敏感度
  screenshot_hotkey: "ctrl+alt+s"    # 截图快捷键
  debug_mode: true                   # 是否启用调试模式
  initial_font_size: 24              # 结果窗口的默认字体大小
```
**注：**

- 请将 **YOUR_API_KEY_HERE** 替换为您的实际 API 密钥。
- 根据您选择的 AI 模型，**model** 和 **base_url** 可能需要相应调整。

> 开发过程使用的api为qwen，不知道怎么获取api_key可以查看 https://bailian.console.aliyun.com/?tab=model#/api-key ，qwen新注册用户每个模型可以免费领100w tokens，推荐使用

### 3. 创建虚拟环境并运行

```bash
conda create -n AI-Translator python=3.8
conda activate AI-Translator
pip install -r requirements.txt
python -m main.py
```

# 其他

- 软件icon来自 [iconfinder](https://www.iconfinder.com/search?q=screenshot&price=free)
- 特别感谢 Gemini 2.5 Flash 和 DeepSeek-V3-0324 帮我 fix bug