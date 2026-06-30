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
    engine/tx.cpp
    engine/iqfile.cpp
    engine/fft.cpp
    engine/spectrum.cpp
    engine/wavfile.cpp
    engine/calib.cpp
)
target_include_directories(qo100_engine PUBLIC engine)

# Command-line front end (no audio/GUI dependencies).
add_executable(qo100_cli apps/qo100_cli.cpp)
target_link_libraries(qo100_cli PRIVATE qo100_engine)

# ---- Optional GUI (Dear ImGui + ImPlot waterfall) ----
option(BUILD_UI "Build the GUI waterfall viewer" ON)
if(BUILD_UI)
    find_package(OpenGL)
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(GLFW glfw3)
    endif()
    set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/extern/imgui)
    set(IMPLOT_DIR ${CMAKE_SOURCE_DIR}/extern/implot)
    if(OpenGL_FOUND AND GLFW_FOUND AND EXISTS ${IMGUI_DIR}/imgui.cpp)
        add_library(qo100_imgui STATIC
            ${IMGUI_DIR}/imgui.cpp
            ${IMGUI_DIR}/imgui_draw.cpp
            ${IMGUI_DIR}/imgui_tables.cpp
            ${IMGUI_DIR}/imgui_widgets.cpp
            ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
            ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
            ${IMPLOT_DIR}/implot.cpp
            ${IMPLOT_DIR}/implot_items.cpp
        )
        target_include_directories(qo100_imgui PUBLIC
            ${IMGUI_DIR} ${IMGUI_DIR}/backends ${IMPLOT_DIR} ${GLFW_INCLUDE_DIRS})
        target_link_libraries(qo100_imgui PUBLIC OpenGL::GL ${GLFW_LIBRARIES} ${CMAKE_DL_LIBS})
        target_link_directories(qo100_imgui PUBLIC ${GLFW_LIBRARY_DIRS})

        add_executable(qo100_ui ui/main.cpp)
        target_link_libraries(qo100_ui PRIVATE qo100_engine qo100_imgui)
        message(STATUS "qo100: building GUI (qo100_ui)")
    else()
        message(STATUS "qo100: GUI skipped (need OpenGL, glfw3 and submodules in extern/)")
    endif()
endif()

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

add_executable(test_tx tests/test_tx.cpp)
target_link_libraries(test_tx PRIVATE qo100_engine)
add_test(NAME tx_loopback COMMAND test_tx)

add_executable(test_calib tests/test_calib.cpp)
target_link_libraries(test_calib PRIVATE qo100_engine)
add_test(NAME beacon_calib COMMAND test_calib)
