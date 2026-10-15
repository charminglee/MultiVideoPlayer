cmake_minimum_required(VERSION 3.21)
project(MultiVideoPlayer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Threads REQUIRED)
find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets OpenGLWidgets Xml Svg SvgWidgets)

# mpv 子模块：使用 meson 构建产物（libmpv-2.dll + 导入库）与 mpv/include 头文件
set(MPV_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/mpv" CACHE PATH "mpv submodule directory")

# QWidget-FancyUI 子模块：库不生成独立 target（见其 src/CMakeLists.txt），
# 按官方做法把源码与图标资源直接编入应用。
set(FANCYUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/QWidget-FancyUI/src")
file(GLOB_RECURSE FANCYUI_SOURCES CONFIGURE_DEPENDS
        "${FANCYUI_DIR}/Fancy/*.h"
        "${FANCYUI_DIR}/Fancy/*.cpp"
        "${FANCYUI_DIR}/resource/*.qrc"
)

add_library(mpv SHARED IMPORTED)
set_target_properties(mpv PROPERTIES
    IMPORTED_LOCATION "${MPV_DIR}/build/libmpv-2.dll"
    IMPORTED_IMPLIB "${MPV_DIR}/build/libmpv.dll.a")
target_include_directories(mpv INTERFACE "${MPV_DIR}/include")

qt_add_executable(MultiVideoPlayer WIN32
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/MediaSlider.h
    src/MediaSlider.cpp
    src/MpvController.h
    src/MpvController.cpp
    src/VideoWidget.h
    src/VideoWidget.cpp
)

# FancyUI 源码 + 精简 OpenCV 滤波（BlurCard 用）直接编入，不生成额外的库。
# qrc 内部是相对 resource/ 的路径，走 AUTORCC 时以其所在目录为基准解析。
target_sources(MultiVideoPlayer PRIVATE
        ${FANCYUI_SOURCES}
        "${FANCYUI_DIR}/include/mini_blur/mini_blur.cpp"
        "${FANCYUI_DIR}/include/mini_blur/mini_blur_avx2.cpp"
)

# 该文件全为显式 AVX2 intrinsic 且带运行时门控，仅此编译单元单独开启；
# 全局开启会让可移植入口点也被向量化成 AVX2，老 CPU 上无法运行。
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties("${FANCYUI_DIR}/include/mini_blur/mini_blur_avx2.cpp"
            PROPERTIES COMPILE_OPTIONS "-mavx2")
endif ()

target_include_directories(MultiVideoPlayer PRIVATE
        "${FANCYUI_DIR}/Fancy"
        "${FANCYUI_DIR}/include/magic_enum"
        "${FANCYUI_DIR}/include/mini_blur"
)

target_link_libraries(MultiVideoPlayer PRIVATE
    Qt6::Widgets
    Qt6::OpenGLWidgets
    Qt6::Xml
    Qt6::Svg
    Qt6::SvgWidgets
    Threads::Threads
    mpv
    dwmapi
    user32
    advapi32
)

# 每次编译后自动打包运行库：Qt（含平台插件）+ libmpv + 依赖闭包，
# 使 build/ 下的 exe 可直接双击运行，无需 MSYS2 环境。
find_program(BASH_EXECUTABLE bash REQUIRED)
find_program(WINDEPLOYQT_EXECUTABLE windeployqt6 REQUIRED)

add_custom_command(TARGET MultiVideoPlayer POST_BUILD
    COMMAND "${BASH_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/bundle.sh"
        $<TARGET_FILE:MultiVideoPlayer>
        "${MPV_DIR}/build"
    VERBATIM
)
