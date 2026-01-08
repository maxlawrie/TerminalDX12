# Unit tests

find_package(GTest CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

# Create a testable static library from terminal components
# This allows unit testing without linking the full application
find_package(nlohmann_json CONFIG REQUIRED)

add_library(TerminalCore STATIC
    ${CMAKE_SOURCE_DIR}/src/core/Config.cpp
    ${CMAKE_SOURCE_DIR}/src/terminal/ScreenBuffer.cpp
    ${CMAKE_SOURCE_DIR}/src/terminal/VTStateMachine.cpp
    ${CMAKE_SOURCE_DIR}/src/terminal/VTStateMachineCSI.cpp
    ${CMAKE_SOURCE_DIR}/src/terminal/VTStateMachineSGR.cpp
    ${CMAKE_SOURCE_DIR}/src/terminal/VTStateMachineOSC.cpp
    ${CMAKE_SOURCE_DIR}/src/terminal/VTStateMachineModes.cpp
)

target_include_directories(TerminalCore PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(TerminalCore PRIVATE
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)

# Compiler settings for the library
if(MSVC)
    target_compile_options(TerminalCore PRIVATE /W4)
else()
    target_compile_options(TerminalCore PRIVATE -Wall -Wextra)
endif()

# Unit test sources
set(TEST_SOURCES
    unit/test_config.cpp
    unit/test_vt_parser.cpp
    unit/test_screen_buffer.cpp
    unit/test_unicode.cpp
    unit/test_performance.cpp
)

# Create test executable
add_executable(TerminalDX12Tests ${TEST_SOURCES})

target_include_directories(TerminalDX12Tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(TerminalDX12Tests PRIVATE
    TerminalCore
    GTest::gtest
    GTest::gtest_main
)

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(TerminalDX12Tests)

# ============================================================================
# Contract Tests (require ConPTY - Windows 10 1809+)
# ============================================================================

# Create ConPTY library for contract testing
add_library(ConPtyLib STATIC
    ${CMAKE_SOURCE_DIR}/src/pty/ConPtySession.cpp
)

target_include_directories(ConPtyLib PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(ConPtyLib PRIVATE
    spdlog::spdlog
)

# Create UI library for contract testing (requires ConPTY)
add_library(UILib STATIC
    ${CMAKE_SOURCE_DIR}/src/ui/TabManager.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/Tab.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/Pane.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/FontEnumerator.cpp
)

target_include_directories(UILib PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(UILib PRIVATE
    ConPtyLib
    TerminalCore
    spdlog::spdlog
    dwrite.lib
)

# Contract test sources
set(CONTRACT_TEST_SOURCES
    unit/contract/test_conpty_contract.cpp
    unit/contract/test_tab_manager.cpp
    unit/contract/test_tab.cpp
    unit/test_font_enumerator.cpp
)

# Create contract test executable
add_executable(TerminalDX12ContractTests ${CONTRACT_TEST_SOURCES})

target_include_directories(TerminalDX12ContractTests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(TerminalDX12ContractTests PRIVATE
    UILib
    ConPtyLib
    TerminalCore
    GTest::gtest
    GTest::gtest_main
    spdlog::spdlog
)

# Register contract tests with CTest
gtest_discover_tests(TerminalDX12ContractTests)
