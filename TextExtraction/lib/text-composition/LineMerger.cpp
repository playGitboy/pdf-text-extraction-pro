#include "LineMerger.h"
#include <algorithm>
#include <cctype>

using namespace std;

static const char* kAbbreviations[] = {
    "Mr","Mrs","Ms","Miss","Dr","Prof","Rev","Hon",
    "Jr","Sr","St","vs","etc","No","Vol","Fig",
    "pp","cf","al","ed","approx","dept","est",
    "Jan","Feb","Mar","Apr","Jun","Jul","Aug",
    "Sep","Oct","Nov","Dec",
    "Ave","Blvd","Rd","Mt","Inc","Ltd","Co","Corp",
    "e.g","i.e","viz","nb","q.v","s.v",
    "U.S","U.K","E.U","P.O",
    NULL
};

static int UTF8CharLen(unsigned char c) {
    if (c < 0x80) return 1;
    if (c < 0xC0) return 0;
    if (c < 0xE0) return 2;
    if (c < 0xF0) return 3;
    return 4;
}

static unsigned int DecodeUTF8Char(const char* data, size_t pos, size_t len, size_t charLen) {
    if (pos + charLen > len) return 0;
    unsigned char b = (unsigned char)data[pos];
    if (charLen == 1) return b;
    if (charLen == 2) return ((b & 0x1F) << 6) | ((unsigned char)data[pos+1] & 0x3F);
    if (charLen == 3) return ((b & 0x0F) << 12) | ((unsigned char)data[pos+1] & 0x3F) << 6 | ((unsigned char)data[pos+2] & 0x3F);
    if (charLen == 4) return ((b & 0x07) << 18) | ((unsigned char)data[pos+1] & 0x3F) << 12 | ((unsigned char)data[pos+2] & 0x3F) << 6 | ((unsigned char)data[pos+3] & 0x3F);
    return 0;
}

static bool IsCJKCodepoint(unsigned int cp) {
    return (cp >= 0x4E00 && cp <= 0x9FFF) ||
           (cp >= 0x3400 && cp <= 0x4DBF) ||
           (cp >= 0xF900 && cp <= 0xFAFF) ||
           cp == 0x3007;
}

static bool IsCJKOrPunctCodepoint(unsigned int cp) {
    return (cp >= 0x2E80 && cp <= 0x9FFF) ||
           (cp >= 0xF900 && cp <= 0xFAFF) ||
           (cp >= 0xFF00 && cp <= 0xFFEF) ||
           (cp >= 0x3000 && cp <= 0x303F);
}

static string GetLastUTF8Char(const string& s) {
    if (s.empty()) return "";
    int i = (int)s.size() - 1;
    while (i > 0 && (unsigned char)s[i] >= 0x80 && (unsigned char)s[i] < 0xC0) i--;
    int len = UTF8CharLen((unsigned char)s[i]);
    if (len == 0 || i + len > (int)s.size()) return s.substr(i, 1);
    return s.substr(i, len);
}

static string GetFirstUTF8Char(const string& s) {
    if (s.empty()) return "";
    int len = UTF8CharLen((unsigned char)s[0]);
    if (len == 0) return s.substr(0, 1);
    return s.substr(0, len);
}

