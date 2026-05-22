#include <iostream>
#include <string>
#include <fstream>
#include <locale>
#include <set>
#include <sstream>
#include <algorithm>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include <unistd.h>
#endif

#include "EStatusCode.h"
#include "BoxingBase.h"

#include "TextExtraction.h"
#include "TableExtraction.h"
#include "lib/text-composition/TextComposer.h"
#include "lib/text-composition/LineMerger.h"

using namespace std;
using namespace PDFHummus;

#ifdef _WIN32
static std::string WideStringToUTF8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string result(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &result[0], size_needed, NULL, NULL);
    return result;
}

static std::vector<std::string> GetUTF8CommandLineArgs() {
    std::vector<std::string> args;
    int argc;
    LPWSTR* argvW = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argvW) {
        for (int i = 0; i < argc; ++i) {
            args.push_back(WideStringToUTF8(argvW[i]));
        }
        LocalFree(argvW);
    }
    return args;
}
#endif

static bool EndsWithPDF(const string& path) {
    if(path.size() < 4) return false;
    string ext = path.substr(path.size() - 4);
    for(size_t i = 0; i < ext.size(); ++i)
        ext[i] = (char)tolower((unsigned char)ext[i]);
    return ext == ".pdf";
}

static string PathCombine(const string& base, const string& name) {
    if(base.empty()) return name;
#ifdef _WIN32
    char sep = (base[base.size()-1] == '\\' || base[base.size()-1] == '/') ? '\0' : '\\';
#else
    char sep = (base[base.size()-1] == '/') ? '\0' : '/';
#endif
    if(sep == '\0') return base + name;
    return base + sep + name;
}

