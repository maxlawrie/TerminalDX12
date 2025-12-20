#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <functional>

namespace TerminalDX12::Terminal {

class ScreenBuffer;

// VT100/ANSI escape sequence parser
class VTStateMachine {
public:
    VTStateMachine(ScreenBuffer* screenBuffer);
    ~VTStateMachine();

    // Process input data
    void ProcessInput(const char* data, size_t size);

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
    void HandleEraseInDisplay();     // ED - ESC[#J
    void HandleEraseInLine();        // EL - ESC[#K
    void HandleSGR();                // SGR - ESC[#m (colors/attributes)
    void HandleDeviceAttributes();   // DA - ESC[#c
    void HandleMode();               // Set/Reset mode
    void HandleOSC();                // OSC - ESC]#;...BEL (Operating System Command)

    // Utility
    int GetParam(size_t index, int defaultValue = 0) const;
    void ResetState();

    ScreenBuffer* m_screenBuffer;
    State m_state;

    char m_intermediateChar;
    char m_finalChar;
    std::vector<int> m_params;
    std::string m_paramBuffer;
    std::string m_oscBuffer;         // Buffer for OSC sequences
};

} // namespace TerminalDX12::Terminal
