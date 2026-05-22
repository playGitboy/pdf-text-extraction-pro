# PDF Text Extraction — AI 开发上下文

> 本文档为 AI 助手提供项目全貌，使其能以最短时间理解项目并接手开发。

---

## 一、项目概述

基于 [pdf-text-extraction](https://github.com/galkahana/pdf-text-extraction)（上游作者 Gal Kahana）的 **增强版**，修复了多个影响中文/CJK用户的关键Bug，并新增叠词过滤和段落行智能合并功能。项目为 C++ CLI 程序，从 PDF 文件提取纯文本或表格（CSV）。

- **版本**：1.1.9
- **许可证**：Apache License 2.0
- **语言**：C++17（实际使用C++11特性即可）
- **构建系统**：CMake ≥ 3.15
- **核心依赖**：PDFHummus/PDF-Writer（通过 FetchContent 自动下载）

---

## 二、项目结构

```
pdf-text-extraction/
├── CMakeLists.txt                  # 根CMake：FetchContent下载pdfhummus，添加子目录
├── Config.cmake.in                 # CMake包配置模板
├── Dockerfile                      # Linux Docker编译
├── build_windows.bat               # Windows MinGW一键编译脚本
├── build_linux.sh                  # Linux一键编译脚本
├── build_macos.sh                  # macOS一键编译脚本
├── PDF_LINE_MERGE_GUIDE.md         # 行合并功能规则定义（设计文档+测试用例）
├── README.md                       # 英文说明
├── README_CN.md                    # 中文说明（含Bug修复详情、编译指南）
│
├── TextExtraction/                 # 核心库
│   ├── CMakeLists.txt              # 库编译配置
│   ├── TextExtraction.h/.cpp       # 文本提取主类（入口）
│   ├── TableExtraction.h/.cpp      # 表格提取主类
│   ├── ErrorsAndWarnings.h         # 错误/警告定义
│   └── lib/
│       ├── text-parsing/           # PDF文本解析层
│       │   ├── ParsedTextPlacement.h   # 核心数据结构：文本放置信息
│       │   ├── TextInterpreter.h/.cpp  # 文本解释器（字体解码→文本放置）
│       │   └── ITextInterpreterHandler.h
│       ├── text-composition/       # 文本合成层（★重点修改区域）
│       │   ├── TextComposer.h/.cpp     # 文本合成器（排序→组行→输出，含叠词过滤）
│       │   └── LineMerger.h/.cpp       # 行合并器（★新增，段落行智能合并）
│       ├── font-translation/       # 字体编码翻译
│       ├── graphic-content-parsing/# PDF图形内容解析
│       ├── interpreter/            # PDF解释器
│       ├── math/                   # 坐标变换
│       ├── bidi/                   # 双向文本（BiDi）支持（需ICU）
│       ├── table-*/               # 表格相关（解析/合成/CSV导出）
│       ├── graphs/                 # 图算法
│       └── pdf-writer-enhancers/   # PDF-Writer辅助
│
├── TextExtractionCLI/              # CLI可执行程序
│   ├── CMakeLists.txt
│   └── extract-text-cli.cpp        # ★CLI入口（参数解析、输出逻辑）
│
├── TextExtractionTesting/          # 测试（ctest）
│   └── Materials/                  # 测试用PDF文件
│
└── build_mingw/                    # MinGW构建目录（本地，不入版本控制）
```

---

## 三、核心数据流

```
PDF文件
  │
  ▼
PDFParser (PDFHummus库)
  │
  ▼
GraphicContentInterpreter ──→ TextInterpreter ──→ ParsedTextPlacement
  │                              │                    │
  │                         字体解码(FontDecoder)      │  text, matrix[6],
  │                         编码翻译(Encoding)         │  localBbox[4], globalBbox[4],
  │                                                    │  spaceWidth, globalSpaceWidth[2]
  ▼                                                    ▼
TextExtraction::textsForPages (ParsedTextPlacementListList)
  │
  ▼
TextComposer::ComposeText()
  │  ├─ 排序（按位置：从上到下，从左到右）
  │  ├─ 叠词过滤（-f 启用）：IsDuplicateTextPlacement()
  │  ├─ 组行（AreSameLine判断同行）
  │  └─ 输出（含空格/换行）
  │
  ▼
LineMerger::MergeLines()（-m 启用）
  │  ├─ 预处理：统一行尾、修复连字符断词
  │  ├─ 逐行判断合并：结构识别→标点判断→空格决策
  │  └─ 后处理：清理标点前空格、合并多余空行
  │
  ▼
最终文本输出（stdout 或 文件）
```

---

## 四、关键类与接口

### 4.1 ParsedTextPlacement（核心数据结构）

```cpp
struct ParsedTextPlacement {
    std::string text;           // 文本内容
    double matrix[6];           // 变换矩阵
    double localBbox[4];        // 局部边界框 [left,bottom,right,top]
    double globalBbox[4];       // 全局边界框 [left,bottom,right,top]
    double spaceWidth;          // 空格宽度
    double globalSpaceWidth[2]; // 全局空格宽度向量
};
```

### 4.2 TextComposer（文本合成器）

```cpp
class TextComposer {
    TextComposer(int bidiFlag, ESpacing spacingFlag, bool filterDuplicates = false);
    void ComposeText(const ParsedTextPlacementList& inTextPlacements, std::ostream& outStream);
};
// filterDuplicates: 启用叠词过滤
// 叠词过滤算法：IsDuplicateTextPlacement() — 中心距离 < maxH*0.3 或 IoU > 0.3
```

### 4.3 LineMerger（行合并器，★新增）

```cpp
class LineMerger {
    std::string MergeLines(const std::string& input);  // 主入口
    // 内部：Preprocess → SplitLines → 多列拆分 → 逐行合并循环 → Postprocess
    // 结构识别：IsStructureStart() — 12种正则模式（含reFieldLabelCN字段标签）
    // 标点判断：IsStrongTerminatorStr / IsClauseTerminatorStr
    // 空格决策：ShouldAddSpaceBetween()（数字续行不加空格）
    // 多列拆分：SplitMultiColumnLine() — 行内≥4连续空格拆为独立行
    // 连续换行：Postprocess合并所有连续换行为单个换行
};
```

### 4.4 TextExtraction（提取主类）

```cpp
class TextExtraction {
    // 原接口（向后兼容）
    EStatusCode ExtractText(const std::string& filePath, long startPage=0, long endPage=-1);
    // 新接口（页码集合，0-based）
    EStatusCode ExtractText(const std::string& filePath, const std::set<long>& pages);
    void GetResultsAsText(int bidiFlag, ESpacing spacing, std::ostream& outStream, bool filterDuplicates=false);
    void GetPageAsText(size_t pageIndex, int bidiFlag, ESpacing spacing, std::ostream& outStream, bool filterDuplicates=false);
    size_t GetPageCount() const;
    long GetOriginalPageNumber(size_t pageIndex) const;  // 返回0-based内部页码
    ParsedTextPlacementListList textsForPages;  // 每页的文本放置列表
    std::vector<long> extractedPageNumbers;  // 每页对应的原始页码（0-based）
};
```

---

## 五、CLI 参数

| 参数 | 说明 | 实现位置 |
|------|------|---------|
| `<path1> [path2] ...` | 输入路径（PDF文件或目录，目录递归扫描），路径和选项可混写 | extract-text-cli.cpp (CollectPDFFiles, ScanDirectoryRecursive) |
| `-p, --pages <spec>` | 页码指定（1-based），支持 `1,5,58` / `2-24` / `1,3-5,10-12`，默认全部 | extract-text-cli.cpp (ParsePageSpec) |
| `-f, --filter-dup` | 过滤叠词（CJK重复字符） | TextComposer.cpp |
| `-m, --merge-lines` | 段落行智能合并 | LineMerger.cpp |
| `--spacing` | 空格策略 BOTH/HOR/VER/NONE | TextComposer.cpp |
| `-t, --tables` | 提取表格为CSV | TableExtraction.cpp |
| `-o, --output` | 输出到文件（扩展名决定格式：.html/.htm=HTML表格, .json=JSON, 其他=纯文本） | extract-text-cli.cpp (DetectOutputFormat) |
| `-q, --quiet` | 静默模式 | extract-text-cli.cpp |
| `-b, --bidi` | BiDi方向（需ICU） | BidiConversion.cpp |
| `-d, --debug` | 调试输出 | extract-text-cli.cpp |

**页码格式说明**（`-p` 参数）：
- 单独页：`-p 1,5,58` — 逗号分隔多个页码
- 页码范围：`-p 2-24` — 连续页码范围
- 混合表达：`-p 1,3-5,10-12` — 逗号分隔，支持单页和范围混写
- 不指定 `-p` 默认提取全部页面
- 内部实现：`ParsePageSpec()` 解析为 `std::set<long>`（0-based），自动去重排序

**输出格式说明**（`-o` 参数）：
- `.html` / `.htm`：HTML表格，三列（文件绝对路径、页码、文本），含CSS样式
- `.json`：JSON数组，`[{"file": "绝对路径", "pages": [{"page": 1, "text": "..."}]}, ...]`
- 其他/无扩展名：纯文本（默认，多文件用 `--- 绝对路径 ---` 分隔）
- 内部实现：`DetectOutputFormat()` 检测扩展名，`GetAbsolutePath()` 转绝对路径，流式写入（不缓存所有结果）
- 连续换行：所有输出格式均合并连续空行为单个换行（`CollapseBlankLines` / LineMerger Postprocess）

**多文件处理说明**：
- 输入路径可以是PDF文件或目录，目录递归扫描所有 `.pdf` 文件
- 路径和选项可混写（如 `file1.pdf -p 1 dir/ -o out.json`）
- `CollectPDFFiles()` 收集所有输入路径，`ScanDirectoryRecursive()` 递归扫描目录
- `IsDirectory()` 判断路径类型（Windows: `GetFileAttributesW`，Linux: `stat`）
- 错误文件自动跳过，不影响其他文件处理
- 多文件时 stderr 显示进度 `[1/N] Processing: ...`
- 表格模式：多文件时输出CSV文件名附加输入文件名前缀

**注意**：路径和选项可混写，所有非选项参数（不以`-`开头且非选项值）均视为输入路径。如 `TextExtraction file1.pdf dir/ -f -m file2.pdf -o out.json`。

---

## 六、编译指南

### 6.1 依赖

- CMake ≥ 3.15
- GCC/MinGW（C++11兼容）
- PDFHummus/PDF-Writer（通过 FetchContent 自动下载，无需手动安装）
- OpenSSL（可选，PDF 2.0加密支持，FetchContent会自动检测）
- ICU（可选，BiDi支持，需 `-DUSE_BIDI=ON`）

### 6.2 Windows（MinGW，Win7兼容）

```powershell
# 设置PATH
$env:Path = "CMake路径;MinGW路径;" + $env:Path

# 方式1：一键编译
.\build_windows.bat

# 方式2：手动编译
mkdir build_mingw && cd build_mingw
cmake -G "MinGW Makefiles" `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_C_FLAGS="-O2 -DNDEBUG -D_WIN32_WINNT=0x0601" `
    -DCMAKE_CXX_FLAGS="-O2 -DNDEBUG -D_WIN32_WINNT=0x0601 -static-libgcc -static-libstdc++" `
    -DCMAKE_EXE_LINKER_FLAGS="-s -static" `
    ..
mingw32-make -j8
```

### 6.3 Linux

```bash
./build_linux.sh
# 或 Docker: docker build -t pdf-text-extraction .
```

### 6.4 macOS

```bash
./build_macos.sh
```

### 6.5 关键编译参数

| 参数 | 说明 |
|------|------|
| `-D_WIN32_WINNT=0x0601` | 目标Windows 7 |
| `-static-libgcc -static-libstdc++` | 静态链接C++运行时 |
| `-s -static` | 去除调试符号+全静态链接 |
| `-DUSE_BIDI=ON` | 启用BiDi支持（需ICU） |
| `-DSHOULD_PARSE_INTERNAL_TABLES=ON` | 启用内部表格解析 |

### 6.6 CMakeLists.txt 依赖管理

根 CMakeLists.txt 使用 FetchContent 管理依赖：
- 优先检查本地 `build/_deps/pdfhummus-src/CMakeLists.txt`
- 不存在则自动从 `https://github.com/galkahana/PDF-Writer.git` 下载
- 仓库名已从 `pdfhummus` 更名为 `PDF-Writer`，分支从 `master` 改为 `main`

---

## 七、已修复的Bug

### 7.1 中文路径/文件名支持

- **问题**：`Cannot read file 测试.pdf`
- **原因**：Windows下 `main(int argc, char* argv[])` 使用系统默认编码
- **修复**：使用 `CommandLineToArgvW()` + `WideCharToMultiByte(CP_UTF8)` 获取UTF-8参数

### 7.2 灵活页码指定

- **问题**：原版仅支持 `-s` 起始页和 `-e` 结束页，无法指定不连续页面
- **修复**：合并 `-s`/`-e` 为 `-p`/`--pages`，支持 `1,5,58` / `2-24` / `1,3-5,10-12` 格式
- **API**：新增 `ExtractText(filePath, set<long>)` 和 `ExtractTables(filePath, set<long>)` 重载
- **原 `-p`/`--spacing`** 改为 `--spacing`（仅长选项），释放 `-p` 短选项给页码功能

### 7.2.1 结构化输出

- **问题**：原版仅支持纯文本输出，无法按页码分列/结构化展示
- **修复**：`-o` 参数根据文件扩展名自动选择输出格式
  - `.html` / `.htm`：HTML表格（文件路径、页码、文本三列）
  - `.json`：JSON格式（`[{"file": "...", "pages": [{"page": 1, "text": "..."}]}]`）
  - 其他/无扩展名：纯文本（默认）
- **API**：新增 `GetPageAsText()` / `GetPageCount()` / `GetOriginalPageNumber()` 按页提取
- **实现**：`DetectOutputFormat()` 检测扩展名，流式写入（不缓存），`EscapeHTML()` / `EscapeJSON()` 转义

### 7.2.2 多文件/目录处理

- **问题**：原版仅支持单文件处理，无法批量处理
- **修复**：支持多文件、目录、混写输入
  - 多文件：`TextExtraction file1.pdf file2.pdf -o result.json`
  - 目录：递归扫描所有 `.pdf` 文件（`ScanDirectoryRecursive`）
  - 混写：文件和目录可混合指定，路径和选项可混写
  - 错误文件自动跳过，不影响其他文件
- **跨平台**：Windows用 `FindFirstFileW`/`FindNextFileW`，Linux用 `opendir`/`readdir`
- **输出**：HTML/JSON合并所有文件结果，纯文本用 `--- 绝对路径 ---` 分隔
- **表格**：多文件时输出CSV文件名附加输入文件名前缀

### 7.3 Windows终端UTF-8输出

- **修复**：`SetConsoleOutputCP(CP_UTF8)` + `_setmode(_fileno(stdout), _O_BINARY)`

### 7.4 CJK叠词过滤

- **问题**：如"第第十十二二条条"
- **算法**：`IsDuplicateTextPlacement()` — 相同文本 + 中心距离 < maxH×0.3 或 IoU > 0.3
- **关键修复**：CJK字符 bounding box 宽度可能为0，需优先用中心距离判断而非 IoU

### 7.5 段落行智能合并

- **问题**：每行独立输出，同段被错误分割
- **算法**：`LineMerger::MergeLines()` — 多列拆分 + 结构识别 + 标点判断 + 空格决策
- **多列拆分**：`SplitMultiColumnLine()` — 行内≥4连续空格拆为独立行
- **数字续行**：`ShouldAddSpaceBetween()` — 数字续行不加空格
- **字段标签**：`reFieldLabelCN` — 识别"房屋编号："等字段标签独立成行
- **连续换行**：Postprocess合并所有连续换行为单个换行
- **规则文件**：`PDF_LINE_MERGE_GUIDE.md`

---

## 八、代码约定

1. **无注释**：代码中不添加注释，除非用户明确要求
2. **命名**：类名 PascalCase，方法 PascalCase，变量 camelCase，常量 sc/k 前缀
3. **UTF-8**：所有字符串使用 UTF-8 编码，中文标点用 `\uXXXX` 转义
4. **正则表达式**：在构造函数中预编译，使用 `try/catch` 包裹 `regex_search`
5. **Postprocess**：避免复杂正则，使用字符串替换遍历处理
6. **Windows兼容**：输出文件写 BOM（`0xEF,0xBB,0xBF`），二进制模式写文件

---

## 九、潜在改进方向

1. **LineMerger 优化**：扩展结构模式（更多法律/技术文档关键词）、测试嵌套列表场景
2. **叠词过滤优化**：当前仅比较相邻文本放置，可考虑全局去重
3. **跨平台测试**：Linux/macOS 版本编译验证
4. **BiDi 支持**：ICU 集成测试
5. **性能**：大文件流式处理、正则预编译优化

---

## 十、快速上手检查清单

- [ ] 阅读 `PDF_LINE_MERGE_GUIDE.md` 理解行合并规则
- [ ] 阅读 `README_CN.md` 了解Bug修复详情
- [ ] 理解 `ParsedTextPlacement` 数据结构（globalBbox[4] = [left,bottom,right,top]）
- [ ] 理解数据流：PDFParser → TextInterpreter → TextComposer → LineMerger → 输出
- [ ] 修改文本后处理逻辑 → `TextComposer.cpp` 和 `LineMerger.cpp`
- [ ] 修改CLI参数 → `extract-text-cli.cpp`
- [ ] 修改PDF解析逻辑 → `TextInterpreter.cpp` / `FontDecoder.cpp`
- [ ] 编译：`build_windows.bat` 或手动 cmake + make
- [ ] 测试：`TextExtraction 测试.pdf -f -m -o output.txt`
