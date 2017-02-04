// DumpLine.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <Dwrite.h>
#include <cassert>
#include <iostream>
#include <map>
#include <vector>
#include <wrl.h>

class TextAnalysisSource : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDWriteTextAnalysisSource> {
public:
	TextAnalysisSource(const WCHAR* textString, UINT32 textLength) : textString(textString), textLength(textLength) {
	}

private:
	IFACEMETHODIMP GetTextAtPosition(UINT32 textPosition, const WCHAR **textString, UINT32 *textLength) override final {
		if (textPosition >= this->textLength) {
			*textString = nullptr;
			*textLength = 0;
		}
		else {
			*textString = this->textString + textPosition;
			*textLength = this->textLength - textPosition;
		}
		return S_OK;
	}

	IFACEMETHODIMP GetTextBeforePosition(UINT32 textPosition, const WCHAR **textString, UINT32 *textLength) override final {
		if (textPosition == 0 || textPosition > this->textLength) {
			*textString = nullptr;
			*textLength = 0;
		}
		else {
			*textString = this->textString;
			*textLength = textPosition;
		}
		return S_OK;
	}

	IFACEMETHODIMP_(DWRITE_READING_DIRECTION) GetParagraphReadingDirection() override final {
		return DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
	}

	IFACEMETHODIMP GetLocaleName(UINT32 textPosition, UINT32* textLength, const WCHAR** localeName) override final {
		*textLength = this->textLength;
		*localeName = nullptr;
		return S_OK;
	}

	IFACEMETHODIMP GetNumberSubstitution(UINT32 textPosition, UINT32* textLength, IDWriteNumberSubstitution** numberSubstitution) override final {
		*textLength = this->textLength;
		*numberSubstitution = nullptr;
		return S_OK;
	}

	const WCHAR* textString;
	UINT32 textLength;
};

class TextAnalysisSink : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDWriteTextAnalysisSink> {
public:
	TextAnalysisSink() {
	}

	const std::map<UINT32, DWRITE_SCRIPT_ANALYSIS>& getScriptAnalyses() const {
		return scriptAnalyses;
	}

private:
	STDMETHODIMP SetScriptAnalysis(UINT32 textPosition, UINT32 textLength, const DWRITE_SCRIPT_ANALYSIS* scriptAnalysis) override final {
		// Assume that the runs reported will never be duplicated or overlap, and that there are no holes.
		scriptAnalyses.insert(std::make_pair(textPosition, *scriptAnalysis));
		return S_OK;
	}

	STDMETHODIMP SetLineBreakpoints(UINT32 textPosition, UINT32 textLength, const DWRITE_LINE_BREAKPOINT* lineBreakpoints) override final {
		return E_NOTIMPL;
	}

	STDMETHODIMP SetBidiLevel(UINT32 textPosition, UINT32 textLength, UINT8 explicitLevel, UINT8 resolvedLevel) override final {
		return E_NOTIMPL;
	}

	STDMETHODIMP SetNumberSubstitution(UINT32 textPosition, UINT32 textLength, IDWriteNumberSubstitution* numberSubstitution) override final {
		return E_NOTIMPL;
	}

	std::map<UINT32, DWRITE_SCRIPT_ANALYSIS> scriptAnalyses;
};

