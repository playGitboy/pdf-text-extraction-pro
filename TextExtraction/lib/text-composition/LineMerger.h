#pragma once

#include <string>
#include <sstream>
#include <vector>
#include <regex>

class LineMerger {
public:
    LineMerger();
    ~LineMerger();

    std::string MergeLines(const std::string& input);

    static bool IsAsciiWordChar(char ch);

private:
    std::regex reChapterCN;
    std::regex reNumberCN;
    std::regex reNumberArabic;
    std::regex reNumberMultiLevel;
    std::regex reNumberAlpha;
    std::regex reNumberRoman;
    std::regex reCircledNum;
    std::regex reBullet;
    std::regex reDashList;
    std::regex reKeywordCN;
    std::regex reKeywordEN;
    std::regex reHyphenBreak;

    bool IsStructureStart(const std::string& line);
    bool IsShortHeading(const std::string& line);
    bool IsStandaloneParenLine(const std::string& line);
    bool IsFieldLabelLine(const std::string& line);
    bool IsParenNumberLine(const std::string& line);
    bool HasMultiColumnGap(const std::string& line);
    bool IsAbbreviation(const std::string& word);
    bool IsCJKChar(unsigned int ch);
    std::string GetEffectiveLastChar(const std::string& line);
    std::string Preprocess(const std::string& input);
    std::string Postprocess(const std::string& input);
    std::vector<std::string> SplitLines(const std::string& input);
};
