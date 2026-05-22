#include "TextExtraction.h"

#include "InputFile.h"
#include "PDFParser.h"
#include "PDFWriter.h"
#include "PDFPageInput.h"

#include "./lib/interpreter/PDFRecursiveInterpreter.h"
#include "./lib/graphic-content-parsing/GraphicContentInterpreter.h"
#include "./lib/math/Transformations.h"

using namespace std;
using namespace PDFHummus;

TextExtraction::TextExtraction():textInterpeter(this) {

}
    
TextExtraction::~TextExtraction() {
    textsForPages.clear();
}

bool TextExtraction::OnParsedTextPlacementComplete(const ParsedTextPlacement& inParsedTextPlacement) {
    // filter out elements outside of the page box
    if(DoBoxesIntersect(currentPageScopeBox, inParsedTextPlacement.globalBbox))
        textsForPages.back().push_back(inParsedTextPlacement);
    return true;
}


bool TextExtraction::OnTextElementComplete(const TextElement& inTextElement) {
    return textInterpeter.OnTextElementComplete(inTextElement);
}

bool TextExtraction::OnPathPainted(const PathElement& inPathElement) {
    // IGNORE (not relevant for text extraction)
    return true;
}


bool TextExtraction::OnResourcesRead(const Resources& inResources, IInterpreterContext* inContext) {
    return textInterpeter.OnResourcesRead(inResources, inContext);
}

EStatusCode TextExtraction::ExtractTextPlacements(PDFParser* inParser, long inStartPage, long inEndPage) {
    EStatusCode status = eSuccess;
    unsigned long start = (unsigned long)(inStartPage >= 0 ? inStartPage : (inParser->GetPagesCount() + inStartPage));
    unsigned long end = (unsigned long)(inEndPage >= 0 ? inEndPage :  (inParser->GetPagesCount() + inEndPage));
    GraphicContentInterpreter interpreter;


    if(end > inParser->GetPagesCount()-1)
        end = inParser->GetPagesCount()-1;
    if(start > end)
        start = end;

    for(unsigned long i=start;i<=end && status == eSuccess;++i) {
        RefCountPtr<PDFDictionary> pageObject(inParser->ParsePage(i));
        if(!pageObject) {
            status = eFailure;
            break;
        }

        PDFPageInput pageInput(inParser,pageObject);
        PDFRectangle mediaBox = pageInput.GetMediaBox();
        currentPageScopeBox[0] = mediaBox.LowerLeftX;
        currentPageScopeBox[1] = mediaBox.LowerLeftY;
        currentPageScopeBox[2] = mediaBox.UpperRightX;
        currentPageScopeBox[3] = mediaBox.UpperRightY;

        textsForPages.push_back(ParsedTextPlacementList());
        extractedPageNumbers.push_back((long)i);
        interpreter.InterpretPageContents(inParser, pageObject.GetPtr(), this);  
    }    

    textInterpeter.ResetInterpretationState();

    return status;
}

EStatusCode TextExtraction::ExtractTextPlacements(PDFParser* inParser, const std::set<long>& inPages) {
    EStatusCode status = eSuccess;
    unsigned long totalPages = inParser->GetPagesCount();
    GraphicContentInterpreter interpreter;

    for(set<long>::const_iterator it = inPages.begin(); it != inPages.end() && status == eSuccess; ++it) {
        unsigned long pageIdx = (unsigned long)(*it >= 0 ? *it : (long)(totalPages + *it));
        if(pageIdx >= totalPages)
            continue;

        RefCountPtr<PDFDictionary> pageObject(inParser->ParsePage(pageIdx));
        if(!pageObject) {
            status = eFailure;
            break;
        }

        PDFPageInput pageInput(inParser,pageObject);
        PDFRectangle mediaBox = pageInput.GetMediaBox();
        currentPageScopeBox[0] = mediaBox.LowerLeftX;
        currentPageScopeBox[1] = mediaBox.LowerLeftY;
        currentPageScopeBox[2] = mediaBox.UpperRightX;
        currentPageScopeBox[3] = mediaBox.UpperRightY;

        textsForPages.push_back(ParsedTextPlacementList());
        extractedPageNumbers.push_back((long)pageIdx);
        interpreter.InterpretPageContents(inParser, pageObject.GetPtr(), this);  
    }    

    textInterpeter.ResetInterpretationState();

    return status;
}

static const string scEmpty = "";

void TextExtraction::ClearState() {
    textsForPages.clear();
    extractedPageNumbers.clear();
    LatestWarnings.clear();
    LatestError.code = eErrorNone;
    LatestError.description = scEmpty;
}