int main() {
	Microsoft::WRL::ComPtr<IDWriteFactory> factory;
	HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), &factory);
	assert(SUCCEEDED(hr));

	Microsoft::WRL::ComPtr<IDWriteFontCollection> fontCollection;
	hr = factory->GetSystemFontCollection(&fontCollection, FALSE);
	assert(SUCCEEDED(hr));
	UINT32 familyIndex;
	BOOL familyExists;
	hr = fontCollection->FindFamilyName(L"Arial", &familyIndex, &familyExists);
	assert(SUCCEEDED(hr));
	assert(familyExists == TRUE);
	Microsoft::WRL::ComPtr<IDWriteFontFamily> fontFamily;
	hr = fontCollection->GetFontFamily(familyIndex, &fontFamily);
	assert(SUCCEEDED(hr));
	Microsoft::WRL::ComPtr<IDWriteFont> font;
	hr = fontFamily->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font);
	assert(SUCCEEDED(hr));
	Microsoft::WRL::ComPtr<IDWriteFontFace> fontFace;
	hr = font->CreateFontFace(&fontFace);
	assert(SUCCEEDED(hr));
	FLOAT fontSize(100);

	Microsoft::WRL::ComPtr<IDWriteTextAnalyzer> textAnalyzer;
	hr = factory->CreateTextAnalyzer(&textAnalyzer);
	assert(SUCCEEDED(hr));

	WCHAR textString[] = { 0x644, 0x637, 0x641, 0x627, 0x64b };
	UINT32 textLength = sizeof(textString) / sizeof(textString[0]);
	Microsoft::WRL::ComPtr<TextAnalysisSource> source(Microsoft::WRL::Make<TextAnalysisSource>(textString, textLength));
	Microsoft::WRL::ComPtr<TextAnalysisSink> sink(Microsoft::WRL::Make<TextAnalysisSink>());
	hr = textAnalyzer->AnalyzeScript(source.Get(), 0, textLength, sink.Get());
	assert(SUCCEEDED(hr));
	const std::map<UINT32, DWRITE_SCRIPT_ANALYSIS>& scriptAnalyses(sink->getScriptAnalyses());
	unsigned runCount = 0;
	for (auto scriptAnalysisIterator(scriptAnalyses.begin()); scriptAnalysisIterator != scriptAnalyses.end(); ++scriptAnalysisIterator) {
		auto scriptAnalysisIteratorNext(scriptAnalysisIterator);
		++scriptAnalysisIteratorNext;
		UINT32 position(scriptAnalysisIterator->first);
		UINT32 length((scriptAnalysisIteratorNext == scriptAnalyses.end() ? textLength : scriptAnalysisIteratorNext->first) - scriptAnalysisIterator->first);
		const DWRITE_SCRIPT_ANALYSIS& scriptAnalysis(scriptAnalysisIterator->second);
		std::cout << "Run " << runCount << " at string position " << position << " with string length: " << length << "." << std::endl;
		BOOL isSideways(FALSE);
		BOOL isRightToLeft(TRUE);
		const WCHAR* localeName(nullptr);
		IDWriteNumberSubstitution* numberSubstitution(nullptr);
		std::vector<UINT16> clusterMap(length);
		std::vector<DWRITE_SHAPING_TEXT_PROPERTIES> textProps(length);
		std::vector<UINT16> glyphs;
		std::vector<DWRITE_SHAPING_GLYPH_PROPERTIES> glyphProps;
		UINT32 actualGlyphCount;
		hr = HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
		for (UINT32 maxGlyphCount(2); hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER); maxGlyphCount *= 2) {
			glyphs = std::vector<UINT16>(maxGlyphCount);
			glyphProps = std::vector<DWRITE_SHAPING_GLYPH_PROPERTIES>(maxGlyphCount);
			hr = textAnalyzer->GetGlyphs(textString + position, length, fontFace.Get(), isSideways, isRightToLeft, &scriptAnalysis, localeName, numberSubstitution, nullptr, nullptr, 0, maxGlyphCount, clusterMap.data(), textProps.data(), glyphs.data(), glyphProps.data(), &actualGlyphCount);
			maxGlyphCount *= 2;
		}
		assert(SUCCEEDED(hr));
		std::vector<FLOAT> advances(actualGlyphCount);
		std::vector<DWRITE_GLYPH_OFFSET> offsets(actualGlyphCount);
		hr = textAnalyzer->GetGlyphPlacements(textString + position, clusterMap.data(), textProps.data(), length, glyphs.data(), glyphProps.data(), actualGlyphCount, fontFace.Get(), fontSize, isSideways, isRightToLeft, &scriptAnalysis, localeName, nullptr, nullptr, 0, advances.data(), offsets.data());
		assert(SUCCEEDED(hr));
		for (UINT32 glyphIndex = 0; glyphIndex < actualGlyphCount; ++glyphIndex) {
			std::cout << "Glyph " << glyphs[glyphIndex] << " with advance " << advances[glyphIndex] << " and offset (" << offsets[glyphIndex].advanceOffset << ", " << offsets[glyphIndex].ascenderOffset << ")" << std::endl;
		}
		++runCount;
	}
    return 0;
}

