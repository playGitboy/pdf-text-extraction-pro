# PDF 文本行合并方案

## 一、问题背景

从 PDF 中提取文本后，同一段落中的各行被错误分割，需要智能合并。同时需保留文档结构元素（标题、条款序号等）不被合并。

### 核心需求

1. **合并**：同一段落内被错误分割的行
2. **保留**：章节标题、条款序号、列表项等结构元素独立成行
3. **支持**：中文、英文、中英混合文档
4. **处理**：英文连字符断词、缩写后换行等特殊情况

---

## 二、核心判断逻辑

### 2.1 不合并的情况（保护段落边界）

```
上一行结尾是以下情况时，不与下一行合并：

1. 强终止符：。！？.!?…
   - 例：这是第一句。\n第二句。 → 保持两行

2. 分句终止符：；;：:
   - 例：条件如下；\n具体内容见附件。 → 保持两行

3. 标题行（行首匹配结构模式）：
   - 中文序号：第一条、一、1.、1.1、（一）
   - 英文序号：A.、I.、Article、Section
   - 项目符号：•、-、①
   - 关键词：鉴于、甲方、附件一、Abstract

4. 独立括号注释行：
   - 例：（见附件一）\n详细内容... → 保持两行

5. 下一行是结构元素开头：
   - 例：正文内容\n第一条 ... → 保持两行
```

### 2.2 需要合并的情况

```
上一行结尾是以下情况时，与下一行合并：

1. 中文逗号/顿号：，、
   - 例：甲方、\n乙方 → 甲方、乙方

2. 英文逗号：
   - 例：However,\nthe results → However, the results（加空格）

3. 中文字符（无标点）：
   - 例：签署后\n三十日内 → 签署后三十日内（不加空格）

4. 英文单词/数字：
   - 例：fox\njumps → fox jumps（加空格）

5. 英文缩写后：
   - 例：Mr.\nSmith → Mr. Smith（加空格）
```

---

## 三、关键正则表达式模式

### 3.1 结构元素识别模式

```regex
中文章节序号：
第[一二三四五六七八九十百千零〇\d]+[章节条款项部分编篇]

中文数字序号：
[一二三四五六七八九十百零〇]+[、.．]

阿拉伯数字序号：
\d+[\.．、]

多级数字序号：
\d+(\.\d+)+\.?

英文字母序号：
[A-Za-z][\.．)]

罗马数字序号：
[IVXLCDM]{1,5}[\.．]

带圈数字：
[①②③④⑤⑥⑦⑧⑨⑩⑪⑫⑬⑭⑮⑯⑰⑱⑲⑳]

项目符号：
[•·※☆★○●◇◆□■△▲►➤➢✓✔✗✘➊➋➌➍➎]

破折号列表：
[-—–]\s
```

### 3.2 关键词模式

```regex
中文关键词（可带序号）：
(?:甲方|乙方|丙方|丁方|鉴于|总则|附则|附件|附录|前言|序言|摘要|目录|引言|定义|释义|说明|备注|签署|签章|盖章|生效|失效|终止|解除|修订|补充|范围|目的|适用|出卖人|买受人|委托人|受托人|出租人|承租人|担保人|抵押人|质权人|债权人|债务人|代理人|法定代表人|授权人|被授权人|转让人|受让人|开发商|承包商|施工方|监理人|审计人|管理人|经办人|负责人|联系人|甲方代理人|乙方代理人)[一二三四五六七八九十百千零〇\d]*(?:[：:（(]|$)

中文字段标签（通用模式）：
[\u4e00-\u9fff\u3007]+[：:]

英文关键词：
(?:Article|Section|Chapter|Part|Appendix|Annex|Schedule|Exhibit|Clause|Item|Paragraph|Subsection|Abstract|Introduction|Conclusion|References|Bibliography|Acknowledgments|Contents|Summary)(?:\s|$)
```

### 3.3 英文缩写列表

```
Mr, Mrs, Ms, Miss, Dr, Prof, Rev, Hon,
Jr, Sr, St, vs, etc, No, Vol, Fig,
pp, cf, al, ed, approx, dept, est,
Jan, Feb, Mar, Apr, Jun, Jul, Aug,
Sep, Oct, Nov, Dec,
Ave, Blvd, Rd, Mt, Inc, Ltd, Co, Corp,
e.g, i.e, viz, nb, q.v, s.v,
U.S, U.K, E.U, P.O
```

### 3.4 行合并正则

