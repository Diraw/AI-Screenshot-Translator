# Markdown 渲染测试文档

## 0. 数学公式

设 $\vec{A}=A_{x}\hat{i}+A_{y}\hat{j}+A_{z}\hat{k}$ 是调和场，有
$$
\nabla \cdot \vec{A}=\frac{\partial A_{x}}{\partial x}+\frac{\partial A_{y}}{\partial y}+\frac{\partial A_{z}}{\partial z}=0
$$
$$
\nabla \times \vec{A}=\begin{vmatrix}
\hat{i}&\hat{j}&\hat{k} \\
\dfrac{{\partial}}{{\partial x}}&\dfrac{{\partial}}{{\partial y}}&\dfrac{{\partial}}{{\partial z}}\\
A_{x}&A_{y}&A_{z}
\end{vmatrix}=0
$$
得 $\dfrac{{\partial A_{z}}}{{\partial y}}=\dfrac{{\partial A_{y}}}{{\partial z}}\text{{，}}\dfrac{{\partial A_{z}}}{{\partial y}}=\dfrac{{\partial A_{y}}}{{\partial z}}\text{{，}}\dfrac{{\partial A_{z}}}{{\partial y}}=\dfrac{{\partial A_{y}}}{{\partial z}}$
以 $A_{x}$ 为例，有
$$
\begin{aligned}
\nabla^{2}A_{x}&=\frac{{\partial^{2}A_{x}}}{{\partial x^{2}}}+\frac{{\partial^{2}A_{y}}}{{\partial y^{2}}}+\frac{{\partial^{2}A_{z}}}{{\partial z^{2}}}\\
&=\frac{{\partial}}{{\partial x}}\left(-\frac{{\partial A_{y}}}{{\partial y}}-\frac{{\partial A_{z}}}{{\partial z}}\right)+\frac{{\partial}}{{\partial y}}\left(\frac{{\partial A_{x}}}{{\partial y}}\right)+\frac{{\partial}}{{\partial z}}\left(\frac{{\partial A_{x}}}{{\partial z}}\right)\\
&=\frac{{\partial}}{{\partial y}}\left( \frac{{\partial A_{x}}}{{\partial y}}-\frac{{\partial A_{y}}}{{\partial x}} \right)+\frac{{\partial}}{{\partial z}}\left( \frac{{\partial A_{x}}}{{\partial z}}-\frac{{\partial A_{z}}}{{\partial x}} \right)\\
&=0
\end{aligned}
$$
$$
\begin{align}
\nabla^{2}A_{x}&=\frac{{\partial^{2}A_{x}}}{{\partial x^{2}}}+\frac{{\partial^{2}A_{y}}}{{\partial y^{2}}}+\frac{{\partial^{2}A_{z}}}{{\partial z^{2}}}\\
&=\frac{{\partial}}{{\partial x}}\left(-\frac{{\partial A_{y}}}{{\partial y}}-\frac{{\partial A_{z}}}{{\partial z}}\right)+\frac{{\partial}}{{\partial y}}\left(\frac{{\partial A_{x}}}{{\partial y}}\right)+\frac{{\partial}}{{\partial z}}\left(\frac{{\partial A_{x}}}{{\partial z}}\right)\\
&=\frac{{\partial}}{{\partial y}}\left( \frac{{\partial A_{x}}}{{\partial y}}-\frac{{\partial A_{y}}}{{\partial x}} \right)+\frac{{\partial}}{{\partial z}}\left( \frac{{\partial A_{x}}}{{\partial z}}-\frac{{\partial A_{z}}}{{\partial x}} \right)\\
&=0
\end{align}
$$
设\(\vec{A} = A_x\hat{i} + A_y\hat{j} + A_z\hat{k}\)是调和场，有
\[
\nabla \cdot \vec{A} = \frac{\partial A_x}{\partial x} + \frac{\partial A_y}{\partial y} + \frac{\partial A_z}{\partial z} = 0
\]
\[
\nabla \times \vec{A} = \begin{vmatrix}
\hat{i} & \hat{j} & \hat{k} \\
\frac{\partial}{\partial x} & \frac{\partial}{\partial y} & \frac{\partial}{\partial z} \\
A_x & A_y & A_z
\end{vmatrix} = 0
\]
得\(\frac{\partial A_z}{\partial y} = \frac{\partial A_y}{\partial z}\)，\(\frac{\partial A_z}{\partial y} = \frac{\partial A_y}{\partial z}\)，\(\frac{\partial A_z}{\partial y} = \frac{\partial A_y}{\partial z}\)
以\(A_x\)为例，有
\[
\begin{align*}
\nabla^2 A_x &= \frac{\partial^2 A_x}{\partial x^2} + \frac{\partial^2 A_y}{\partial y^2} + \frac{\partial^2 A_z}{\partial z^2} \\
&= \frac{\partial}{\partial x}\left(-\frac{\partial A_y}{\partial y} - \frac{\partial A_z}{\partial z}\right) + \frac{\partial}{\partial y}\left(\frac{\partial A_x}{\partial y}\right) + \frac{\partial}{\partial z}\left(\frac{\partial A_x}{\partial z}\right) \\
&= \frac{\partial}{\partial y}\left(\frac{\partial A_x}{\partial y} - \frac{\partial A_y}{\partial x}\right) + \frac{\partial}{\partial z}\left(\frac{\partial A_x}{\partial z} - \frac{\partial A_z}{\partial x}\right) \\
&= 0
\end{align*}
\]
\[
\begin{align*}
\nabla^2 A_x &= \frac{\partial^2 A_x}{\partial x^2} + \frac{\partial^2 A_y}{\partial y^2} + \frac{\partial^2 A_z}{\partial z^2} \quad (1) \\
&= \frac{\partial}{\partial x}\left(-\frac{\partial A_y}{\partial y} - \frac{\partial A_z}{\partial z}\right) + \frac{\partial}{\partial y}\left(\frac{\partial A_x}{\partial y}\right) + \frac{\partial}{\partial z}\left(\frac{\partial A_x}{\partial z}\right) \quad (2) \\
&= \frac{\partial}{\partial y}\left(\frac{\partial A_x}{\partial y} - \frac{\partial A_y}{\partial x}\right) + \frac{\partial}{\partial z}\left(\frac{\partial A_x}{\partial z} - \frac{\partial A_z}{\partial x}\right) \quad (3) \\
&= 0 \quad (4)
\end{align*}
\]

