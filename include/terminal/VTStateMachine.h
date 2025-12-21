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
    void HandleCursorSave();         // CSI s - Save cursor position
    void HandleCursorRestore();      // CSI u - Restore cursor position
    void HandleScrollUp();           // CSI n S - Scroll up
    void HandleScrollDown();         // CSI n T - Scroll down

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

    // Saved cursor states
    SavedCursorState m_savedCursor;      // For DECSC/DECRC (ESC 7 / ESC 8)
    SavedCursorState m_savedCursorCSI;   // For CSI s / CSI u

    // Terminal modes
    bool m_applicationCursorKeys = false;  // DECCKM (mode 1)
    bool m_autoWrap = true;                // DECAWM (mode 7)
    bool m_bracketedPaste = false;         // Mode 2004
    
    // Mouse modes
    MouseMode m_mouseMode = MouseMode::None;  // Current mouse tracking mode
    bool m_sgrMouseMode = false;              // Mode 1006 - SGR extended format
};

} // namespace TerminalDX12::Terminal
