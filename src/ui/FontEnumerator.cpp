#include "ui/FontEnumerator.h"
#include <spdlog/spdlog.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <algorithm>

#pragma comment(lib, "dwrite.lib")

using Microsoft::WRL::ComPtr;

namespace TerminalDX12::UI {

class FontEnumerator::Impl {
public:
    Impl() {
        HRESULT hr = DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(m_factory.GetAddressOf())
        );
        if (FAILED(hr)) {
            spdlog::error("Failed to create DirectWrite factory: 0x{:08X}", hr);
        }
    }

    bool IsMonospaceFont(IDWriteFontFamily* fontFamily) {
        ComPtr<IDWriteFont> font;
        HRESULT hr = fontFamily->GetFirstMatchingFont(
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            &font
        );
        if (FAILED(hr)) return false;

        ComPtr<IDWriteFontFace> fontFace;
        hr = font->CreateFontFace(&fontFace);
        if (FAILED(hr)) return false;

        // Compare glyph widths of 'i' and 'M' to detect monospace
        UINT32 codePoints[2] = { L'i', L'M' };
        UINT16 glyphIndices[2] = { 0 };
        hr = fontFace->GetGlyphIndices(codePoints, 2, glyphIndices);
        if (FAILED(hr)) return false;

        // If either glyph is missing, can't determine monospace status
        if (glyphIndices[0] == 0 || glyphIndices[1] == 0) return false;

        DWRITE_GLYPH_METRICS metrics[2] = {};
        hr = fontFace->GetDesignGlyphMetrics(glyphIndices, 2, metrics, FALSE);
        if (FAILED(hr)) return false;

        // Monospace fonts have identical advance widths for all glyphs
        return metrics[0].advanceWidth == metrics[1].advanceWidth;
    }

    std::wstring GetFamilyName(IDWriteFontFamily* fontFamily) {
        ComPtr<IDWriteLocalizedStrings> names;
        HRESULT hr = fontFamily->GetFamilyNames(&names);
        if (FAILED(hr)) return L"";

        UINT32 index = 0;
        BOOL exists = FALSE;
        hr = names->FindLocaleName(L"en-us", &index, &exists);
        if (FAILED(hr) || !exists) {
            index = 0; // Use first available
        }

        UINT32 length = 0;
        hr = names->GetStringLength(index, &length);
        if (FAILED(hr)) return L"";

        std::wstring name(length + 1, L'\0');
        hr = names->GetString(index, &name[0], length + 1);
        if (FAILED(hr)) return L"";

        name.resize(length);
        return name;
    }

    std::vector<FontInfo> EnumerateMonospaceFonts() {
        std::vector<FontInfo> fonts;

        if (!m_factory) {
            spdlog::warn("DirectWrite factory not available, returning empty font list");
            return fonts;
        }

        ComPtr<IDWriteFontCollection> fontCollection;
        HRESULT hr = m_factory->GetSystemFontCollection(&fontCollection);
        if (FAILED(hr)) {
            spdlog::error("Failed to get system font collection: 0x{:08X}", hr);
            return fonts;
        }

        UINT32 familyCount = fontCollection->GetFontFamilyCount();
        spdlog::debug("Enumerating {} font families for monospace detection", familyCount);

        for (UINT32 i = 0; i < familyCount; ++i) {
            ComPtr<IDWriteFontFamily> fontFamily;
            hr = fontCollection->GetFontFamily(i, &fontFamily);
            if (FAILED(hr)) continue;

            std::wstring name = GetFamilyName(fontFamily.Get());
            if (name.empty()) continue;

            bool isMonospace = IsMonospaceFont(fontFamily.Get());
            if (isMonospace) {
                fonts.push_back({name, true});
            }
        }

        // Sort alphabetically
        std::sort(fonts.begin(), fonts.end(),
            [](const FontInfo& a, const FontInfo& b) {
                return a.familyName < b.familyName;
            });

        spdlog::info("Found {} monospace fonts", fonts.size());
        return fonts;
    }

    bool FontExists(const std::wstring& familyName) {
        if (!m_factory) return false;

        ComPtr<IDWriteFontCollection> fontCollection;
        HRESULT hr = m_factory->GetSystemFontCollection(&fontCollection);
        if (FAILED(hr)) return false;

        UINT32 index = 0;
        BOOL exists = FALSE;
        hr = fontCollection->FindFamilyName(familyName.c_str(), &index, &exists);
        return SUCCEEDED(hr) && exists;
    }

    ComPtr<IDWriteFactory> m_factory;
};

FontEnumerator::FontEnumerator() : m_impl(std::make_unique<Impl>()) {}
FontEnumerator::~FontEnumerator() = default;

std::vector<FontInfo> FontEnumerator::EnumerateMonospaceFonts() {
    return m_impl->EnumerateMonospaceFonts();
}

bool FontEnumerator::FontExists(const std::wstring& familyName) {
    return m_impl->FontExists(familyName);
}

std::wstring FontEnumerator::GetFontOrFallback(const std::wstring& requestedFamily) {
    if (m_impl->FontExists(requestedFamily)) {
        return requestedFamily;
    }

    spdlog::warn("Requested font '{}' not found, falling back to {}",
        std::string(requestedFamily.begin(), requestedFamily.end()),
        "Consolas");
    return kDefaultFont;
}

} // namespace TerminalDX12::UI
