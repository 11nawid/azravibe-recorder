# Azravibe Recorder — cross-platform build.
# One source tree, three platforms: Linux (X11), Windows (Win32/DXGI), macOS (Cocoa).
#
#   cmake -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
#
# The GitHub Actions release workflow (.github/workflows/release.yml) uses this
# file to produce the .deb / .exe installer / .dmg for tagged releases.
cmake_minimum_required(VERSION 3.16)
project(azravibe-recorder VERSION 1.2.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC OFF)

if(APPLE)
    enable_language(OBJCXX)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(AZRAVIBE_SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/RecorderController.cpp
    src/TitleBar.cpp
    src/PreviewWidget.cpp
    src/WaveformWidget.cpp
    src/RecordButton.cpp
    src/PauseButton.cpp
    src/media/Encoder.cpp
    src/capture/Capture.cpp
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND AZRAVIBE_SOURCES
        src/capture/X11Capture.cpp
        src/capture/X11WindowEnumerator.cpp
        src/audio/AudioDevicesPulse.cpp
        src/audio/AudioCapturerPulse.cpp)

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PULSE REQUIRED libpulse libpulse-simple IMPORTED_TARGET)
    pkg_check_modules(XLIBS REQUIRED x11 xext xrandr xfixes IMPORTED_TARGET)

    # FFmpeg via pkg-config (distro ffmpeg dev packages)
    pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil libswscale libswresample IMPORTED_TARGET)

    set(AZRAVIBE_LINK_LIBS PkgConfig::PULSE PkgConfig::XLIBS PkgConfig::FFMPEG)

elseif(WIN32)
    list(APPEND AZRAVIBE_SOURCES
        src/capture/WinCapture.cpp
        src/capture/WinWindowEnumerator.cpp
        src/audio/AudioDevicesWin.cpp
        src/audio/AudioCapturerWin.cpp)

    # FFmpeg prebuilt dev packages — set -DFFMPEG_DIR=<path to ffmpeg root>
    set(FFMPEG_DIR "$ENV{FFMPEG_DIR}" CACHE PATH "FFmpeg root (include/ + lib/)")
    if(NOT FFMPEG_DIR)
        message(FATAL_ERROR "FFMPEG_DIR is not set. Point it at a prebuilt FFmpeg with headers and libs.")
    endif()
    find_path(FFMPEG_INCLUDE_DIR libavcodec/avcodec.h PATHS "${FFMPEG_DIR}/include" REQUIRED)
    foreach(lib avcodec avformat avutil swscale swresample)
        find_library(FFMPEG_${lib}_LIB NAMES ${lib} PATHS "${FFMPEG_DIR}/lib" REQUIRED)
        set(FFMPEG_LIBS ${FFMPEG_LIBS} ${FFMPEG_${lib}_LIB})
    endforeach()

    # MSVC-specific flags for WRL/DXGI
    if(MSVC)
        add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
    endif()

    set(AZRAVIBE_LINK_LIBS d3d11 dxgi dwmapi avrt ole32 uuid ${FFMPEG_LIBS})

elseif(APPLE)
    list(APPEND AZRAVIBE_SOURCES
        src/capture/MacCapture.mm
        src/capture/MacWindowEnumerator.cpp
        src/audio/AudioDevicesMac.cpp
        src/audio/AudioCapturerMac.mm)

    # FFmpeg via pkg-config (Homebrew ffmpeg)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil libswscale libswresample IMPORTED_TARGET)

    set(AZRAVIBE_LINK_LIBS)   # frameworks + PkgConfig are linked explicitly below

endif()

add_executable(azravibe-recorder ${AZRAVIBE_SOURCES})

target_include_directories(azravibe-recorder PRIVATE src ${FFMPEG_INCLUDE_DIR})
target_compile_definitions(azravibe-recorder PRIVATE QT_CORE_LIB QT_GUI_LIB QT_WIDGETS_LIB NDEBUG)

if(APPLE)
    target_link_libraries(azravibe-recorder PRIVATE Qt6::Widgets PkgConfig::FFMPEG
        "-framework CoreAudio" "-framework AudioToolbox" "-framework CoreGraphics"
        "-framework AppKit" "-framework AVFoundation" "-framework QuartzCore")
else()
    target_link_libraries(azravibe-recorder PRIVATE Qt6::Widgets ${AZRAVIBE_LINK_LIBS})
endif()

# Desktop integration
if(APPLE)
    set_target_properties(azravibe-recorder PROPERTIES MACOSX_BUNDLE TRUE)
elseif(NOT WIN32)
    install(TARGETS azravibe-recorder RUNTIME DESTINATION bin)
    install(FILES assets/azravibe-recorder.svg DESTINATION share/icons/hicolor/scalable/apps)
    install(FILES assets/azravibe-recorder.desktop DESTINATION share/applications)
endif()
