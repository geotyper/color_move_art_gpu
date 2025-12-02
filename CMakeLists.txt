cmake_minimum_required(VERSION 3.22)
project(color_move_art LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets ShaderTools)

# Ensure shader baking includes compute-capable GLSL profiles.
set(QT6_SHADER_BAKER_OPTIONS "--glsl 310es,430 --hlsl 50 --msl 12")

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_executable(color_move_art
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/app_window.h
    ${SRC_DIR}/app_window.cpp
    ${SRC_DIR}/mpm_renderer.h
    ${SRC_DIR}/mpm_renderer.cpp
)

target_include_directories(color_move_art PRIVATE ${SRC_DIR})

set(RHI_SHADERS
    shaders/splat.vert
    shaders/splat.frag
    shaders/blur.frag
    shaders/blur.comp
    shaders/fullscreen.vert
    shaders/fullscreen.frag
)

qt_add_shaders(color_move_art "rhi_shaders"
    GLSL "310es,430"
    HLSL 50
    MSL 12
    PREFIX "/shaders"
    FILES ${RHI_SHADERS}
)

target_link_libraries(color_move_art PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::GuiPrivate
)

if (WIN32)
    set_property(TARGET color_move_art PROPERTY WIN32_EXECUTABLE TRUE)
endif()