```regex
修复英文连字符断词：
(\w)-[ \t]*\n([ \t]*[a-z])  →  \1\2

修复英文缩写后换行：
\b(Abbreviation)\.[ \t]*\n  →  \1. 

合并中文顿号后换行：
([、])\s*\n\s*  →  \1

合并中文逗号后换行：
([，])\s*\n\s*  →  \1

合并英文逗号后换行（加空格）：
(,)\s*\n\s*  →  \1 

合并CJK字符间换行：
([\u4e00-\u9fff])\s*\n\s*([\u4e00-\u9fff])  →  \1\2

合并英文单词间换行（加空格）：
([A-Za-z0-9)])\s*\n\s*([A-Za-z])  →  \1 \2
```

### 3.5 清理规则

```regex
清理中文标点前空格：
 ([。！？；：，、\u201c\u201d\u2018\u2019》】〕）)…])  →  \1

清理英文右标点前空格：
 (?=[.,;:!?\'\u2019\u201d)\]}])  →  (空)

合并多余空格：
(?<=\S) {2,}  →  (单个空格)

合并多余空行：
\n{2,}  →  \n
```

---

## 四、算法流程

### 步骤 1：预处理

```
1. 统一行尾：\r\n 和 \r → \n
2. 修复英文连字符断词：impor-\ntant → important
3. 修复英文缩写后换行：Mr.\nSmith → Mr. Smith
```

### 步骤 2：逐行判断合并

```
对于每一对相邻行 (prev, curr)：

IF 任一行为空行：
    不合并，保持 curr 独立

ELSE IF prev 结尾是强终止符（。！？.!?…）且不是缩写：
    不合并，保持 curr 独立

ELSE IF prev 结尾是分句终止符（；;：:）：
    不合并，保持 curr 独立

ELSE IF prev 是标题行（匹配结构模式，不含字段标签）：
    不合并，保持 curr 独立

ELSE IF prev 含多列布局（行内≥4连续空格）：
    不合并，保持 curr 独立

ELSE IF prev 是独立括号注释行：
    不合并，保持 curr 独立

ELSE IF curr 以结构元素开头（含字段标签如"房屋编号："）：
    不合并，保持 curr 独立

ELSE IF curr 以缩进开头（>3空格）：
    不合并，保持 curr 独立

ELSE IF curr 以少量缩进开头（≤3空格）：
    合并两行（续行场景，如跨行数字/中文）
    根据字符类型决定是否加空格（数字续行不加空格）

ELSE：
    合并两行
    根据字符类型决定是否加空格
```

**多列布局拆分**：在合并循环之前，先检测行内是否有≥4个连续空格（多列标志），将一行拆分为多个独立行。例如 `GF-2014-0171                                    合同编号： YS202302157234` 拆分为 `GF-2014-0171` 和 `合同编号： YS202302157234`。

### 步骤 3：空格判断逻辑

```
判断是否需要在合并处加空格：

IF 上一行末尾是ASCII数字 AND 下一行开头是ASCII数字：
    不加空格（796032 + 67 → 79603267，数字续行场景）

ELSE IF 上一行末尾是ASCII字母数字 AND 下一行开头是ASCII字母数字：
    加空格（fox + jumps → fox jumps）

ELSE IF 上一行末尾是右引号/右括号/句点 AND 下一行开头是ASCII字母：
    加空格（) + shall → ) shall）

ELSE IF 上一行末尾是英文标点(,;:!?) AND 下一行开头是ASCII字母或CJK字符：
    加空格（, + the → , the）

ELSE IF 上一行末尾是CJK标点 AND 下一行开头是ASCII字母：
    加空格（，+ the → ， the）

ELSE：
    不加空格（中文间、中英间直接连接）
```

### 步骤 4：后处理清理

```
1. 清理中文标点前的多余空格
2. 清理英文右标点前的多余空格
3. 合并连续多个空格为单个空格
4. 合并连续多个空行为单个换行
```

---

## 五、测试用例

### 5.1 中文合同

```python
# 条款序号独立，正文合并
输入: "第一条 本合同的目的\n为规范双方合作事宜，\n特制定本合同。"
输出: "第一条 本合同的目的\n为规范双方合作事宜，特制定本合同。"

# 连续正文合并
输入: "甲方应在本合同签署后\n三十日内支付首期款项，\n乙方收到款项后出具收据。"
输出: "甲方应在本合同签署后三十日内支付首期款项，乙方收到款项后出具收据。"

# 鉴于关键词独立
输入: "鉴于\n甲方拥有相关技术，\n乙方具备市场推广能力，\n双方达成如下协议。"
输出: "鉴于\n甲方拥有相关技术，乙方具备市场推广能力，双方达成如下协议。"

# 顿号后合并
输入: "甲方、\n乙方应共同遵守。"
输出: "甲方、乙方应共同遵守。"

# 附件关键词独立
输入: "附件一\n技术规格说明书\n附则\n本合同自签署之日起生效。"
输出: "附件一\n技术规格说明书\n附则\n本合同自签署之日起生效。"
```

