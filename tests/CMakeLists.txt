# Unit tests

find_package(GTest CONFIG REQUIRED)

# Unit test sources
set(TEST_SOURCES
    unit/test_vt_parser.cpp
    unit/test_screen_buffer.cpp
)

# Create test executable
add_executable(TerminalDX12Tests ${TEST_SOURCES})

target_include_directories(TerminalDX12Tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(TerminalDX12Tests PRIVATE
    GTest::gtest
    GTest::gtest_main
)

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(TerminalDX12Tests)
