cmake_minimum_required(VERSION 3.20)
project(DroneCam LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(PLUGIN_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/plugin-sdk" CACHE PATH "Path to plugin-sdk")

include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.5
)
FetchContent_MakeAvailable(imgui)

add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx9.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
)
target_include_directories(imgui_lib PUBLIC 
    ${imgui_SOURCE_DIR} 
    ${imgui_SOURCE_DIR}/backends
)

file(GLOB PLUGIN_SHARED_SOURCES "${PLUGIN_SDK_DIR}/shared/*.cpp")

add_library(plugin_sa STATIC ${PLUGIN_SHARED_SOURCES})

target_compile_definitions(plugin_sa PUBLIC
    GTASA
    _CRT_SECURE_NO_WARNINGS
    _CRT_NON_CONFORMING_SWPRINTFS
    _USE_MATH_DEFINES
    NOMINMAX
)

target_include_directories(plugin_sa PUBLIC
    "${PLUGIN_SDK_DIR}/plugin_sa"
    "${PLUGIN_SDK_DIR}/plugin_sa/game_sa"
    "${PLUGIN_SDK_DIR}/shared"
    "${PLUGIN_SDK_DIR}/shared/game"
)

if(MSVC)
    target_compile_options(plugin_sa PRIVATE /MP /W3 /permissive)
endif()

add_library(DroneCam SHARED main.cpp)

target_include_directories(DroneCam PRIVATE
    ${PLUGIN_SDK_DIR}/plugin_sa
    ${PLUGIN_SDK_DIR}/plugin_sa/game_sa
    ${PLUGIN_SDK_DIR}/shared
    ${PLUGIN_SDK_DIR}/shared/game
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_compile_definitions(DroneCam PRIVATE
    GTASA
    _CRT_SECURE_NO_WARNINGS
    _CRT_NON_CONFORMING_SWPRINTFS
    _USE_MATH_DEFINES
    NOMINMAX
)

if(MSVC)
    target_compile_options(DroneCam PRIVATE /MP /permissive)
endif()

set_target_properties(DroneCam PROPERTIES
    SUFFIX ".asi"
    PREFIX ""
)

target_link_libraries(DroneCam PRIVATE 
    plugin_sa
    imgui_lib 
    d3d9.lib
)