static bool StringEndsWith(const string& s, const string& suffix) {
    if (suffix.size() > s.size()) return false;
    return s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

LineMerger::LineMerger() {
    reChapterCN = regex(u8"^[ \t]*\u7b2c[\u4e00\u4e8c\u4e09\u56db\u4e94\u516d\u4e03\u516b\u4e5d\u5341\u767e\u5343\u96f6\u3007\\d]+[\u7ae0\u8282\u6761\u6b3e\u9879\u90e8\u5206\u7f16\u7bc7]");
    reNumberCN = regex(u8"^[ \t]*[\u4e00\u4e8c\u4e09\u56db\u4e94\u516d\u4e03\u516b\u4e5d\u5341\u767e\u96f6\u3007]+[\u3001\\.\uff0e]");
    reNumberArabic = regex(u8"^[ \t]*\\d+[\\.\u3001]");
    reNumberMultiLevel = regex(u8"^[ \t]*\\d+(\\.\\d+)+\\.?");
    reNumberAlpha = regex(u8"^[ \t]*[A-Za-z][\\.\\.\uff0e)]");
    reNumberRoman = regex(u8"^[ \t]*[IVXLCDM]{1,5}[\\.\\.\uff0e]");
    reCircledNum = regex(u8"^[ \t]*[\u2460\u2461\u2462\u2463\u2464\u2465\u2466\u2467\u2468\u2469\u246a\u246b\u246c\u246d\u246e\u246f\u2470\u2471\u2472\u2473]");
    reBullet = regex(u8"^[ \t]*[\u2022\u00b7\u203b\u2606\u2605\u25cb\u25cf\u25c7\u25c6\u25a1\u25a0\u25b3\u25b2\u25ba\u27a4\u27a2\u2713\u2714\u2717\u2718\u278a\u278b\u278c\u278d\u278e]");
    reDashList = regex(u8"^[ \t]*[-\u2014\u2013]\\s");
    reKeywordCN = regex(u8"^[ \t]*(?:\u7532\u65b9|\u4e59\u65b9|\u4e19\u65b9|\u4e01\u65b9|\u9274\u4e8e|\u603b\u5219|\u9644\u5219|\u9644\u4ef6|\u9644\u5f55|\u524d\u8a00|\u5e8f\u8a00|\u6458\u8981|\u76ee\u5f55|\u5f15\u8a00|\u5b9a\u4e49|\u91ca\u4e49|\u8bf4\u660e|\u5907\u6ce8|\u7b7e\u7f72|\u7b7e\u7ae0|\u76d6\u7ae0|\u751f\u6548|\u5931\u6548|\u7ec8\u6b62|\u89e3\u9664|\u4fee\u8ba2|\u8865\u5145|\u8303\u56f4|\u76ee\u7684|\u9002\u7528|\u51fa\u5356\u4eba|\u4e70\u53d7\u4eba|\u59d4\u6258\u4eba|\u53d7\u6258\u4eba|\u51fa\u79df\u4eba|\u627f\u79df\u4eba|\u62c5\u4fdd\u4eba|\u62b5\u62bc\u4eba|\u8d28\u6743\u4eba|\u503a\u6743\u4eba|\u503a\u52a1\u4eba|\u4ee3\u7406\u4eba|\u6cd5\u5b9a\u4ee3\u8868\u4eba|\u6388\u6743\u4eba|\u88ab\u6388\u6743\u4eba|\u8f6c\u8ba9\u4eba|\u53d7\u8ba9\u4eba|\u5f00\u53d1\u5546|\u627f\u5305\u5546|\u65bd\u5de5\u65b9|\u76d1\u7406\u4eba|\u5ba1\u8ba1\u4eba|\u7ba1\u7406\u4eba|\u7ecf\u529e\u4eba|\u8d1f\u8d23\u4eba|\u8054\u7cfb\u4eba|\u7532\u65b9\u4ee3\u7406\u4eba|\u4e59\u65b9\u4ee3\u7406\u4eba)[\u4e00\u4e8c\u4e09\u56db\u4e94\u516d\u4e03\u516b\u4e5d\u5341\u767e\u5343\u96f6\u3007\\d]*(?:[\uff1a:\uff08(]|$)");
    reKeywordEN = regex(u8"^[ \t]*(?:Article|Section|Chapter|Part|Appendix|Annex|Schedule|Exhibit|Clause|Item|Paragraph|Subsection|Abstract|Introduction|Conclusion|References|Bibliography|Acknowledgments|Contents|Summary)(?:\\s|$)");
    reHyphenBreak = regex("(\\w)-[ \\t]*\\r?\\n[ \\t]*([a-z])");
}

LineMerger::~LineMerger() {}

bool LineMerger::IsCJKChar(unsigned int ch) {
    return (ch >= 0x4E00 && ch <= 0x9FFF) ||
           (ch >= 0x3400 && ch <= 0x4DBF) ||
           (ch >= 0xF900 && ch <= 0xFAFF);
}

bool LineMerger::IsAsciiWordChar(char ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9');
}

static bool IsStrongTerminatorStr(const string& ch) {
    return ch == u8"\u3002" || ch == u8"\uff01" || ch == u8"\uff1f" ||
           ch == "." || ch == "!" || ch == "?" ||
           ch == u8"\u2026";
}

static bool IsClauseTerminatorStr(const string& ch) {
    return ch == u8"\uff1b" || ch == u8"\uff1a" || ch == ";" || ch == ":";
}

static bool IsCJKPunctuationStr(const string& ch) {
    return ch == u8"\uff0c" || ch == u8"\u3001" ||
           ch == u8"\u3002" || ch == u8"\uff01" || ch == u8"\uff1f" ||
           ch == u8"\uff1b" || ch == u8"\uff1a" ||
           ch == u8"\u201c" || ch == u8"\u201d" ||
           ch == u8"\u2018" || ch == u8"\u2019" ||
           ch == u8"\u300a" || ch == u8"\u300b" ||
           ch == u8"\u3010" || ch == u8"\u3011" ||
           ch == u8"\uff08" || ch == u8"\uff09" ||
           ch == u8"\u2026";
}

static bool IsSkipCharStr(const string& ch) {
    return ch == "\"" || ch == u8"\u201c" || ch == u8"\u201d" ||
           ch == "'" || ch == "`" || ch == u8"\u2018" || ch == u8"\u2019" ||
           ch == u8"\u300b" || ch == u8"\u3011" || ch == u8"\u3015" ||
           ch == u8"\uff09" || ch == ")" ||
           ch == u8"\u301d" || ch == u8"\u301f";
}

static bool IsCJKCharStr(const string& ch) {
    if (ch.size() < 3) return false;
    unsigned int codepoint = 0;
    if ((unsigned char)ch[0] >= 0xE0) {
        codepoint = ((unsigned char)ch[0] & 0x0F) << 12;
        codepoint |= ((unsigned char)ch[1] & 0x3F) << 6;
        codepoint |= ((unsigned char)ch[2] & 0x3F);
    }
    return (codepoint >= 0x4E00 && codepoint <= 0x9FFF) ||
           (codepoint >= 0x3400 && codepoint <= 0x4DBF) ||
           (codepoint >= 0xF900 && codepoint <= 0xFAFF);
}

bool LineMerger::IsAbbreviation(const string& word) {
    for (int i = 0; kAbbreviations[i] != NULL; ++i) {
        if (word == kAbbreviations[i]) return true;
    }
    return false;
}

string LineMerger::GetEffectiveLastChar(const string& line) {
    string ch = GetLastUTF8Char(line);
    while (!ch.empty() && IsSkipCharStr(ch)) {
        string remaining = line.substr(0, line.size() - ch.size());
        ch = GetLastUTF8Char(remaining);
    }
    return ch;
}

bool LineMerger::IsStructureStart(const string& line) {
    if (line.empty()) return false;
    try {
        if (regex_search(line, reChapterCN)) return true;
        if (regex_search(line, reNumberCN)) return true;
        if (regex_search(line, reNumberMultiLevel)) return true;
        if (regex_search(line, reNumberArabic)) return true;
        if (regex_search(line, reNumberAlpha)) return true;
        if (regex_search(line, reNumberRoman)) return true;
        if (regex_search(line, reCircledNum)) return true;
        if (regex_search(line, reBullet)) return true;
        if (regex_search(line, reDashList)) return true;
        if (regex_search(line, reKeywordCN)) return true;
        if (regex_search(line, reKeywordEN)) return true;
    } catch (...) {}
    if (IsFieldLabelLine(line)) return true;
    if (IsParenNumberLine(line)) return true;
    return false;
}

bool LineMerger::IsShortHeading(const string& line) {
    if (line.empty()) return false;
    if (line.size() > 35) return false;
    return IsStructureStart(line);
}

bool LineMerger::IsFieldLabelLine(const string& line) {
    if (line.empty()) return false;
    size_t pos = 0;
    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) ++pos;
    if (pos >= line.size()) return false;

    int cjkCount = 0;
    while (pos < line.size()) {
        unsigned char b = (unsigned char)line[pos];
        size_t charLen = UTF8CharLen(b);
        if (charLen == 0 || pos + charLen > line.size()) break;

        unsigned int codepoint = DecodeUTF8Char(line.c_str(), pos, line.size(), charLen);

        if (IsCJKCodepoint(codepoint)) {
            ++cjkCount;
            if (cjkCount > 8) return false;
            pos += charLen;
        } else if (codepoint == 0xFF1A || codepoint == ':') {
            return cjkCount >= 1 && cjkCount <= 8;
        } else {
            return false;
        }
    }
    return false;
}