#ifdef _WIN32
static std::wstring UTF8ToWideString(const std::string& str) {
    if(str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring result(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &result[0], size_needed);
    return result;
}

static void ScanDirectoryRecursive(const string& dirPath, vector<string>& pdfFiles) {
    string searchPattern = PathCombine(dirPath, "*");
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(UTF8ToWideString(searchPattern).c_str(), &findData);
    if(hFind == INVALID_HANDLE_VALUE) {
        cerr << "Warning: Cannot open directory: " << dirPath << endl;
        return;
    }

    do {
        wstring wName(findData.cFileName);
        string name = WideStringToUTF8(wName);
        if(name == "." || name == "..") continue;

        string fullPath = PathCombine(dirPath, name);

        if(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            ScanDirectoryRecursive(fullPath, pdfFiles);
        } else if(EndsWithPDF(name)) {
            pdfFiles.push_back(fullPath);
        }
    } while(FindNextFileW(hFind, &findData));

    FindClose(hFind);
}

static bool IsDirectory(const string& path) {
    DWORD attrs = GetFileAttributesW(UTF8ToWideString(path).c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES) && (attrs & FILE_ATTRIBUTE_DIRECTORY);
}
#else
static void ScanDirectoryRecursive(const string& dirPath, vector<string>& pdfFiles) {
    DIR* dir = opendir(dirPath.c_str());
    if(!dir) {
        cerr << "Warning: Cannot open directory: " << dirPath << endl;
        return;
    }

    struct dirent* entry;
    while((entry = readdir(dir)) != NULL) {
        string name = entry->d_name;
        if(name == "." || name == "..") continue;

        string fullPath = PathCombine(dirPath, name);
        struct stat st;
        if(stat(fullPath.c_str(), &st) == 0) {
            if(S_ISDIR(st.st_mode)) {
                ScanDirectoryRecursive(fullPath, pdfFiles);
            } else if(S_ISREG(st.st_mode) && EndsWithPDF(name)) {
                pdfFiles.push_back(fullPath);
            }
        }
    }
    closedir(dir);
}

static bool IsDirectory(const string& path) {
    struct stat st;
    if(stat(path.c_str(), &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}
#endif

static void CollectPDFFiles(const vector<string>& inputPaths, vector<string>& pdfFiles) {
    for(size_t i = 0; i < inputPaths.size(); ++i) {
        const string& p = inputPaths[i];
        if(IsDirectory(p)) {
            ScanDirectoryRecursive(p, pdfFiles);
        } else {
            pdfFiles.push_back(p);
        }
    }
}

static bool ParsePageSpec(const string& spec, set<long>& pages) {
    pages.clear();
    stringstream ss(spec);
    string segment;

    while (getline(ss, segment, ',')) {
        size_t dashPos = segment.find('-');
        if (dashPos == string::npos) {
            char* endPtr = NULL;
            long pageNum = strtol(segment.c_str(), &endPtr, 10);
            if (endPtr == segment.c_str() || *endPtr != '\0') {
                cerr << "Invalid page number: " << segment << endl;
                return false;
            }
            if (pageNum == 0) {
                cerr << "Page number must be non-zero: " << segment << endl;
                return false;
            }
            long internal = (pageNum > 0) ? (pageNum - 1) : pageNum;
            pages.insert(internal);
        } else {
            string leftStr = segment.substr(0, dashPos);
            string rightStr = segment.substr(dashPos + 1);

            char* endPtr = NULL;
            long left = strtol(leftStr.c_str(), &endPtr, 10);
            if (endPtr == leftStr.c_str() || *endPtr != '\0' || left <= 0) {
                cerr << "Invalid page range start: " << leftStr << endl;
                return false;
            }
            long right = strtol(rightStr.c_str(), &endPtr, 10);
            if (endPtr == rightStr.c_str() || *endPtr != '\0' || right <= 0) {
                cerr << "Invalid page range end: " << rightStr << endl;
                return false;
            }
            if (left > right) {
                cerr << "Page range start > end: " << segment << endl;
                return false;
            }
            for (long p = left; p <= right; ++p) {
                pages.insert(p - 1);
            }
        }
    }
    return !pages.empty();
}

static void ShowUsage(const string& name)
{
    cerr << "Usage: " << name << " <path1> [path2] ... [option(s)]\n"
              << "paths - PDF file(s) or directory(s). Directories are scanned recursively for PDF files.\n"
              << "Options:\n"
              << "\t-p, --pages <spec>\t\t\tpage numbers to extract (1-based). formats: 1,5,58 or 2-24 or 1,3-5,10-12. default: all pages\n"
#if (SUPPORT_ICU_BIDI==1)
              << "\t-b, --bidi <RTL|LTR>\t\t\tuse bidi algo to convert visual to logical. provide default direction per document writing direction.\n"
#endif
              << "\t    --spacing <BOTH|HOR|VER|NONE>\tadd spaces between pieces of text considering their relative positions. default is BOTH\n"
              << "\t-t, --tables\t\t\t\textract tables instead of text. Each table is represented in CSV\n"
              << "\t-o, --output /path/to/file\t\twrite result to output file (format by extension: .html/.htm=HTML table, .json=JSON, else plain text)\n"
              << "\t-q, --quiet\t\t\t\tquiet run. only shows errors and warnings\n"
              << "\t-f, --filter-dup\t\t\tfilter duplicate text placements (fixes doubled characters in some CJK PDFs)\n"
              << "\t-m, --merge-lines\t\t\tmerge broken lines into paragraphs (smart line joining for PDF text output)\n"
              << "\t-h, --help\t\t\t\tShow this help message\n"
              << "\t-d, --debug /path/to/file\t\tcreate debug output file\n"
              << endl;
}

static const string BIDI_LTR = "LTR";
static const string BIDI_RTL = "RTL";
static const string SPACING_BOTH = "BOTH";
static const string SPACING_HOR = "HOR";
static const string SPACING_VER = "VER";
static const string SPACING_NONE = "NONE";

static const string scCSVExtension = ".csv";
static const string scDot = ".";

enum OutputFormat {
    eOutputText,
    eOutputHTML,
    eOutputJSON
};

static OutputFormat DetectOutputFormat(const string& filePath) {
    size_t dotPos = filePath.find_last_of('.');
    if(dotPos == string::npos)
        return eOutputText;

    string ext = filePath.substr(dotPos);
    string extLower = ext;
    for(size_t i = 0; i < extLower.size(); ++i)
        extLower[i] = (char)tolower((unsigned char)extLower[i]);

    if(extLower == ".html" || extLower == ".htm")
        return eOutputHTML;
    if(extLower == ".json")
        return eOutputJSON;
    return eOutputText;
}

static string EscapeHTML(const string& input) {
    string result;
    result.reserve(input.size());
    for(size_t i = 0; i < input.size(); ++i) {
        char c = input[i];
        switch(c) {
            case '&':  result += "&amp;"; break;
            case '<':  result += "&lt;"; break;
            case '>':  result += "&gt;"; break;
            case '"':  result += "&quot;"; break;
            case '\'': result += "&#39;"; break;
            default:   result += c; break;
        }
    }
    return result;
}

static string EscapeJSON(const string& input) {
    string result;
    result.reserve(input.size() + 4);
    for(size_t i = 0; i < input.size(); ++i) {
        unsigned char c = (unsigned char)input[i];
        switch(c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if(c < 0x20) {
                    char buf[8];
                    sprintf(buf, "\\u%04x", c);
                    result += buf;
                } else {
                    result += c;
                }
                break;
        }
    }
    return result;
}

static string NormalizeLineEndings(const string& input) {
    string result;
    result.reserve(input.size());
    for(size_t i = 0; i < input.size(); ++i) {
        if(input[i] == '\r' && i + 1 < input.size() && input[i+1] == '\n') {
#ifdef _WIN32
            result += "\r\n";
#else
            result += "\n";
#endif
            ++i;
        } else if(input[i] == '\n') {
#ifdef _WIN32
            result += "\r\n";
#else
            result += "\n";
#endif
        } else if(input[i] == '\r') {
#ifdef _WIN32
            result += "\r\n";
#else
            result += "\n";
#endif
        } else {
            result += input[i];
        }
    }
    return result;
}

static string CollapseBlankLines(const string& input) {
#ifdef _WIN32
    static const char* lineEnd = "\r\n";
    static const size_t lineEndLen = 2;
#else
    static const char* lineEnd = "\n";
    static const size_t lineEndLen = 1;
#endif
    string result;
    result.reserve(input.size());
    size_t i = 0;
    while(i < input.size()) {
        if(input[i] == '\r' && i + 1 < input.size() && input[i+1] == '\n') {
            result.append(lineEnd, lineEndLen);
            i += 2;
            while(i < input.size()) {
                if(input[i] == '\r' && i + 1 < input.size() && input[i+1] == '\n') {
                    i += 2;
                } else if(input[i] == '\n') {
                    ++i;
                } else {
                    break;
                }
            }
        } else if(input[i] == '\n') {
            result.append(lineEnd, lineEndLen);
            ++i;
            while(i < input.size()) {
                if(input[i] == '\r' && i + 1 < input.size() && input[i+1] == '\n') {
                    i += 2;
                } else if(input[i] == '\n') {
                    ++i;
                } else {
                    break;
                }
            }
        } else {
            result += input[i];
            ++i;
        }
    }
    return result;
}

#ifdef _WIN32
static string GetAbsolutePath(const string& path) {
    wstring wPath = UTF8ToWideString(path);
    wchar_t absPath[MAX_PATH] = {0};
    if(GetFullPathNameW(wPath.c_str(), MAX_PATH, absPath, NULL)) {
        return WideStringToUTF8(wstring(absPath));
    }
    return path;
}
#else
static string GetAbsolutePath(const string& path) {
    if(path.empty()) return path;
    if(path[0] == '/') return path;
    char* cwd = getcwd(NULL, 0);
    if(!cwd) return path;
    string result = string(cwd) + "/" + path;
    free(cwd);
    return result;
}
#endif

static const unsigned char scUTF8Bom[3] = {0xEF,0xBB,0xBF};

int main(int argc, char* argv[])
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
#endif

#ifdef _WIN32
    std::vector<std::string> utf8Args = GetUTF8CommandLineArgs();
    if (utf8Args.size() < 2) {
        ShowUsage(utf8Args.empty() ? "extract-text" : utf8Args[0]);
        return 1;
    }
#else
    std::vector<std::string> utf8Args;
    for (int i = 0; i < argc; ++i) {
        utf8Args.push_back(argv[i]);
    }
    if (utf8Args.size() < 2) {
        ShowUsage(argv[0]);
        return 1;
    }
#endif

    vector<string> inputPaths;
    bool debugging = false;
    string debugPath = "";
    bool writeToOutputFile = false;
    string outputFilePath = "";
    TextComposer::ESpacing spacing = TextComposer::eSpacingBoth;
    bool usePageSpec = false;
    set<long> pageSet;
    bool quiet = false;
    long bidiFlag = -1;
    bool extractTables = false;
    bool filterDuplicates = false;
    bool mergeLines = false;

    for (size_t i = 1; i < utf8Args.size(); ++i) {
        std::string arg = utf8Args[i];
        if ((arg == "-h") || (arg == "--help")) {
            ShowUsage(utf8Args[0]);
            return 0;
        } else if ((arg == "-q") || (arg == "--quiet")) {
            quiet = true;
        } else if ((arg == "-f") || (arg == "--filter-dup")) {
            filterDuplicates = true;
        } else if ((arg == "-m") || (arg == "--merge-lines")) {
            mergeLines = true;
        } else if ((arg == "-t") || (arg == "--tables")) {
            extractTables = true;
        } else if ((arg == "-p") || (arg == "--pages")) {
            if (i + 1 < utf8Args.size()) {
                if (!ParsePageSpec(utf8Args[++i], pageSet)) {
                    return 1;
                }
                usePageSpec = true;
            } else {
                std::cerr << "--pages option requires one argument, which is the page specification." << std::endl;
                return 1;                 
            }            
#if (SUPPORT_ICU_BIDI==1)                
        } else if ((arg == "-b") || (arg == "--bidi")) {
            if (i + 1 < utf8Args.size()) {
                string argString = utf8Args[++i];
                if(argString == BIDI_LTR)
                    bidiFlag = 0;
                else if(argString == BIDI_RTL)
                    bidiFlag = 1;
                else {
                    std::cerr << "--bidi option requires one argument to specify document direction, use LTR or RTL." << std::endl;
                    return 1;                     
                }
            } else {
                std::cerr << "--bidi option requires one argument to specify document direction, use LTR or RTL." << std::endl;
                return 1;                 
            } 
#endif
        } else if(arg == "--spacing") {
            if (i + 1 < utf8Args.size()) {
                string argString = utf8Args[++i];
                if(argString == SPACING_BOTH)
                    spacing = TextComposer::eSpacingBoth;
                else if(argString == SPACING_HOR)
                    spacing = TextComposer::eSpacingHorizontal;
                else if(argString == SPACING_VER)
                    spacing = TextComposer::eSpacingVertical;
                else if(argString == SPACING_NONE)
                    spacing = TextComposer::eSpacingNone;
                else {
                    std::cerr << "--spacing option requires one argument, which is the spaces addition policy. Use either BOTH, HOR (for horizontal only), VER (for vertical only) or NONE." << std::endl;
                    return 1;                 
                }
            } else {
                std::cerr << "--spacing option requires one argument, which is the spaces addition policy. Use either BOTH, HOR (for horizontal only), VER (for vertical only) or NONE." << std::endl;
                return 1;                 
            }            

        } else if((arg == "-d") || (arg == "--debug")) {
            debugging = true;
            if (i + 1 < utf8Args.size()) {
                debugPath = utf8Args[++i];
            } else {
                std::cerr << "--debug option requires one argument, which is the debug output file path." << std::endl;
                return 1;                 
            }            
        } else if((arg == "-o") || (arg == "--output")) {
            writeToOutputFile = true;
            if (i + 1 < utf8Args.size()) {
                outputFilePath = utf8Args[++i];
            } else {
                std::cerr << "--output option requires one argument, which is the output file path." << std::endl;
                return 1;                 
            }            
        } else if(arg[0] == '-') {
            cerr << "Unrecognized option " << arg << std::endl ;
            ShowUsage(utf8Args[0]);
            return 1;
        } else {
            inputPaths.push_back(arg);
        }
    }

    if(inputPaths.empty()) {
        cerr << "Error: No input files or directories specified." << endl;
        ShowUsage(utf8Args[0]);
        return 1;
    }

    vector<string> pdfFiles;
    CollectPDFFiles(inputPaths, pdfFiles);

    if(pdfFiles.empty()) {
        cerr << "Error: No PDF files found in the specified paths." << endl;
        return 1;
    }

    if(!quiet && pdfFiles.size() > 1) {
        cerr << "Found " << pdfFiles.size() << " PDF file(s) to process." << endl;
    }

    if(debugging) {
        EStatusCode status = eSuccess;
        for(size_t fi = 0; fi < pdfFiles.size(); ++fi) {
            TextExtraction textExtraction;
            EStatusCode s = textExtraction.DecryptPDFForDebugging(pdfFiles[fi], debugPath);
            if(s != eSuccess) status = s;
        }
        return status == eSuccess ? 0 : 1;
    }

    if(extractTables) {
        EStatusCode globalStatus = eSuccess;
        for(size_t fi = 0; fi < pdfFiles.size(); ++fi) {
            const string& filePath = pdfFiles[fi];
            if(!quiet && pdfFiles.size() > 1) {
                cerr << "[" << (fi+1) << "/" << pdfFiles.size() << "] Processing: " << filePath << endl;
            }

            TableExtraction tableExtraction;
            EStatusCode status;
            if(usePageSpec) {
                status = tableExtraction.ExtractTables(filePath, pageSet);
            } else {
                status = tableExtraction.ExtractTables(filePath);
            }

            if(status != eSuccess) {
                cerr << "Error [" << filePath << "]: " << tableExtraction.LatestError.description << endl;
                globalStatus = status;
                continue;
            }
            ExtractionWarningList::iterator it = tableExtraction.LatestWarnings.begin();
            for(; it != tableExtraction.LatestWarnings.end(); ++it) {
                cerr << "Warning [" << filePath << "]: " << it->description << endl;
            }

            if(writeToOutputFile) {
                size_t extensionPos = outputFilePath.find_last_of(scDot);
                string baseOutputFilePath;
                if(pdfFiles.size() == 1) {
                    baseOutputFilePath = outputFilePath.substr(0, extensionPos);
                } else {
                    size_t lastSep = filePath.find_last_of("/\\");
                    string fileBaseName = (lastSep != string::npos) ? filePath.substr(lastSep + 1) : filePath;
                    size_t dotInName = fileBaseName.find_last_of('.');
                    if(dotInName != string::npos) fileBaseName = fileBaseName.substr(0, dotInName);
                    baseOutputFilePath = outputFilePath.substr(0, extensionPos) + "_" + fileBaseName;
                }
                string outPath  = baseOutputFilePath;
                int ordinal = 0;
                
                TableListList::iterator itPages = tableExtraction.tablesForPages.begin();
                for(; itPages != tableExtraction.tablesForPages.end() && status == eSuccess; ++itPages) {
                    TableList::iterator itTables = itPages->begin();
                    for(; itTables != itPages->end() && status == eSuccess; ++itTables) {
                        string fileFullPath = outPath + scCSVExtension;
                        ofstream outputFile(fileFullPath, ios::binary);
                        if (!outputFile.is_open()) {
                            cerr << "Error: Cannot open target file path for writing in" << fileFullPath << endl;
                            status = eFailure;
                        } else {
                            outputFile.write((const char*)scUTF8Bom, 3);
                            tableExtraction.GetTableAsCSVText(*itTables, bidiFlag, spacing, outputFile);
                            outputFile.close();
                            cerr << "Wrote table to " << fileFullPath << endl;
                        }
                        ++ordinal;
                        outPath = baseOutputFilePath + Int(ordinal).ToString();
                    }
                }
            } else if(!quiet) {
                tableExtraction.GetAllAsCSVText(bidiFlag, spacing, cout);
            }
        }
        return globalStatus == eSuccess ? 0 : 1;
    }

    // Text extraction with streaming multi-file support
    EStatusCode globalStatus = eSuccess;
    size_t successCount = 0;
    OutputFormat fmt = writeToOutputFile ? DetectOutputFormat(outputFilePath) : eOutputText;

    ofstream outputFile;
    if(writeToOutputFile) {
        outputFile.open(outputFilePath, ios::binary);
        if(!outputFile.is_open()) {
            cerr << "Error: Cannot open target file path for writing in" << outputFilePath << endl;
            return 1;
        }
        outputFile.write((const char*)scUTF8Bom, 3);

        if(fmt == eOutputHTML) {
            outputFile << "<!DOCTYPE html>\n<html>\n<head>\n"
                       << "<meta charset=\"UTF-8\">\n"
                       << "<title>PDF Text Extraction</title>\n"
                       << "<style>\n"
                       << "body { font-family: sans-serif; margin: 20px; font-size: small; }\n"
                       << "table { border-collapse: collapse; width: 100%; font-size: small; }\n"
                       << "th, td { border: 1px solid #ccc; padding: 8px; text-align: left; vertical-align: top; }\n"
                       << "th { background-color: #f0f0f0; }\n"
                       << "td.file-path { width: 20%; word-break: break-all; }\n"
                       << "td.page-num { white-space: nowrap; width: 5%; }\n"
                       << "td.text-content { white-space: pre-wrap; width: 75%; }\n"
                       << "</style>\n"
                       << "</head>\n<body>\n"
                       << "<table>\n<tr><th>File</th><th>Page</th><th>Text</th></tr>\n";
        } else if(fmt == eOutputJSON) {
            outputFile << "[\n";
        }
    }

    LineMerger merger;

    for(size_t fi = 0; fi < pdfFiles.size(); ++fi) {
        const string& filePath = pdfFiles[fi];
        string absFilePath = GetAbsolutePath(filePath);
        if(!quiet && pdfFiles.size() > 1) {
            cerr << "[" << (fi+1) << "/" << pdfFiles.size() << "] Processing: " << filePath << endl;
        }

        TextExtraction textExtraction;
        EStatusCode status;
        if(usePageSpec) {
            status = textExtraction.ExtractText(filePath, pageSet);
        } else {
            status = textExtraction.ExtractText(filePath);
        }

        if(status != eSuccess) {
            cerr << "Error [" << filePath << "]: " << textExtraction.LatestError.description << endl;
            globalStatus = status;
            continue;
        }
        ExtractionWarningList::iterator it = textExtraction.LatestWarnings.begin();
        for(; it != textExtraction.LatestWarnings.end(); ++it) {
            cerr << "Warning [" << filePath << "]: " << it->description << endl;
        }

        if(writeToOutputFile) {
            if(fmt == eOutputHTML) {
                size_t pageCount = textExtraction.GetPageCount();
                for(size_t i = 0; i < pageCount; ++i) {
                    long pageNum = textExtraction.GetOriginalPageNumber(i) + 1;
                    stringstream buffer;
                    textExtraction.GetPageAsText(i, bidiFlag, spacing, buffer, filterDuplicates);
                    string text = buffer.str();
                    if(mergeLines)
                        text = merger.MergeLines(text);
                    else
                        text = CollapseBlankLines(text);

                    outputFile << "<tr><td class=\"file-path\">" << EscapeHTML(absFilePath) << "</td>"
                               << "<td class=\"page-num\">" << pageNum << "</td>"
                               << "<td class=\"text-content\">" << EscapeHTML(text) << "</td></tr>\n";
                }
            } else if(fmt == eOutputJSON) {
                if(successCount > 0)
                    outputFile << ",\n";

                outputFile << "  {\n    \"file\": \"" << EscapeJSON(absFilePath) << "\",\n    \"pages\": [\n";

                size_t pageCount = textExtraction.GetPageCount();
                for(size_t i = 0; i < pageCount; ++i) {
                    long pageNum = textExtraction.GetOriginalPageNumber(i) + 1;
                    stringstream buffer;
                    textExtraction.GetPageAsText(i, bidiFlag, spacing, buffer, filterDuplicates);
                    string text = buffer.str();
                    if(mergeLines)
                        text = merger.MergeLines(text);
                    else
                        text = CollapseBlankLines(text);

                    outputFile << "      {\"page\": " << pageNum << ", \"text\": \"" << EscapeJSON(text) << "\"}";
                    if(i + 1 < pageCount)
                        outputFile << ",";
                    outputFile << "\n";
                }

                outputFile << "    ]\n  }";
            } else {
                if(successCount > 0) {
                    outputFile << "\n--- " << absFilePath << " ---\n\n";
                }
                if(mergeLines) {
                    stringstream buffer;
                    textExtraction.GetResultsAsText(bidiFlag, spacing, buffer, filterDuplicates);
                    string merged = NormalizeLineEndings(merger.MergeLines(buffer.str()));
                    outputFile.write(merged.c_str(), merged.size());
                } else {
                    stringstream buffer;
                    textExtraction.GetResultsAsText(bidiFlag, spacing, buffer, filterDuplicates);
                    string text = CollapseBlankLines(buffer.str());
                    outputFile.write(text.c_str(), text.size());
                }
            }
        } else if(!quiet) {
            if(successCount > 0) {
                cout << "\n--- " << absFilePath << " ---\n\n";
            }
            if(mergeLines) {
                stringstream buffer;
                textExtraction.GetResultsAsText(bidiFlag, spacing, buffer, filterDuplicates);
                string merged = NormalizeLineEndings(merger.MergeLines(buffer.str()));
                cout.write(merged.c_str(), merged.size());
            } else {
                stringstream buffer;
                textExtraction.GetResultsAsText(bidiFlag, spacing, buffer, filterDuplicates);
                string text = CollapseBlankLines(buffer.str());
                cout.write(text.c_str(), text.size());
            }
        }

        ++successCount;
    }

    if(writeToOutputFile) {
        if(fmt == eOutputHTML) {
            outputFile << "</table>\n</body>\n</html>\n";
        } else if(fmt == eOutputJSON) {
            outputFile << "\n]\n";
        }
        outputFile.close();
        if(successCount > 0) {
            const char* fmtName = (fmt == eOutputHTML) ? "HTML" : (fmt == eOutputJSON) ? "JSON" : "text";
            cout << "Wrote " << fmtName << " to " << outputFilePath << endl;
        }
    }

    if(successCount == 0) {
        return 1;
    }

    return globalStatus == eSuccess ? 0 : 1;
}
