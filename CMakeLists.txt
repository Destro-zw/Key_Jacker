cmake_minimum_required(VERSION 3.16)

project(Key_Jacker VERSION 1.0.0 LANGUAGES CXX)

add_library(Key_Jacker STATIC
    Key_Jacker.h
    Key_Jacker.cpp
    src/KJ_Controls.h
    src/KJ_Controls.cpp
    src/KJ_Global.h
    src/KJ_Keyboard.h
    src/KJ_Keyboard.cpp
    src/KJ_Mouse.h
    src/KJ_Mouse.cpp
    src/KJ_Joystick.h
    src/KJ_Joystick.cpp
)

target_compile_features(Key_Jacker PUBLIC cxx_std_17)

# Consumers include "Key_Jacker.h" from this directory.
target_include_directories(Key_Jacker PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(Key_Jacker PUBLIC xinput.lib winmm.lib)