bool LineMerger::IsParenNumberLine(const string& line) {
    if (line.empty()) return false;
    size_t pos = 0;
    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) ++pos;
    if (pos >= line.size()) return false;

    unsigned char b = (unsigned char)line[pos];
    size_t openLen = UTF8CharLen(b);
    if (openLen == 0 || pos + openLen > line.size()) return false;
    unsigned int openCp = DecodeUTF8Char(line.c_str(), pos, line.size(), openLen);
    bool isFullwidthOpen = (openCp == 0xFF08);
    bool isHalfwidthOpen = (openCp == '(');
    if (!isFullwidthOpen && !isHalfwidthOpen) return false;
    pos += openLen;

    int innerCount = 0;
    while (pos < line.size()) {
        unsigned char cb = (unsigned char)line[pos];
        size_t cLen = UTF8CharLen(cb);
        if (cLen == 0 || pos + cLen > line.size()) break;
        unsigned int cp = DecodeUTF8Char(line.c_str(), pos, line.size(), cLen);

        if (IsCJKCodepoint(cp) || (cp >= '0' && cp <= '9')) {
            ++innerCount;
            if (innerCount > 4) return false;
            pos += cLen;
        } else if ((isFullwidthOpen && cp == 0xFF09) || (isHalfwidthOpen && cp == ')')) {
            return innerCount >= 1;
        } else {
            return false;
        }
    }
    return false;
}

