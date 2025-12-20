# Unit tests

find_package(GTest CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

# Create a testable static library from terminal components
# This allows unit testing without linking the full application
add_library(TerminalCore STATIC
    ${CMAKE_SOURCE_DIR}/src/terminal/ScreenBuffer.cpp
    ${CMAKE_SOURCE_DIR}/src/terminal/VTStateMachine.cpp
)

target_include_directories(TerminalCore PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(TerminalCore PRIVATE
    spdlog::spdlog
)

# Compiler settings for the library
if(MSVC)
    target_compile_options(TerminalCore PRIVATE /W4)
else()
    target_compile_options(TerminalCore PRIVATE -Wall -Wextra)
endif()

# Unit test sources
set(TEST_SOURCES
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