EStatusCode TextExtraction::ExtractText(const std::string& inFilePath, long inStartPage, long inEndPage) {
    EStatusCode status = eSuccess;
    InputFile sourceFile;

    ClearState();

    do {
        status = sourceFile.OpenFile(inFilePath);
        if (status != eSuccess) {
            LatestError.code = eErrorFileNotReadable;
            LatestError.description = string("Cannot read file ") + inFilePath;
            break;
        }


        PDFParser parser;
        status = parser.StartPDFParsing(sourceFile.GetInputStream());
        if(status != eSuccess)
        {
            LatestError.code = eErrorInternalPDFWriter;
            LatestError.description = string("Failed to parse file");
            break;
        }

        status = ExtractTextPlacements(&parser, inStartPage, inEndPage);
        if(status != eSuccess)
            break;

    } while(false);

    return status;
}

PDFHummus::EStatusCode TextExtraction::ExtractText(PDFParser* inParser, long inStartPage, long inEndPage) {
    ClearState();

    return ExtractTextPlacements(inParser, inStartPage, inEndPage);
}

PDFHummus::EStatusCode TextExtraction::ExtractText(IByteReaderWithPosition* inStream, long inStartPage, long inEndPage) {
    EStatusCode status = eSuccess;
    InputFile sourceFile;

    ClearState();

    do {
        PDFParser parser;
        status = parser.StartPDFParsing(inStream);
        if(status != eSuccess)
        {
            LatestError.code = eErrorInternalPDFWriter;
            LatestError.description = string("Failed to parse file");
            break;
        }

        status = ExtractTextPlacements(&parser, inStartPage, inEndPage);
        if(status != eSuccess)
            break;

    } while(false);

    return status;
}

PDFHummus::EStatusCode TextExtraction::ExtractText(const std::string& inFilePath, const std::set<long>& inPages) {
    EStatusCode status = eSuccess;
    InputFile sourceFile;

    ClearState();

    do {
        status = sourceFile.OpenFile(inFilePath);
        if (status != eSuccess) {
            LatestError.code = eErrorFileNotReadable;
            LatestError.description = string("Cannot read file ") + inFilePath;
            break;
        }

        PDFParser parser;
        status = parser.StartPDFParsing(sourceFile.GetInputStream());
        if(status != eSuccess)
        {
            LatestError.code = eErrorInternalPDFWriter;
            LatestError.description = string("Failed to parse file");
            break;
        }

        status = ExtractTextPlacements(&parser, inPages);
        if(status != eSuccess)
            break;

    } while(false);

    return status;
}

PDFHummus::EStatusCode TextExtraction::ExtractText(PDFParser* inParser, const std::set<long>& inPages) {
    ClearState();
    return ExtractTextPlacements(inParser, inPages);
}

PDFHummus::EStatusCode TextExtraction::ExtractText(IByteReaderWithPosition* inStream, const std::set<long>& inPages) {
    EStatusCode status = eSuccess;

    ClearState();

    do {
        PDFParser parser;
        status = parser.StartPDFParsing(inStream);
        if(status != eSuccess)
        {
            LatestError.code = eErrorInternalPDFWriter;
            LatestError.description = string("Failed to parse file");
            break;
        }

        status = ExtractTextPlacements(&parser, inPages);
        if(status != eSuccess)
            break;

    } while(false);

    return status;
}

static const string scCRLN = "\r\n";

void TextExtraction::GetResultsAsText(int bidiFlag, TextComposer::ESpacing spacingFlag, std::ostream& outStream, bool filterDuplicates) {
    ParsedTextPlacementListList::iterator itPages = textsForPages.begin();
    TextComposer composer(bidiFlag, spacingFlag, filterDuplicates);

    for(; itPages != textsForPages.end();++itPages) {
        composer.ComposeText(*itPages, outStream);
        outStream<<scCRLN;
    }
}

void TextExtraction::GetPageAsText(size_t pageIndex, int bidiFlag, TextComposer::ESpacing spacingFlag, std::ostream& outStream, bool filterDuplicates) {
    if(pageIndex >= textsForPages.size())
        return;

    ParsedTextPlacementListList::iterator itPages = textsForPages.begin();
    advance(itPages, pageIndex);
    TextComposer composer(bidiFlag, spacingFlag, filterDuplicates);
    composer.ComposeText(*itPages, outStream);
}

size_t TextExtraction::GetPageCount() const {
    return textsForPages.size();
}

long TextExtraction::GetOriginalPageNumber(size_t pageIndex) const {
    if(pageIndex >= extractedPageNumbers.size())
        return -1;
    return extractedPageNumbers[pageIndex];
}


EStatusCode TextExtraction::DecryptPDFForDebugging(
    const string& inTemplateFilePath,
    const string& inTargetOutputFilePath
) {
    return PDFWriter::RecryptPDF(  
		inTemplateFilePath,
		"",
		inTargetOutputFilePath,
        LogConfiguration::DefaultLogConfiguration(),
		PDFCreationSettings(false, true)
    );
}