bool LineMerger::IsStandaloneParenLine(const string& line) {
    if (line.empty()) return false;
    string trimmed = line;
    size_t start = trimmed.find_first_not_of(" \t\r\n");
    size_t end = trimmed.find_last_not_of(" \t\r\n");
    if (start == string::npos) return false;
    trimmed = trimmed.substr(start, end - start + 1);
    if (trimmed.size() < 2) return false;

    string firstCh = GetFirstUTF8Char(trimmed);
    string lastCh = GetLastUTF8Char(trimmed);

    bool startsWithOpen = firstCh == u8"\uff08" || firstCh == "(" ||
                          firstCh == u8"\u300a" || firstCh == "<" ||
                          firstCh == u8"\u3010" || firstCh == "[";
    bool endsWithClose = lastCh == u8"\uff09" || lastCh == ")" ||
                         lastCh == u8"\u300b" || lastCh == ">" ||
                         lastCh == u8"\u3011" || lastCh == "]";

    return startsWithOpen && endsWithClose;
}

bool LineMerger::HasMultiColumnGap(const string& line) {
    size_t consecutiveSpaces = 0;
    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] == ' ') {
            ++consecutiveSpaces;
            if (consecutiveSpaces >= 4) return true;
        } else {
            consecutiveSpaces = 0;
        }
    }
    return false;
}

string LineMerger::Preprocess(const string& input) {
    string result = input;
    result = regex_replace(result, regex("\\r\\n"), "\n");
    result = regex_replace(result, regex("\\r"), "\n");
    result = regex_replace(result, reHyphenBreak, "$1$2");
    return result;
}