## 1. 标题测试
# H1
## H2
### H3
#### H4
##### H5
###### H6

## 2. 文本样式
- **粗体**
- *斜体*
- ~~删除线~~
- `行内代码`
- 这是<sub>下标</sub>和<sup>上标</sup>

## 3. 列表测试
### 无序列表
- 项目 1
- 项目 2
  - 子项目 2.1
  - 子项目 2.2

### 有序列表
1. 第一项
2. 第二项
   1. 子项 2.1
   2. 子项 2.2

## 4. 链接和图片
[Google](https://www.google.com)

![](./img/0.1.gif)

## 5. 代码块
```python
def hello_world():
    print("Hello, Markdown!")
```

## 6. 表格

| 姓名 | 年龄 | 职业       | 评分  |
| ---- | ---- | ---------- | ----- |
| 张三 | 28   | 软件工程师 | ★★★★☆ |
| 李四 | 35   | 数据分析师 | ★★★☆☆ |
| 王五 | 42   | 产品经理   | ★★★★★ |
| 赵六 | 29   | UI设计师   | ★★☆☆☆ |

## 7. 引用

> 这是一个单行引用。

> 这是一个多行引用，
> 可以跨越多行文本，
> 并且会自动保持引用格式。

> 嵌套引用：
> > 这是第二层引用。
> > > 这是第三层引用。

## 8. 分割线

第一段

---

第二段

***

第三段

___

## 9. 任务列表

- [x] 完成项目需求分析  
- [ ] 编写核心代码  
- [ ] 进行单元测试  
- [ ] 部署到生产环境  
  - [x] 准备服务器  
  - [ ] 配置数据库  
- [ ] 编写文档 📝