### 5.2 英文文档

```python
# 简单断行合并
输入: "The quick brown fox\njumps over the lazy dog."
输出: "The quick brown fox jumps over the lazy dog."

# 句号后不合并
输入: "This is the first sentence\nof the paragraph.\nThis is the second sentence."
输出: "This is the first sentence of the paragraph.\nThis is the second sentence."

# 缩写后合并
输入: "Mr. Smith went to\nthe meeting yesterday."
输出: "Mr. Smith went to the meeting yesterday."

# 连字符断词
输入: "This is an impor-\ntant finding."
输出: "This is an important finding."

# 条款序号独立
输入: "A. Introduction\nThis paper presents a novel approach.\nB. Methodology\nWe use deep learning."
输出: "A. Introduction\nThis paper presents a novel approach.\nB. Methodology\nWe use deep learning."
```

### 5.3 中英混合

```python
# 英文单词后接中文
输入: "公司采用Agile\n开发模式进行项目管理。"
输出: "公司采用Agile开发模式进行项目管理。"

# 章节序号独立
输入: "第3章 System Design\n本章描述系统架构。\n3.1 数据库设计\n采用MySQL数据库。"
输出: "第3章 System Design\n本章描述系统架构。\n3.1 数据库设计\n采用MySQL数据库。"

# 英文论文结构
输入: "Abstract\nThis paper proposes a new method\nfor text processing.\nIntroduction\nWe begin with an overview."
输出: "Abstract\nThis paper proposes a new method for text processing.\nIntroduction\nWe begin with an overview."
```

### 5.4 特殊情况

```python
# 独立括号注释
输入: "（见附件一）\n详细内容如下。"
输出: "（见附件一）\n详细内容如下。"

# 行尾引号
输入: 'He said "Hello."\nShe replied.'
输出: 'He said "Hello."\nShe replied.'

# 分号后不合并
输入: "条件如下；\n具体内容见附件。"
输出: "条件如下；\n具体内容见附件。"

# 冒号后不合并
输入: "注意事项：\n请仔细阅读以下内容。"
输出: "注意事项：\n请仔细阅读以下内容。"

# 项目符号列表
输入: "• 第一项内容\n• 第二项内容\n• 第三项内容"
输出: "• 第一项内容\n• 第二项内容\n• 第三项内容"

# 带圈数字序号
输入: "①基本要求\n应符合国家标准。\n②附加要求\n可参考行业规范。"
输出: "①基本要求\n应符合国家标准。\n②附加要求\n可参考行业规范。"
```

---

## 六、实现要点

### 6.1 判断优先级

```
1. 先检查"不合并"条件（保护结构）
2. 再执行合并操作
3. 最后清理格式
```

### 6.2 字符类型判断

```python
def is_ascii_word_char(ch):
    return ('a' <= ch <= 'z') or ('A' <= ch <= 'Z') or ('0' <= ch <= '9')

def is_cjk_char(ch):
    return '\u4e00' <= ch <= '\u9fff' or '\u3400' <= ch <= '\u4dbf' or '\uf900' <= ch <= '\ufaff'
```

### 6.3 行尾引号/括号处理

```
判断句子终止符时，需跳过行尾的引号和括号：
- "Hello." → 实际终止符是 .
- （见附件一）→ 检查右括号前的内容

跳过字符集："\u201c\u201d\'\u2018\u2019》】〕）)』」\u300d\u300f
```

### 6.4 缩写识别

```
检查行尾是否为"缩写."形式：
1. 行尾必须是 .
2. . 前面匹配已知缩写列表
3. 缩写前不能是字母（避免误匹配单词的一部分）
```

---

## 七、扩展建议

### 7.1 可扩展的结构模式

```regex
添加新的序号格式：
- 希腊字母：[αβγδεζηθικλμνξοπρστυφχψω]

添加新的关键词：
- 法律文档：原告、被告、上诉人、被上诉人
- 技术文档：环境要求、系统配置、接口说明
```

### 7.2 激进模式

```
当 aggressive=True 时：
- 合并缩进开头的行（适用于某些PDF格式）
- 更积极地合并短行
```

### 7.3 性能优化

```
1. 预编译所有正则表达式
2. 使用生成器处理大文件
3. 批量处理而非逐字符检查
```

---

## 八、调用示例

### Python 调用

```python
from merge_lines import merge_pdf_lines

text = """第一条 总则
本合同旨在明确
双方的权利义务。

第二条 适用范围
本合同适用于
所有合作项目。"""

result = merge_pdf_lines(text)
print(result)
```

### 输出结果

```
第一条 总则
本合同旨在明确双方的权利义务。

第二条 适用范围
本合同适用于所有合作项目。
```
