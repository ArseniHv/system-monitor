cmake_minimum_required(VERSION 3.16)
project(syspeek VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Make version available to source code.
add_compile_definitions(SYSPEEK_VERSION="0.1.0")

add_executable(system-monitor
    src/main.cpp
    src/collector.cpp
    src/renderer.cpp
    src/utils.cpp
    src/config.cpp
)

target_include_directories(system-monitor PRIVATE src)
target_compile_options(system-monitor PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(system-monitor PRIVATE pthread)

enable_testing()

add_executable(test_parser
    tests/test_parser.cpp
    src/utils.cpp
    src/config.cpp
)

target_include_directories(test_parser PRIVATE src)
target_compile_options(test_parser PRIVATE -Wall -Wextra)
target_link_libraries(test_parser PRIVATE pthread)

add_test(NAME ParserTests COMMAND test_parser)