cmake_minimum_required(VERSION 3.20)
project(window_grid LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Install raylib separately, or point CMake at a local raylib checkout with
# -Draylib_DIR=<raylib-build>/src. A plain include/library installation also
# works through RAYLIB_DIR.
set(RAYLIB_DIR "" CACHE PATH "raylib installation directory")
find_package(raylib CONFIG QUIET)
if(NOT TARGET raylib)
    find_path(RAYLIB_INCLUDE_DIR raylib.h
        HINTS "${RAYLIB_DIR}" "${RAYLIB_DIR}/include")
    find_library(RAYLIB_LIBRARY NAMES raylib
        HINTS "${RAYLIB_DIR}" "${RAYLIB_DIR}/lib" "${RAYLIB_DIR}/lib/Release")
    if(RAYLIB_INCLUDE_DIR AND RAYLIB_LIBRARY)
        add_library(raylib UNKNOWN IMPORTED)
        set_target_properties(raylib PROPERTIES
            IMPORTED_LOCATION "${RAYLIB_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${RAYLIB_INCLUDE_DIR}")
    else()
        message(FATAL_ERROR "raylib was not found. Set raylib_DIR or RAYLIB_DIR.")
    endif()
endif()

set(RAYGUI_DIR "" CACHE PATH "Directory containing raygui.h")
find_path(RAYGUI_INCLUDE_DIR raygui.h
    HINTS "${RAYGUI_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/external")
if(NOT RAYGUI_INCLUDE_DIR)
    message(FATAL_ERROR "raygui.h was not found. Set RAYGUI_DIR to its directory.")
endif()

add_executable(window_grid WIN32
    main.cpp
    window_grid.hpp
    gui.hpp)

# raylib uses a normal C++ main(). Keep the Windows GUI subsystem without
# requiring a WinMain implementation, so Release does not open a console.
if(MSVC)
    target_link_options(window_grid PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
endif()

target_include_directories(window_grid PRIVATE "${RAYGUI_INCLUDE_DIR}")
target_link_libraries(window_grid PRIVATE raylib)

if(MSVC)
    target_compile_options(window_grid PRIVATE /W4 /permissive-)
else()
    target_compile_options(window_grid PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    target_link_libraries(window_grid PRIVATE user32 psapi)
endif()