string LineMerger::Postprocess(const string& input) {
    string result = input;

    static const vector<string> cnPuncts = {
        u8"\u3002", u8"\uff01", u8"\uff1f", u8"\uff1b", u8"\uff1a",
        u8"\uff0c", u8"\u3001", u8"\u201c", u8"\u201d", u8"\u2018",
        u8"\u2019", u8"\u300b", u8"\u3011", u8"\u3015", u8"\uff09",
        u8"\u2026"
    };
    for (size_t i = 0; i < cnPuncts.size(); ++i) {
        string target = " " + cnPuncts[i];
        size_t pos = 0;
        while ((pos = result.find(target, pos)) != string::npos) {
            result.replace(pos, target.size(), cnPuncts[i]);
        }
    }

    static const string enPuncts = ".,;:!?)";
    for (size_t i = 0; i < enPuncts.size(); ++i) {
        string target = string(" ") + enPuncts[i];
        size_t pos = 0;
        while ((pos = result.find(target, pos)) != string::npos) {
            result.replace(pos, target.size(), string(1, enPuncts[i]));
        }
    }

    size_t pos = 0;
    while ((pos = result.find("  ", pos)) != string::npos) {
        result.replace(pos, 2, " ");
    }

    {
        string cleaned;
        cleaned.reserve(result.size());
        size_t i = 0;
        while (i < result.size()) {
            if (result[i] == ' ') {
                bool prevIsCJKType = false;
                {
                    size_t j = i;
                    while (j > 0 && (unsigned char)result[j-1] >= 0x80 && (unsigned char)result[j-1] < 0xC0) --j;
                    if (j > 0) {
                        unsigned char pb = (unsigned char)result[j-1];
                        size_t pLen = UTF8CharLen(pb);
                        if (pLen >= 2 && j - 1 + pLen <= i) {
                            unsigned int pcp = DecodeUTF8Char(result.c_str(), j - 1, result.size(), pLen);
                            prevIsCJKType = IsCJKOrPunctCodepoint(pcp);
                        }
                    }
                }

                bool nextIsCJKType = false;
                {
                    size_t k = i + 1;
                    if (k < result.size()) {
                        unsigned char nb = (unsigned char)result[k];
                        size_t nLen = UTF8CharLen(nb);
                        if (nLen >= 2 && k + nLen <= result.size()) {
                            unsigned int ncp = DecodeUTF8Char(result.c_str(), k, result.size(), nLen);
                            nextIsCJKType = IsCJKOrPunctCodepoint(ncp);
                        }
                    }
                }

                if (prevIsCJKType && nextIsCJKType) {
                    ++i;
                    continue;
                }
            }
            cleaned += result[i];
            ++i;
        }
        result = cleaned;
    }

    pos = 0;
    while ((pos = result.find("\n\n", pos)) != string::npos) {
        result.replace(pos, 2, "\n");
    }

    return result;
}

