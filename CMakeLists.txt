cmake_minimum_required(VERSION 3.16)
project(qo100 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra)

# Hardware-free DSP engine (no external dependencies yet).
add_library(qo100_engine
    engine/ssb.cpp
)
target_include_directories(qo100_engine PUBLIC engine)

enable_testing()
add_executable(test_ssb tests/test_ssb.cpp)
target_link_libraries(test_ssb PRIVATE qo100_engine)
add_test(NAME ssb_roundtrip COMMAND test_ssb)
