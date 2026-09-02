cmake_minimum_required(VERSION 3.24)

project(WarmNote LANGUAGES CXX)

if(NOT MSVC)
    message(FATAL_ERROR "WarmNote currently requires the MSVC compiler.")
endif()

find_package(OpenCV 5 REQUIRED COMPONENTS core)

option(WARMNOTE_ENABLE_LOGGING "Enable WarmNote stderr logging" ON)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/WarmNoteAddActionLayer.cmake")

set(WARMNOTE_ACTION_LAYER_SOURCES)
set(WARMNOTE_ACTION_LAYER_TYPES)
set(WARMNOTE_ACTION_LAYER_DECLARATIONS)
set(WARMNOTE_ACTION_LAYER_REGISTRY)

# 参数依次为param中的动作类型和对应的C++动作层类名。
warmnote_add_action_layer(KeyCodeInput KeyCodeInputLayer)
warmnote_add_action_layer(Tremolo TremoloLayer)

set(WARMNOTE_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${WARMNOTE_GENERATED_INCLUDE_DIR}")
configure_file("cmake/action_layer_declaration.h.in" "${WARMNOTE_GENERATED_INCLUDE_DIR}/action_layer_declaration.h" @ONLY)
configure_file("cmake/action_layer_registry.h.in" "${WARMNOTE_GENERATED_INCLUDE_DIR}/action_layer_registry.h" @ONLY)

add_library(warmnote_backend STATIC
        src/Screenshot.cpp
        include/Core.h
        src/Core.cpp
        include/Control.h
        src/Control.cpp
        include/Score.h
        src/Score.cpp
        ${WARMNOTE_ACTION_LAYER_SOURCES}
)

target_include_directories(warmnote_backend
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${OpenCV_INCLUDE_DIRS}
    PRIVATE
        ${WARMNOTE_GENERATED_INCLUDE_DIR}
)

target_compile_features(warmnote_backend PUBLIC cxx_std_20)
target_compile_definitions(warmnote_backend
    PUBLIC
        UNICODE
        _UNICODE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        WARMNOTE_ENABLE_LOGGING=$<BOOL:${WARMNOTE_ENABLE_LOGGING}>
)
target_compile_options(warmnote_backend PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(warmnote_backend PUBLIC Gdi32 User32 ${OpenCV_LIBS})

add_executable(WarmNote
        src/main.cpp
)

target_compile_options(WarmNote PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(WarmNote PRIVATE warmnote_backend)
