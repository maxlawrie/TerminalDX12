# Research: Settings Dialog Implementation

**Feature**: 006-settings-dialog
**Date**: 2025-12-28

## 1. Tab Control Approach

**Decision**: Use Win32 Property Sheets (`PropertySheet` API)

**Rationale**:
- Standard Windows approach for settings dialogs
- Automatic handling of tab switching, Apply/OK/Cancel semantics
- Built-in keyboard navigation (accessibility)
- Less code than manual tab control management

**Alternatives considered**:
- Manual Tab Control: More control but requires managing page visibility manually
- Custom DX12 UI: Would violate constitution (GPU rendering for UI chrome)

**Implementation**:
```cpp
PROPSHEETPAGE pages[3] = {};
pages[0].pszTemplate = MAKEINTRESOURCE(IDD_GENERAL_PAGE);
pages[1].pszTemplate = MAKEINTRESOURCE(IDD_APPEARANCE_PAGE);
pages[2].pszTemplate = MAKEINTRESOURCE(IDD_TERMINAL_PAGE);

PROPSHEETHEADER psh = {};
psh.dwFlags = PSH_PROPSHEETPAGE;
psh.nPages = 3;
psh.ppsp = pages;
PropertySheet(&psh);
```

## 2. Font Enumeration (Monospace Only)

**Decision**: DirectWrite with glyph width comparison

**Rationale**:
- DirectWrite is already linked (via FreeType/harfbuzz chain)
- Glyph width comparison (`'i'` vs `'M'`) is definitive test for monospace
- Works with all Unicode font names
- Modern Windows 10+ API

**Alternatives considered**:
- GDI `EnumFontFamilies`: Legacy, less reliable monospace detection
- Hardcoded list: Would miss user-installed fonts

**Implementation**:
```cpp
bool IsMonospaceFont(IDWriteFontFace* fontFace) {
    UINT16 glyphIndices[2];
    UINT32 codePoints[2] = { L'i', L'M' };
    fontFace->GetGlyphIndices(codePoints, 2, glyphIndices);

    DWRITE_GLYPH_METRICS metrics[2];
    fontFace->GetDesignGlyphMetrics(glyphIndices, 2, metrics, FALSE);

    return metrics[0].advanceWidth == metrics[1].advanceWidth;
}
```

## 3. Color Picker

**Decision**: Windows `ChooseColorW` common dialog

**Rationale**:
- Native Windows look and feel
- Supports custom color palette persistence
- No additional dependencies
- Users are familiar with the interface

**Alternatives considered**:
- Custom color picker: Significant work, no benefit
- Third-party library: Unnecessary dependency

**Implementation**:
```cpp
COLORREF customColors[16] = {};
CHOOSECOLORW cc = { sizeof(cc) };
cc.hwndOwner = hwnd;
cc.lpCustColors = customColors;
cc.rgbResult = RGB(color.r, color.g, color.b);
cc.Flags = CC_ANYCOLOR | CC_FULLOPEN | CC_RGBINIT;
if (ChooseColorW(&cc)) {
    // Use cc.rgbResult
}
```

## 4. Folder Selection

**Decision**: `IFileDialog` with `FOS_PICKFOLDERS` flag

**Rationale**:
- Modern Windows Vista+ interface
- Breadcrumb navigation, address bar, favorites
- Much better UX than legacy `SHBrowseForFolder`
- Already using COM for DirectX

**Alternatives considered**:
- `SHBrowseForFolder`: Legacy, poor UX (no breadcrumbs, no address bar)

**Implementation**:
```cpp
ComPtr<IFileDialog> pfd;
CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
DWORD options;
pfd->GetOptions(&options);
pfd->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
pfd->Show(hwndOwner);
```

## 5. Live Settings Preview

**Decision**: Debounced updates with temporary config copy

**Rationale**:
- 150ms debounce prevents excessive updates during typing
- Temporary config copy allows clean Cancel rollback
- Minimal impact on render thread (atomic config pointer swap)

**Alternatives considered**:
- Immediate updates: Too jarring, performance impact
- No preview: Poor UX, users expect live feedback
- Observer pattern: Over-engineered for this use case

**Implementation**:
```cpp
// On control change
KillTimer(hwnd, PREVIEW_TIMER_ID);
SetTimer(hwnd, PREVIEW_TIMER_ID, 150, nullptr);

// On timer
void ApplyPreview() {
    ReadSettingsToConfig(hwnd, m_previewConfig);
    m_application->SetPreviewConfig(&m_previewConfig);
}

// On Cancel
void OnCancel() {
    m_application->SetPreviewConfig(nullptr);  // Revert to saved
}
```

## Summary

| Component | Technology | Key Benefit |
|-----------|------------|-------------|
| Tabs | Property Sheets | Native Windows semantics |
| Fonts | DirectWrite | Accurate monospace detection |
| Colors | ChooseColorW | Familiar Windows UI |
| Folders | IFileDialog | Modern breadcrumb navigation |
| Preview | Debounced timer | Responsive but not jarring |
