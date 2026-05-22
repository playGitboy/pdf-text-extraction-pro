# PDF Text Extraction 增强版

基于 [pdf-text-extraction](https://github.com/galkahana/pdf-text-extraction) 的增强版本，修复了多个影响中文/CJK用户的关键Bug，并新增叠词过滤和段落行智能合并功能。

## 与原版对比

| 特性 | 原版 | 增强版 |
|------|------|--------|
| 中文路径/文件名 | ❌ 报错 `Cannot read file` | ✅ 完整Unicode支持 |
| 页码指定 | ❌ 仅起止页 `-s` `-e` | ✅ `-p` 支持页码范围、混写 |
| 结构化输出 | ❌ 仅纯文本 | ✅ `-o` 根据扩展名自动输出HTML/JSON |
| 多文件处理 | ❌ 仅单文件 | ✅ 支持多文件/目录，递归扫描 |
| Windows终端中文 | ❌ 乱码 | ✅ UTF-8输出 |
| CJK叠词/重复字符 | ❌ 如"第第十十二二条条" | ✅ `-f` 参数过滤 |
| 段落行智能合并 | ❌ 每行独立输出 | ✅ `-m` 参数合并同段行 |
| 跨平台兼容 | ❌ 仅Windows | ✅ Windows/Linux/macOS，行尾和路径自适应 |
| 静态链接 | ❌ 依赖运行时库 | ✅ 全静态链接，零依赖 |
| 体积优化 | ❌ 含调试信息 | ✅ strip + UPX压缩 |

## 下载

从项目 [Releases](https://github.com/galkahana/pdf-text-extraction/releases) 页面下载预编译二进制文件，或自行编译（见下方编译章节）。

| 平台 | 说明 |
|------|------|
| Windows 7+ (x64) | MinGW 编译，全静态链接 |
| Linux (x64) | GCC 编译，需自行编译 |
| macOS (x64) | Clang 编译，需自行编译 |

## 用法

```bash
TextExtraction <路径1> [路径2] ... [选项]
```

路径可以是PDF文件或目录。指定目录时递归扫描其中所有PDF文件。路径和选项可以混写。

### 选项

| 参数 | 说明 |
|------|------|
| `-p, --pages <spec>` | 指定页码（1-based），支持多种格式，默认全部页面 |
| `-f, --filter-dup` | 过滤重复文本（修复CJK叠词问题） |
| `-m, --merge-lines` | 智能合并段落行（将PDF断行合并为连续段落） |
| `--spacing <BOTH\|HOR\|VER\|NONE>` | 空格插入策略，默认BOTH |
| `-t, --tables` | 提取表格为CSV |
| `-o, --output <路径>` | 输出到文件（根据扩展名自动选择格式：.html/.htm=HTML表格, .json=JSON, 其他=纯文本） |
| `-q, --quiet` | 静默模式 |
| `-d, --debug <路径>` | 调试输出 |
| `-h, --help` | 显示帮助信息 |

### 页码格式（`-p` 参数）

| 格式 | 示例 | 说明 |
|------|------|------|
| 单独页 | `-p 1,5,58` | 逗号分隔多个页码 |
| 页码范围 | `-p 2-24` | 连续页码范围 |
| 混合表达 | `-p 1,3-5,10-12` | 逗号分隔，支持单页和范围混写 |
| 全部页面 | 不指定 `-p` | 默认提取所有页面 |

### 输出格式（`-o` 参数扩展名）

| 扩展名 | 格式 | 说明 |
|------|------|------|
| `.html` / `.htm` | HTML表格 | 三列表格：文件绝对路径(20%)、页码(5%)、文本内容(75%) |
| `.json` | JSON数组 | `[{"file":"绝对路径", "pages":[{"page":N, "text":"..."}]}]` |
| 其他 / 无扩展名 | 纯文本 | 默认格式，与原版行为一致 |

### 常用示例

```bash
# 提取全部文本
TextExtraction input.pdf

# 提取第1页
TextExtraction input.pdf -p 1

# 提取第3到第5页，启用叠词过滤
TextExtraction input.pdf -p 3-5 -f

# 提取第1、3、5页
TextExtraction input.pdf -p 1,3,5

# 混合表达：第1页、第3到5页、第10到12页，同时过滤和合并
TextExtraction input.pdf -p 1,3-5,10-12 -f -m

# 启用段落行合并
TextExtraction input.pdf -m

# 同时启用叠词过滤和段落行合并，保存到文件
TextExtraction input.pdf -f -m -o output.txt

# 输出为HTML表格（按页码分列显示）
TextExtraction input.pdf -p 1,3-5 -f -o result.html

# 输出为JSON格式（结构化数据，便于程序处理）
TextExtraction input.pdf -f -m -o result.json

# 处理多个PDF文件
TextExtraction file1.pdf file2.pdf file3.pdf -f -o result.json

# 递归扫描目录下所有PDF文件
TextExtraction /path/to/pdfs/ -f -o result.html

# 混写：文件和目录组合
TextExtraction file1.pdf /path/to/more/ -p 1-5 -f -m -o result.json

# 提取表格
TextExtraction input.pdf -t -o tables.csv

# 处理中文文件名
TextExtraction "合同.pdf" -f -m -o 合同文本.html
```

## Bug修复与增强详情

### 1. 中文路径/文件名支持

**问题**：文件路径包含中文字符时报错 `Cannot read file 测试.pdf`

**原因**：Windows下 `main(int argc, char* argv[])` 使用系统默认编码解析命令行参数，非UTF-8环境下中文被截断

**修复**：使用 `CommandLineToArgvW()` 获取宽字符参数，再转换为UTF-8

### 2. 灵活页码指定

**问题**：原版仅支持 `-s` 起始页和 `-e` 结束页，无法指定不连续的页面

**修复**：合并 `-s`/`-e` 为 `-p`/`--pages` 参数，支持多种页码格式：
- 单独页：`-p 1,5,58`
- 页码范围：`-p 2-24`
- 混合表达：`-p 1,3-5,10-12`
- 不指定 `-p` 默认提取全部页面
- 页码为1-based（第1页=1），自动转换为内部0-based索引

### 3. 多文件/目录处理

**问题**：原版仅支持单文件处理，无法批量处理多个PDF或目录

**修复**：支持多文件、目录、混写输入：
- 多文件：`TextExtraction file1.pdf file2.pdf -o result.json`
- 目录：递归扫描所有PDF文件
- 混写：文件和目录可混合指定
- 错误文件自动跳过，不影响其他文件处理
- 多文件输出：HTML/JSON合并所有文件结果，纯文本用 `---` 分隔

### 4. Windows终端中文正常显示

**问题**：提取的中文在终端显示为乱码

**原因**：程序输出UTF-8编码，但Windows终端默认使用GBK编码

**修复**：启动时设置 `SetConsoleOutputCP(CP_UTF8)` 和 `_setmode(_fileno(stdout), _O_BINARY)`

### 5. CJK叠词过滤

**问题**：部分PDF提取结果出现叠词，如"第第十十二二条条 逾逾期期交交付付责责任任"

**原因**：PDF内容流中同一文本被绘制了两次（阴影/描边效果或重叠文本层），两次绘制的字符位置接近（中心距离 < 1.5倍字高）

**修复**：新增基于中心距离的去重算法，通过 `-f` 参数启用。算法检测相同文本内容且位置接近的文本放置，保留第一个，过滤后续重复项

### 6. 段落行智能合并

**问题**：PDF提取的文本每行独立输出，同一段落被错误分割为多行

**修复**：新增 `-m` 参数启用段落行智能合并。算法识别文档结构元素（条款序号、标题、列表项等）保持独立，同时合并同一段落内被错误分割的行。支持中文、英文、中英混合文档，正确处理英文连字符断词和缩写

**合并规则**：
- ✅ 合并：中文间换行、英文单词间换行、逗号后换行、顿号后换行、少量缩进续行、数字续行
- ❌ 不合并：句号/问号/感叹号后换行、条款序号行、标题行、项目符号行、字段标签行（如"房屋编号："）
- 📐 多列布局：行内≥4连续空格自动拆分为独立行
- 📄 连续空行：所有输出格式均合并连续空行为单个换行

**结构检测**（CJK部分不使用正则，手动解析UTF-8字符确保跨平台兼容）：
- 中文章节编号：第X条/章/节
- 阿拉伯/中文数字列表：1. / 一、
- 多级编号：1.1 / 1.1.1
- 括号编号：（一）/ (1)
- 圈号：①②③
- 项目符号/破折号列表：• / — / -
- 字段标签：1-8个CJK字符后跟冒号（如"合同编号："）

### 7. 跨平台兼容性

**行尾处理**：输出使用平台对应的行尾符（Windows用CRLF，Linux/macOS用LF）。`NormalizeLineEndings` 和 `CollapseBlankLines` 均自动适配。

**路径处理**：`PathCombine` 在Windows上使用 `\`，Linux/macOS上使用 `/`。`GetAbsolutePath` 在Windows上使用 `GetFullPathNameW`（文件不存在也可用），Linux/macOS上使用 `getcwd` 拼接绝对路径。

**C++标准**：CMake 显式设置 `CMAKE_CXX_STANDARD=14` 并启用 `CMAKE_CXX_STANDARD_REQUIRED=ON`，确保不同编译器下编译一致。

**UTF-8处理**：所有CJK字符检测使用手动UTF-8解码（`DecodeUTF8Char` + `IsCJKCodepoint`），而非正则表达式的Unicode范围，避免 `std::regex` 在MinGW/libstdc++/libc++/MSVC STL间的兼容性问题。

**边界安全**：`DecodeUTF8Char` 包含边界检查（`pos + charLen > len`），防止缓冲区越界读取。

## 编译

### Windows（MinGW，Win7兼容）

```bash
# 前置：MinGW-w64 GCC, CMake, UPX（可选）
build_windows.bat
```

或手动编译：
```bash
mkdir build_mingw && cd build_mingw
cmake -G "MinGW Makefiles" ^
      -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_C_FLAGS="-O2 -DNDEBUG -D_WIN32_WINNT=0x0601" ^
      -DCMAKE_CXX_FLAGS="-O2 -DNDEBUG -D_WIN32_WINNT=0x0601 -static-libgcc -static-libstdc++" ^
      -DCMAKE_EXE_LINKER_FLAGS="-s -static" ^
      ..
mingw32-make -j%NUMBER_OF_PROCESSORS%
strip TextExtractionCLI\TextExtraction.exe
upx --best TextExtractionCLI\TextExtraction.exe
```

### Linux

```bash
# 方式1：直接编译
chmod +x build_linux.sh
./build_linux.sh

# 方式2：Docker编译
docker build -t pdf-text-extraction .
docker create --name extract pdf-text-extraction
docker cp extract:/TextExtraction-linux-x64 ./
docker rm extract
```

### macOS

```bash
chmod +x build_macos.sh
./build_macos.sh
```

### 编译参数说明

| 参数 | 说明 |
|------|------|
| `-D_WIN32_WINNT=0x0601` | Windows目标版本为Win7（0x0601） |
| `-static-libgcc -static-libstdc++` | 静态链接C/C++运行时 |
| `-s` | 链接时去除调试符号 |
| `-static` | 全静态链接（Windows） |
| `-O2` | 优化等级 |
| `-DNDEBUG` | 禁用assert等调试断言 |

## 作为库使用

```cpp
#include "TextExtraction.h"

TextExtraction extraction;
extraction.ExtractText("input.pdf");  // 全部页面

// 指定页码集合（0-based）
std::set<long> pages = {0, 2, 3, 4};  // 第1、3、4、5页
extraction.ExtractText("input.pdf", pages);

// 获取全部文本
std::ostringstream stream;
extraction.GetResultsAsText(-1, TextComposer::eSpacingBoth, stream, true);
std::string text = stream.str();

// 按页获取文本（用于结构化输出）
for(size_t i = 0; i < extraction.GetPageCount(); ++i) {
    long pageNum = extraction.GetOriginalPageNumber(i) + 1;  // 1-based
    std::ostringstream pageStream;
    extraction.GetPageAsText(i, -1, TextComposer::eSpacingBoth, pageStream, true);
    // pageNum: 页码, pageStream.str(): 该页文本
}
```

## 许可证

Apache License 2.0（与原项目一致）
