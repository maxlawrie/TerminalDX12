#pragma once

#include <string>
#include <functional>
#include <Windows.h>

namespace TerminalDX12::Terminal {
    class ScreenBuffer;
}

namespace TerminalDX12::Pty {
    class ConPtySession;
}

namespace TerminalDX12::UI {

/// @brief Position in the terminal grid
struct SelectionPos {
    int x = 0;
    int y = 0;
};

/// @brief Manages text selection and clipboard operations for the terminal
class SelectionManager {
public:
    SelectionManager() = default;

    /// @brief Clear the current selection
    void ClearSelection();

    /// @brief Check if there is an active selection
    bool HasSelection() const { return m_hasSelection; }

    /// @brief Check if currently dragging to select
    bool IsSelecting() const { return m_selecting; }

    /// @brief Check if rectangle selection mode is active
    bool IsRectangleSelection() const { return m_rectangleSelection; }

    /// @brief Get selection start position
    const SelectionPos& GetSelectionStart() const { return m_selectionStart; }

    /// @brief Get selection end position
    const SelectionPos& GetSelectionEnd() const { return m_selectionEnd; }

    /// @brief Start a new selection at the given position
    /// @param pos The starting cell position
    /// @param rectangleMode Whether to use rectangle selection mode
    /// @param pane The pane the selection is in (optional, for multi-pane support)
    void StartSelection(SelectionPos pos, bool rectangleMode = false, void* pane = nullptr);

    /// @brief Get the pane the current selection belongs to
    void* GetSelectionPane() const { return m_selectionPane; }

    /// @brief Extend the selection to the given position
    /// @param pos The new end position
    void ExtendSelection(SelectionPos pos);

    /// @brief End the current selection drag
    void EndSelection();

    /// @brief Get the selected text from the screen buffer
    /// @param screenBuffer The screen buffer to read from
    /// @return UTF-8 encoded selected text
    std::string GetSelectedText(Terminal::ScreenBuffer* screenBuffer) const;

    /// @brief Copy selection to clipboard
    /// @param screenBuffer The screen buffer to read from
    /// @param hwnd Window handle for clipboard access
    void CopySelectionToClipboard(Terminal::ScreenBuffer* screenBuffer, HWND hwnd);

    /// @brief Paste from clipboard to terminal
    /// @param terminal The terminal to write to
    /// @param hwnd Window handle for clipboard access
    void PasteFromClipboard(Pty::ConPtySession* terminal, HWND hwnd);

    /// @brief Get clipboard text (for OSC 52 read)
    /// @param hwnd Window handle for clipboard access
    /// @return UTF-8 encoded clipboard text
    std::string GetClipboardText(HWND hwnd);

    /// @brief Set clipboard text (for OSC 52 write)
    /// @param text UTF-8 encoded text to set
    /// @param hwnd Window handle for clipboard access
    void SetClipboardText(const std::string& text, HWND hwnd);

    /// @brief Select the word at the given position
    /// @param screenBuffer The screen buffer to read from
    /// @param cellX Column position
    /// @param cellY Row position
    void SelectWord(Terminal::ScreenBuffer* screenBuffer, int cellX, int cellY);

    /// @brief Select the entire line at the given position
    /// @param screenBuffer The screen buffer to read from
    /// @param cellY Row position
    void SelectLine(Terminal::ScreenBuffer* screenBuffer, int cellY);

    /// @brief Check if a character is part of a word
    /// @param ch The character to check
    /// @return true if the character is a word character
    bool IsWordChar(char32_t ch) const;

    /// @brief Show context menu at the given position
    /// @param x Screen X coordinate
    /// @param y Screen Y coordinate
    /// @param hwnd Window handle
    /// @param screenBuffer Screen buffer for select-all operation
    /// @param terminal Terminal for paste operation
    void ShowContextMenu(int x, int y, HWND hwnd, Terminal::ScreenBuffer* screenBuffer,
                         Pty::ConPtySession* terminal);

    /// @brief Set callback for opening settings dialog
    void SetSettingsCallback(std::function<void()> callback) { m_onShowSettings = std::move(callback); }

    /// @brief Set callback for opening pane settings dialog
    void SetPaneSettingsCallback(std::function<void()> callback) { m_onShowPaneSettings = std::move(callback); }

    /// @brief Set callback for opening profile manager dialog
    void SetProfileManagerCallback(std::function<void()> callback) { m_onShowProfileManager = std::move(callback); }

    /// @brief Set callback for splitting pane horizontally
    void SetSplitHorizontalCallback(std::function<void()> callback) { m_onSplitHorizontal = std::move(callback); }

    /// @brief Set callback for splitting pane vertically
    void SetSplitVerticalCallback(std::function<void()> callback) { m_onSplitVertical = std::move(callback); }

    /// @brief Set callback for closing pane
    void SetClosePaneCallback(std::function<void()> callback) { m_onClosePane = std::move(callback); }

    /// @brief Set callback for creating new tab
    void SetNewTabCallback(std::function<void()> callback) { m_onNewTab = std::move(callback); }

    /// @brief Set whether multiple panes exist (for enabling/disabling close option)
    void SetHasMultiplePanes(std::function<bool()> callback) { m_hasMultiplePanes = std::move(callback); }

private:
    std::function<void()> m_onShowSettings;
    std::function<void()> m_onShowPaneSettings;
    std::function<void()> m_onShowProfileManager;
    std::function<void()> m_onSplitHorizontal;
    std::function<void()> m_onSplitVertical;
    std::function<void()> m_onClosePane;
    std::function<void()> m_onNewTab;
    std::function<bool()> m_hasMultiplePanes;
    SelectionPos m_selectionStart{0, 0};
    SelectionPos m_selectionEnd{0, 0};
    bool m_selecting = false;
    bool m_hasSelection = false;
    bool m_rectangleSelection = false;
    void* m_selectionPane = nullptr;  // Track which pane selection is in
};

} // namespace TerminalDX12::UI
