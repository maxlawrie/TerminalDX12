#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <functional>
#include "ScreenBuffer.h"

namespace TerminalDX12::Terminal {

// Saved cursor state for DECSC/DECRC
struct SavedCursorState {
    int x = 0;
    int y = 0;
    CellAttributes attr;
    bool originMode = false;
    bool autoWrap = true;
    bool valid = false;
};

// OSC 52 clipboard security policy
enum class Osc52Policy {
    Disabled,       // OSC 52 completely disabled
    ReadOnly,       // Only allow clipboard queries (read)
    WriteOnly,      // Only allow clipboard writes (safer default)
    ReadWrite       // Allow both read and write
};

// VT100/ANSI escape sequence parser
class VTStateMachine {
public:
    VTStateMachine(ScreenBuffer* screenBuffer);
    ~VTStateMachine();

    // Process input data
    void ProcessInput(const char* data, size_t size);

    // Response callback for device queries (CPR, DA, etc.)
    void SetResponseCallback(std::function<void(const std::string&)> callback) {
        m_responseCallback = callback;
    }

    // Clipboard callbacks for OSC 52
    void SetClipboardReadCallback(std::function<std::string()> callback) {
        m_clipboardReadCallback = callback;
    }
    void SetClipboardWriteCallback(std::function<void(const std::string&)> callback) {
        m_clipboardWriteCallback = callback;
    }

    // OSC 52 security policy (default: WriteOnly for safety)
    void SetOsc52Policy(Osc52Policy policy) { m_osc52Policy = policy; }
    Osc52Policy GetOsc52Policy() const { return m_osc52Policy; }

    // Cursor style enum
    enum class CursorStyle {
        BlinkingBlock = 0,    // Default
        BlinkingBlock1 = 1,   // Blinking block
        SteadyBlock = 2,      // Steady block
        BlinkingUnderline = 3,
        SteadyUnderline = 4,
        BlinkingBar = 5,
        SteadyBar = 6
    };

    // Mouse mode enum
    enum class MouseMode {
        None,           // No mouse reporting
        X10,            // Mode 1000 - Button press only
        Normal,         // Mode 1002 - Button press/release + motion while pressed
        All,            // Mode 1003 - All motion
    };

    // Mode accessors
    bool IsApplicationCursorKeysEnabled() const { return m_applicationCursorKeys; }
    bool IsAutoWrapEnabled() const { return m_autoWrap; }
    bool IsBracketedPasteEnabled() const { return m_bracketedPaste; }
    bool IsKeypadApplicationModeEnabled() const { return m_keypadApplicationMode; }
    bool IsCursorBlinkEnabled() const { return m_cursorBlink; }
    bool IsOriginModeEnabled() const { return m_originMode; }
    bool IsInsertModeEnabled() const { return m_insertMode; }
    bool IsLineFeedNewLineModeEnabled() const { return m_lineFeedNewLineMode; }
    CursorStyle GetCursorStyle() const { return m_cursorStyle; }

    // Theme color accessors (OSC 10/11)
    void GetThemeForeground(uint8_t& r, uint8_t& g, uint8_t& b) const { r = m_themeFgR; g = m_themeFgG; b = m_themeFgB; }
    void GetThemeBackground(uint8_t& r, uint8_t& g, uint8_t& b) const { r = m_themeBgR; g = m_themeBgG; b = m_themeBgB; }
    bool HasThemeForeground() const { return m_hasThemeFg; }
    bool HasThemeBackground() const { return m_hasThemeBg; }

    // Mouse mode accessors
    MouseMode GetMouseMode() const { return m_mouseMode; }
    bool IsMouseReportingEnabled() const { return m_mouseMode != MouseMode::None; }
    bool IsSGRMouseModeEnabled() const { return m_sgrMouseMode; }

private:
    enum class State {
        Ground,          // Normal text
        Escape,          // ESC received
        CSI_Entry,       // ESC [ received (Control Sequence Introducer)
        CSI_Param,       // Reading CSI parameters
        CSI_Intermediate,// CSI intermediate bytes
        OSC_String,      // Operating System Command string
    };

    void ProcessCharacter(char ch);
    void HandleEscapeSequence(char ch);
    void HandleCSI();
    void ExecuteCharacter(char ch);
    void Clear();

