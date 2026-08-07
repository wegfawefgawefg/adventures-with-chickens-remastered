cmake_minimum_required(VERSION 3.21)

project(adventures_with_chickens_remastered VERSION 0.1.0 LANGUAGES C CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(AWC_STRICT "Enable strict compiler warnings" ON)
option(AWC_WARN_AS_ERROR "Treat compiler warnings as errors" ON)
option(AWC_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
option(AWC_BUILD_EXECUTABLE "Build the SDL3 game executable" ON)

function(awc_target_defaults target)
    target_compile_features(${target} PUBLIC cxx_std_20)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
        if(AWC_WARN_AS_ERROR)
            target_compile_options(${target} PRIVATE /WX)
        endif()
        return()
    endif()

    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    if(AWC_STRICT)
        target_compile_options(${target} PRIVATE -Wconversion -Wsign-conversion -Wshadow)
    endif()
    if(AWC_WARN_AS_ERROR)
        target_compile_options(${target} PRIVATE -Werror)
    endif()
    if(AWC_SANITIZERS)
        target_compile_options(${target} PRIVATE
            -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
endfunction()

if(AWC_BUILD_EXECUTABLE)
    if(NOT EMSCRIPTEN)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(FLUIDSYNTH REQUIRED IMPORTED_TARGET fluidsynth)
    endif()

    set(AWC_GUBSY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Splonks/gubsy"
        CACHE PATH "Gubsy source tree")
    set(GUB_MODE consumer CACHE STRING "" FORCE)
    set(GUB_BUILD_SAMPLE OFF CACHE BOOL "" FORCE)
    set(GUB_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(GUB_ENABLE_LUA_MOD_HOST OFF CACHE BOOL "" FORCE)
    set(GUB_ENABLE_STEAM OFF CACHE BOOL "" FORCE)
    set(GUB_FETCH_DEPS ON CACHE BOOL "" FORCE)
    set(GUB_SDL_DEPS fetch CACHE STRING "" FORCE)
    if(EMSCRIPTEN)
        set(GUB_WARN_AS_ERROR OFF CACHE BOOL "" FORCE)
        set(SDLTTF_HARFBUZZ OFF CACHE BOOL "" FORCE)
    endif()
    if(EXISTS "${AWC_GUBSY_DIR}/CMakeLists.txt")
        add_subdirectory("${AWC_GUBSY_DIR}" gubsy)
    else()
        FetchContent_Declare(gubsy
            GIT_REPOSITORY https://github.com/wegfawefgawefg/gubsy.git
            GIT_TAG f95d5644fc254d85d8406a47df23def8f89dc0dd)
        FetchContent_MakeAvailable(gubsy)
        set(AWC_GUBSY_DIR "${gubsy_SOURCE_DIR}")
    endif()
    set(AWC_GUBSY_ASSETS_DIR "${AWC_GUBSY_DIR}/src/assets"
        CACHE STRING "Gubsy asset path exposed to the runtime")

    set(AWC_SOURCES
        src/assets.cpp
        src/controls.cpp
        src/draw.cpp
        src/draw_controls.cpp
        src/gubsy.cpp
        src/inputs.cpp
        src/level.cpp
        src/math.cpp
        src/save.cpp
        src/setup.cpp
        src/step.cpp
        src/steps/actors.cpp
        src/steps/common.cpp
        src/steps/frontend.cpp
        src/steps/movement.cpp
        src/steps/playing.cpp
        src/ui.cpp
    )
    if(EMSCRIPTEN)
        list(APPEND AWC_SOURCES
            src/audio_web.cpp
            src/browser_main.cpp)
    else()
        list(APPEND AWC_SOURCES
            src/audio.cpp
            src/cli.cpp
            src/main.cpp
            src/window.cpp)
    endif()

    add_executable(adventures-with-chickens-remastered ${AWC_SOURCES})
    target_include_directories(adventures-with-chickens-remastered
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${AWC_GUBSY_DIR}"
            "${AWC_GUBSY_DIR}/imgui"
            "${AWC_GUBSY_DIR}/imgui/backends")
    target_link_libraries(adventures-with-chickens-remastered PRIVATE gubsy::engine SDL3::SDL3)
    if(EMSCRIPTEN)
        set_target_properties(adventures-with-chickens-remastered PROPERTIES
            OUTPUT_NAME "adventures-with-chickens"
            SUFFIX ".js")
        target_compile_definitions(adventures-with-chickens-remastered PRIVATE
            AWC_ASSET_ROOT="/assets"
            AWC_SOURCE_DIR="/awc"
            AWC_GUBSY_ASSETS_DIR="/gubsy-assets")
        target_link_options(adventures-with-chickens-remastered PRIVATE
            "-sMODULARIZE=1"
            "-sEXPORT_ES6=1"
            "-sEXPORT_NAME=createAdventuresWithChickens"
            "-sENVIRONMENT=web"
            "-sALLOW_MEMORY_GROWTH=1"
            "-sINITIAL_MEMORY=67108864"
            "-sSTACK_SIZE=8388608"
            "-sNO_EXIT_RUNTIME=1"
            "-sFILESYSTEM=1"
            "-sFORCE_FILESYSTEM=1"
            "-sMIN_WEBGL_VERSION=2"
            "-sMAX_WEBGL_VERSION=2"
            "-sEXPORTED_FUNCTIONS=['_native_awc_start','_native_awc_resize','_native_awc_frame','_native_awc_shutdown','_native_awc_audio_resume','_native_awc_last_error']"
            "-sEXPORTED_RUNTIME_METHODS=['ccall','FS']"
            "-lidbfs.js"
            "SHELL:--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/assets/graphics@/assets/graphics"
            "SHELL:--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/assets/data@/assets/data"
            "SHELL:--preload-file ${AWC_GUBSY_ASSETS_DIR}@/gubsy-assets")
    else()
        target_compile_definitions(adventures-with-chickens-remastered PRIVATE
            AWC_ASSET_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/assets"
            AWC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
            AWC_GUBSY_ASSETS_DIR="${AWC_GUBSY_ASSETS_DIR}")
        target_link_libraries(adventures-with-chickens-remastered PRIVATE PkgConfig::FLUIDSYNTH)
    endif()
    awc_target_defaults(adventures-with-chickens-remastered)
endif()