vector<string> LineMerger::SplitLines(const string& input) {
    vector<string> lines;
    istringstream stream(input);
    string line;
    while (getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

static bool IsAsciiDigit(char ch) {
    return ch >= '0' && ch <= '9';
}

static bool ShouldAddSpaceBetween(const string& lastCharStr, const string& nextCharStr) {
    if (lastCharStr.empty() || nextCharStr.empty()) return false;

    bool lastIsAsciiWord = lastCharStr.size() == 1 && LineMerger::IsAsciiWordChar(lastCharStr[0]);
    bool nextIsAsciiWord = nextCharStr.size() == 1 && LineMerger::IsAsciiWordChar(nextCharStr[0]);

    if (lastIsAsciiWord && nextIsAsciiWord) {
        if (lastCharStr.size() == 1 && IsAsciiDigit(lastCharStr[0]) &&
            nextCharStr.size() == 1 && IsAsciiDigit(nextCharStr[0]))
            return false;
        return true;
    }

    if (lastIsAsciiWord && IsCJKCharStr(nextCharStr)) return false;
    if (IsCJKCharStr(lastCharStr) && nextIsAsciiWord) return true;
    if (IsCJKCharStr(lastCharStr) && IsCJKCharStr(nextCharStr)) return false;

    if (lastCharStr == ")" || lastCharStr == "]" || lastCharStr == "}" ||
        lastCharStr == u8"\uff09" || lastCharStr == u8"\u3011" ||
        lastCharStr == "\"" || lastCharStr == "'") {
        if (nextIsAsciiWord) return true;
    }

    if (lastCharStr == "," || lastCharStr == ";" || lastCharStr == ":" ||
        lastCharStr == "!" || lastCharStr == "?" ||
        lastCharStr == u8"\uff0c" || lastCharStr == u8"\u3001") {
        if (nextIsAsciiWord) return true;
        if (IsCJKCharStr(nextCharStr)) return false;
    }

    return false;
}

static vector<string> SplitMultiColumnLine(const string& line) {
    vector<string> result;
    size_t start = 0;
    size_t i = 0;
    while (i < line.size()) {
        if (line[i] == ' ') {
            size_t gapStart = i;
            while (i < line.size() && line[i] == ' ') ++i;
            if (i - gapStart >= 4) {
                if (gapStart > start)
                    result.push_back(line.substr(start, gapStart - start));
                start = i;
            }
        } else {
            ++i;
        }
    }
    if (start < line.size())
        result.push_back(line.substr(start));
    return result;
}

string LineMerger::MergeLines(const string& input) {
    string preprocessed = Preprocess(input);
    vector<string> rawLines = SplitLines(preprocessed);

    if (rawLines.size() <= 1) return input;

    vector<string> lines;
    for (size_t ri = 0; ri < rawLines.size(); ++ri) {
        const string& line = rawLines[ri];
        if (HasMultiColumnGap(line)) {
            vector<string> parts = SplitMultiColumnLine(line);
            for (size_t pi = 0; pi < parts.size(); ++pi)
                lines.push_back(parts[pi]);
        } else {
            lines.push_back(line);
        }
    }

    if (lines.size() <= 1) return input;

    vector<string> merged;
    merged.push_back(lines[0]);

    for (size_t i = 1; i < lines.size(); ++i) {
        string& prev = merged.back();
        const string& curr = lines[i];

        if (prev.empty() || curr.empty()) {
            merged.push_back(curr);
            continue;
        }

        string trimmedPrev = prev;
        {
            size_t p = trimmedPrev.find_last_not_of(" \t");
            if (p != string::npos) trimmedPrev = trimmedPrev.substr(0, p + 1);
        }

        string trimmedCurr = curr;
        {
            size_t p = trimmedCurr.find_first_not_of(" \t");
            if (p != string::npos) trimmedCurr = trimmedCurr.substr(p);
        }

        if (trimmedPrev.empty() || trimmedCurr.empty()) {
            merged.push_back(curr);
            continue;
        }

        string effectiveLastStr = GetEffectiveLastChar(trimmedPrev);

        if (IsStrongTerminatorStr(effectiveLastStr)) {
            if (effectiveLastStr == ".") {
                size_t dotPos = trimmedPrev.rfind('.');
                if (dotPos != string::npos && dotPos >= 2) {
                    string beforeDot = trimmedPrev.substr(0, dotPos);
                    size_t wordStart = beforeDot.find_last_of(" \t,;:!?");
                    string lastWord = (wordStart == string::npos) ? beforeDot : beforeDot.substr(wordStart + 1);
                    if (IsAbbreviation(lastWord)) {
                        {
                            size_t tp = prev.find_last_not_of(" \t");
                            if (tp != string::npos && tp + 1 < prev.size()) prev = prev.substr(0, tp + 1);
                        }
                        string nextChStr = GetFirstUTF8Char(trimmedCurr);
                        if (ShouldAddSpaceBetween(effectiveLastStr, nextChStr)) {
                            prev += " " + trimmedCurr;
                        } else {
                            prev += trimmedCurr;
                        }
                        continue;
                    }
                }
            }
            merged.push_back(curr);
            continue;
        }

        if (IsClauseTerminatorStr(effectiveLastStr)) {
            merged.push_back(curr);
            continue;
        }

        if (IsShortHeading(trimmedPrev)) {
            merged.push_back(curr);
            continue;
        }

        if (HasMultiColumnGap(prev)) {
            merged.push_back(curr);
            continue;
        }

        if (IsStandaloneParenLine(trimmedPrev)) {
            merged.push_back(curr);
            continue;
        }

        if (IsStructureStart(trimmedCurr)) {
            merged.push_back(curr);
            continue;
        }

        {
            size_t p = prev.find_last_not_of(" \t");
            if (p != string::npos && p + 1 < prev.size()) prev = prev.substr(0, p + 1);
        }

        string nextChStr = GetFirstUTF8Char(trimmedCurr);
        if (ShouldAddSpaceBetween(effectiveLastStr, nextChStr)) {
            prev += " " + trimmedCurr;
        } else {
            prev += trimmedCurr;
        }
    }

    string result;
    for (size_t i = 0; i < merged.size(); ++i) {
        if (i > 0) result += "\n";
        result += merged[i];
    }

    result = Postprocess(result);

    return result;
}
