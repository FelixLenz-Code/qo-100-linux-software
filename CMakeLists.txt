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
    engine/rx.cpp
    engine/iqfile.cpp
    engine/fft.cpp
    engine/spectrum.cpp
    engine/wavfile.cpp
)
target_include_directories(qo100_engine PUBLIC engine)

# Command-line front end (no audio/GUI dependencies).
add_executable(qo100_cli apps/qo100_cli.cpp)
target_link_libraries(qo100_cli PRIVATE qo100_engine)

enable_testing()

add_executable(test_ssb tests/test_ssb.cpp)
target_link_libraries(test_ssb PRIVATE qo100_engine)
add_test(NAME ssb_roundtrip COMMAND test_ssb)

add_executable(test_rx tests/test_rx.cpp)
target_link_libraries(test_rx PRIVATE qo100_engine)
add_test(NAME rx_chain COMMAND test_rx)

add_executable(test_backend tests/test_backend.cpp)
target_link_libraries(test_backend PRIVATE qo100_engine)
add_test(NAME dsp_backend COMMAND test_backend)
