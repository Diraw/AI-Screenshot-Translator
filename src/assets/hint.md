## 一、API 设置

以qwen为例：

<img src="./assets/hint/v0.3.png" width="700">

## 二、代理地址

如果电脑装了网络代理软件，请在这里填入软件使用的本地HTTP代理端口（SOCKS也支持），http://主机名:端口号，不然软件会连不上网

- Clash默认是：http://127.0.0.1:7890
- v2rayN默认是：http://127.0.0.1:10808

clash：

<img src="./assets/hint/clash.png" width="700">

v2rayN（注意不是主界面显示的远程端口）：

<img src="./assets/hint/v2rayN_1.png" width="700">

<img src="./assets/hint/v2rayN_2.png" width="700">

## 三、截图快捷键

软件通过keyboard库注册快捷键，配置规则如下：

### 1、基本格式

使用+号连接多个按键名称

### 2、修饰键

可以是以下组合之一或多个

- ctrl：Control键
- alt：Alt键
- shift：Shift键
- win 或 super：Windows键/超级键
- meta：Meta键（在Mac上是Command键）

### 3、普通按键

- 字母键：直接使用字母，如a、b、c等
- 数字键：直接使用数字，如1、2、3等
- 功能键：使用f1、f2等表示
- 特殊键：例如space(空格)、tab、esc、enter等

### 4、默认设置

ctrl+alt+s表示同时按下Control键、Alt键和字母S键

## 四、其他设置

1. 初始字体大小：翻译窗口显示的初始字体大小
2. 边框颜色：使用RGB数值，默认是(100,100,100)灰色
3. 最大窗口数量：超过该数量会按创建顺序删除窗口，0代表不限制
4. 缩放敏感度：数值越大灵敏度越高
5. 调试模式：开启后会在exe所在目录下生成logs目录，存放调试信息