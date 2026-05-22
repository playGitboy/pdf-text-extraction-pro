A CLI (command line interface) to Extract text from PDF files.
Use from your terminal to dump a PDF file text to the std output.
Options exists to output to file, choose pages range etc.

This is an **enhanced version** based on [pdf-text-extraction](https://github.com/galkahana/pdf-text-extraction), with critical bug fixes for Chinese/CJK users and new features like duplicate character filtering and smart line merging.

## What's New vs. Original

| Feature | Original | Enhanced |
|---------|----------|----------|
| Chinese path/filename | ❌ `Cannot read file` error | ✅ Full Unicode support |
| Page selection | ❌ Only `-s`/`-e` start/end | ✅ `-p` supports ranges & mixed: `1,3-5,10-12` |
| Structured output | ❌ Plain text only | ✅ `-o` auto-detects HTML/JSON by extension |
| Multi-file processing | ❌ Single file only | ✅ Multiple files/directories, recursive scan |
| Windows terminal CJK | ❌ Garbled output | ✅ UTF-8 output |
| CJK duplicate chars | ❌ e.g. "第第十十二二条条" | ✅ `-f` filter flag |
| Smart line merging | ❌ Each line separate | ✅ `-m` merges paragraph lines |
| Cross-platform | ❌ Windows-centric | ✅ Windows/Linux/macOS with proper line endings & path handling |
| Static linking | ❌ Runtime dependencies | ✅ Fully static, zero dependencies |
| Size optimization | ❌ Debug info included | ✅ strip + UPX compression |

## Usage

```bash
TextExtraction <path1> [path2] ... [option(s)]
```

Paths can be PDF files or directories. Directories are scanned recursively for PDF files. Paths and options can be mixed.

### Options

| Flag | Description |
|------|-------------|
| `-p, --pages <spec>` | Page numbers to extract (1-based). Formats: `1,5,58` or `2-24` or `1,3-5,10-12`. Default: all pages |
| `-f, --filter-dup` | Filter duplicate text placements (fixes doubled characters in some CJK PDFs) |
| `-m, --merge-lines` | Merge broken lines into paragraphs (smart line joining for PDF text output) |
| `--spacing <BOTH\|HOR\|VER\|NONE>` | Add spaces between text pieces. Default is BOTH |
| `-t, --tables` | Extract tables instead of text (CSV output) |
| `-o, --output <path>` | Write result to file (format by extension: .html/.htm=HTML, .json=JSON, else plain text) |
| `-q, --quiet` | Quiet run, only errors and warnings |
| `-d, --debug <path>` | Create debug output file |
| `-h, --help` | Show help message |

### Page Spec Format (`-p`)

| Format | Example | Description |
|--------|---------|-------------|
| Single pages | `-p 1,5,58` | Comma-separated page numbers |
| Range | `-p 2-24` | Continuous page range |
| Mixed | `-p 1,3-5,10-12` | Combine single pages and ranges |
| All pages | (omit `-p`) | Default: extract all pages |

### Output Format (`-o` extension)

| Extension | Format | Description |
|-----------|--------|-------------|
| `.html` / `.htm` | HTML table | 3-column table: absolute file path (20%), page (5%), text (75%) |
| `.json` | JSON array | `[{"file":"abs_path", "pages":[{"page":N, "text":"..."}]}]` |
| Other | Plain text | Default format, same as original behavior |

### Common Examples

```bash
# Extract all text from a PDF
TextExtraction input.pdf

# Extract page 1 only
TextExtraction input.pdf -p 1

# Extract pages 3-5 with duplicate character filtering
TextExtraction input.pdf -p 3-5 -f

# Extract specific pages
TextExtraction input.pdf -p 1,3,5

# Mixed: page 1, pages 3-5, pages 10-12, with filtering and merging
TextExtraction input.pdf -p 1,3-5,10-12 -f -m

# Enable paragraph line merging
TextExtraction input.pdf -m

# Both duplicate filtering and line merging, save to file
TextExtraction input.pdf -f -m -o output.txt

# Output as HTML table (per-page columns)
TextExtraction input.pdf -p 1,3-5 -f -o result.html

# Output as JSON (structured data for programmatic use)
TextExtraction input.pdf -f -m -o result.json

# Process multiple PDF files
TextExtraction file1.pdf file2.pdf file3.pdf -f -o result.json

# Recursively scan directory for PDFs
TextExtraction /path/to/pdfs/ -f -o result.html

# Mix files and directories
TextExtraction file1.pdf /path/to/more/ -p 1-5 -f -m -o result.json

# Extract tables
TextExtraction input.pdf -t -o tables.csv

# Handle Chinese filename
TextExtraction "合同.pdf" -f -m -o 合同文本.html
```

## Bug Fixes & Enhancements Detail

### 1. Chinese Path/Filename Support

**Problem**: Files with Chinese characters in path/name cause `Cannot read file` error.

**Cause**: Windows `main(int argc, char* argv[])` uses system default encoding, non-UTF-8 locales truncate Chinese.

**Fix**: Use `CommandLineToArgvW()` to get wide-char arguments, convert to UTF-8.

### 2. Flexible Page Selection

**Problem**: Original only supports `-s` start page and `-e` end page, cannot specify non-contiguous pages.

**Fix**: Merged `-s`/`-e` into `-p`/`--pages` supporting multiple formats. Pages are 1-based, auto-converted to internal 0-based index.

### 3. Multi-file/Directory Processing

**Problem**: Original only handles a single file.

**Fix**: Support multiple files, directories, and mixed input. Directories are recursively scanned. Errors in one file don't stop processing others. HTML/JSON output merges all files; plain text separates with `---`.

### 4. Windows Terminal CJK Display

**Problem**: Extracted Chinese text appears garbled in terminal.

**Fix**: Set `SetConsoleOutputCP(CP_UTF8)` and `_setmode(_fileno(stdout), _O_BINARY)` at startup.

### 5. CJK Duplicate Character Filtering

**Problem**: Some PDFs produce doubled characters like "第第十十二二条条 逾逾期期交交付付责责任任".

**Cause**: PDF content stream draws the same text twice (shadow/stroke effect or overlapping text layers). Duplicate placements have similar positions (center distance < 1.5× font height).

**Fix**: New center-distance dedup algorithm, enabled via `-f` flag.

### 6. Smart Line Merging

**Problem**: PDF text extraction outputs each line independently, splitting paragraphs incorrectly.

**Fix**: New `-m` flag enables smart paragraph merging. The algorithm identifies document structure elements (clause numbers, headings, list items, field labels) and keeps them separate, while merging lines that belong to the same paragraph.

**Structure Detection** (no regex for CJK, manual UTF-8 parsing for cross-platform compatibility):
- Chinese chapter numbers: 第X条/章/节
- Arabic/Chinese number lists: 1. / 一、
- Multi-level numbering: 1.1 / 1.1.1
- Parenthesized numbers: （一）/ (1)
- Circled numbers: ①②③
- Bullet/dash lists: • / — / -
- Field labels: 1-8 CJK chars followed by colon (e.g. "合同编号：")

### 7. Cross-Platform Compatibility

**Line Endings**: Output uses platform-appropriate line endings (CRLF on Windows, LF on Linux/macOS). Both `NormalizeLineEndings` and `CollapseBlankLines` adapt automatically.

**Path Handling**: `PathCombine` uses `\` on Windows and `/` on Linux/macOS. `GetAbsolutePath` uses `GetFullPathNameW` on Windows (works even if file doesn't exist) and `getcwd` on Linux/macOS.

**C++ Standard**: CMake explicitly sets `CMAKE_CXX_STANDARD=14` with `CMAKE_CXX_STANDARD_REQUIRED=ON`, ensuring consistent compilation across compilers.

**UTF-8 Processing**: All CJK character detection uses manual UTF-8 decoding (`DecodeUTF8Char` + `IsCJKCodepoint`) instead of regex with Unicode ranges, avoiding `std::regex` incompatibilities across MinGW/libstdc++/libc++/MSVC STL.

**Boundary Safety**: `DecodeUTF8Char` includes bounds checking (`pos + charLen > len`) to prevent buffer overreads.

## Building

### Windows (MinGW, Win7 compatible)

```bash
# Prerequisites: MinGW-w64 GCC, CMake, UPX (optional)
build_windows.bat
```

Or manually:
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
# Option 1: Direct build
chmod +x build_linux.sh
./build_linux.sh

# Option 2: Docker build
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

### Build Parameters

| Parameter | Description |
|-----------|-------------|
| `-D_WIN32_WINNT=0x0601` | Target Windows 7 (0x0601) |
| `-static-libgcc -static-libstdc++` | Static link C/C++ runtime |
| `-s` | Strip debug symbols at link time |
| `-static` | Full static linking (Windows) |
| `-O2` | Optimization level |
| `-DNDEBUG` | Disable assert and debug assertions |

## Using as a Library

```cpp
#include "TextExtraction.h"

TextExtraction extraction;
extraction.ExtractText("input.pdf");  // all pages

// Specify page set (0-based)
std::set<long> pages = {0, 2, 3, 4};  // pages 1, 3, 4, 5
extraction.ExtractText("input.pdf", pages);

// Get all text
std::ostringstream stream;
extraction.GetResultsAsText(-1, TextComposer::eSpacingBoth, stream, true);
std::string text = stream.str();

// Get text per page (for structured output)
for(size_t i = 0; i < extraction.GetPageCount(); ++i) {
    long pageNum = extraction.GetOriginalPageNumber(i) + 1;  // 1-based
    std::ostringstream pageStream;
    extraction.GetPageAsText(i, -1, TextComposer::eSpacingBoth, pageStream, true);
    // pageNum: page number, pageStream.str(): page text
}
```

## Bidirectional Text Support

PDF files contain text as drawing instructions, so parsed text is in visual order. For right-to-left text or mixed RTL/LTR content, the parsed text may appear reversed or disorganized.

BIDI conversion uses ICU library and is off by default. Enable with `-DUSE_BIDI=1` during cmake configuration:

```bash
mkdir build && cd build
cmake .. -DUSE_BIDI=1
```

ICU installation tries: (1) Win10 SDK native ICU on Windows, (2) pre-installed package (e.g. `brew install icu4c` on Mac), (3) download and build ICU72 from source.

## Solution Architecture

This implementation is based on hummus PDF library. It uses the parsing capabilities to interpret page content and understand lines and texts.

- `PDFRecursiveInterpreter` — basic PDF content interpretation, recurses into forms
- `GraphicContentInterpreter` — understands path and text operators
- `TextInterpreter` — converts text placements to actual text using font data
- `LineMerger` — smart paragraph merging for PDF text output (new)
- `TableComposer` — builds tables from lines and text
- `TableCSVExport` — exports Table objects to CSV

## License

Apache License 2.0 (same as original project)