    // CSI handlers
    void HandleCursorPosition();      // CUP - ESC[#;#H
    void HandleCursorUp();           // CUU - ESC[#A
    void HandleCursorDown();         // CUD - ESC[#B
    void HandleCursorForward();      // CUF - ESC[#C
    void HandleCursorBack();         // CUB - ESC[#D
    void HandleCursorNextLine();     // CNL - ESC[#E
    void HandleCursorPreviousLine(); // CPL - ESC[#F
    void HandleCursorHorizontalAbsolute();  // CHA - ESC[#G
    void HandleCursorVerticalAbsolute();    // VPA - ESC[#d
    void HandleEraseInDisplay();     // ED - ESC[#J
    void HandleEraseInLine();        // EL - ESC[#K
    void HandleEraseCharacter();     // ECH - ESC[#X
    void HandleDeleteCharacter();    // DCH - ESC[#P
    void HandleInsertCharacter();    // ICH - ESC[#@
    void HandleInsertLine();         // IL - ESC[#L
    void HandleDeleteLine();         // DL - ESC[#M
    void HandleSGR();                // SGR - ESC[#m (colors/attributes)
    void HandleDeviceAttributes();   // DA - ESC[#c
    void HandleMode();               // Set/Reset mode
    void HandleSetScrollingRegion(); // DECSTBM - ESC[#;#r
    void HandleDeviceStatusReport(); // DSR - ESC[#n
    void HandleOSC();                // OSC - ESC]#;...BEL
    void HandleOSC133(const std::string& param);  // OSC 133 shell integration
    void HandleOSC8(const std::string& param);    // OSC 8 - hyperlinks
    void HandleOSC10(const std::string& param);   // OSC 10 - foreground color
    void HandleOSC11(const std::string& param);   // OSC 11 - background color
    void HandleOSC52(const std::string& param);   // OSC 52 - clipboard access
    void HandleOSC4(const std::string& param);    // OSC 4 - set color palette
    bool ParseOSCColor(const std::string& colorStr, uint8_t& r, uint8_t& g, uint8_t& b);
    void HandleCursorSave();         // CSI s - Save cursor position
    void HandleCursorRestore();      // CSI u - Restore cursor position
    void HandleScrollUp();           // CSI n S - Scroll up
    void HandleScrollDown();         // CSI n T - Scroll down
    void HandleTabClear();           // CSI n g - Tab clear (TBC)
    void HandleCursorStyle();        // CSI n SP q - Set cursor style (DECSCUSR)
    void HandleXtversion();          // CSI > q - Terminal version query
    void HandleKittyKeyboard();      // CSI > u / < u / ? u - Kitty keyboard protocol
    void HandleDECRQM();             // CSI ? Ps $ p - Request mode
    void HandleWindowManipulation(); // CSI n t - Window manipulation (XTWINOPS)

    // Utility
    int GetParam(size_t index, int defaultValue = 0) const;
    void ResetState();
    void SendResponse(const std::string& response);

    ScreenBuffer* m_screenBuffer;
    State m_state;

    char m_intermediateChar;
    char m_finalChar;
    std::vector<int> m_params;
    std::string m_paramBuffer;
    std::string m_oscBuffer;         // Buffer for OSC sequences

    // Response callback
    std::function<void(const std::string&)> m_responseCallback;

    // Clipboard callbacks (OSC 52)
    std::function<std::string()> m_clipboardReadCallback;
    std::function<void(const std::string&)> m_clipboardWriteCallback;

    // Saved cursor states
    SavedCursorState m_savedCursor;      // For DECSC/DECRC (ESC 7 / ESC 8)
    SavedCursorState m_savedCursorCSI;   // For CSI s / CSI u
    SavedCursorState m_savedCursor1049;  // For mode 1049 (alt buffer switch)

    // Terminal modes
    bool m_applicationCursorKeys = false;  // DECCKM (mode 1)
    bool m_originMode = false;             // DECOM (mode 6) - cursor relative to scroll region
    bool m_autoWrap = true;                // DECAWM (mode 7)
    bool m_bracketedPaste = false;         // Mode 2004
    bool m_keypadApplicationMode = false;  // DECKPAM/DECKPNM
    bool m_cursorBlink = true;             // Mode 12 - Cursor blink (default on)
    CursorStyle m_cursorStyle = CursorStyle::BlinkingBlock;  // DECSCUSR cursor style

    // ANSI modes (non-private)
    bool m_insertMode = false;             // IRM (mode 4) - insert vs replace
    bool m_lineFeedNewLineMode = false;    // LNM (mode 20) - LF implies CR

    // Theme colors (OSC 10/11)
    uint8_t m_themeFgR = 204, m_themeFgG = 204, m_themeFgB = 204;  // Default light gray
    uint8_t m_themeBgR = 12, m_themeBgG = 12, m_themeBgB = 12;     // Default near black
    bool m_hasThemeFg = false;  // True if OSC 10 was used to set foreground
    bool m_hasThemeBg = false;  // True if OSC 11 was used to set background
    
    // Mouse modes
    MouseMode m_mouseMode = MouseMode::None;  // Current mouse tracking mode
    bool m_sgrMouseMode = false;              // Mode 1006 - SGR extended format
    bool m_focusReporting = false;            // Mode 1004 - Report focus in/out
    bool m_synchronizedOutput = false;        // Mode 2026 - Buffered output

    // OSC 52 clipboard security
    Osc52Policy m_osc52Policy = Osc52Policy::WriteOnly;  // Default: safer write-only

    // UTF-8 decoding state
    uint8_t m_utf8Buffer[4];     // Buffer for UTF-8 multi-byte sequences
    int m_utf8BytesNeeded = 0;   // Number of continuation bytes expected
    int m_utf8BytesReceived = 0; // Number of continuation bytes received
};

} // namespace TerminalDX12::Terminal
