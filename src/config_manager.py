import yaml
import os
from collections import OrderedDict

# 让YAML保持字典顺序的辅助类
def represent_ordereddict(dumper, data):
    items = []
    for key, value in data.items():
        items.append((dumper.represent_data(key), dumper.represent_data(value)))
    return yaml.nodes.MappingNode('tag:yaml.org,2002:map', items)

class ConfigManager:
    def __init__(self, config_file="config.yaml"):
        self.config_file = config_file
        # 注册OrderedDict的表示方法
        yaml.add_representer(OrderedDict, represent_ordereddict)

        if not os.path.exists(self.config_file):
            print(f"配置文件 '{self.config_file}' 不存在，正在创建默认配置文件...")
            default_config = self.get_default_config()
            self.save_config(default_config) # 保存默认配置
            self.config_data = default_config # 加载新创建的默认配置
        else:
            self.config_data = self._load_config()


    def _load_config(self):
        try:
            with open(self.config_file, 'r', encoding='utf-8') as f:
                # 加载配置时转换为普通的 dict，因为保存时会处理顺序
                return yaml.safe_load(f)
        except yaml.YAMLError as e:
            print(f"Error loading YAML file: {e}")
            return None # 如果加载失败，返回 None
        except Exception as e:
            print(f"Unexpected error loading YAML file: {e}")
            return None

    def save_config(self, data):
        self.config_data = data
        try:
            # 使用OrderedDict保存配置，根据默认模板的顺序
            config_to_save = self._convert_to_ordered_dict(data, self.get_default_config())
            with open(self.config_file, 'w', encoding='utf-8') as f:
                yaml.dump(config_to_save, f, allow_unicode=True, indent=2, sort_keys=False)
            return True
        except Exception as e:
            print(f"Error saving YAML file: {e}")
            return False
    
    def _convert_to_ordered_dict(self, data, template):
        """根据模板的顺序创建OrderedDict"""
        result = OrderedDict()
        
        for key in template:
            if key in data:
                if isinstance(data[key], dict) and isinstance(template[key], dict):
                    # 递归处理嵌套字典
                    result[key] = self._convert_to_ordered_dict(data[key], template[key])
                else:
                    result[key] = data[key]
            elif key in template:
                result[key] = template[key]

        for key in data:
            if key not in result:
                result[key] = data[key]
                
        return result

    def get_config(self):
        # 直接返回内部存储的配置数据
        return self.config_data

    def config_exists(self):
        return os.path.exists(self.config_file)

    def get_default_config(self):
        # 提供一个默认配置结构，方便新用户填写
        return {
            "api": {
                "model": "",
                "prompt_text": "请将图中的英文翻译成中文后以中文回复文本，如果包含数学公式请用tex格式输出。",
                "api_key": "",
                "base_url": "",
                "proxy": ""
            },
            "app_settings": {
                "screenshot_hotkey": "ctrl+alt+s",
                "initial_font_size": 24,
                "card_border_color": "100,100,100",
                "max_windows": 0,
                "zoom_sensitivity": 500,
                "debug_mode": False,
            }
        }