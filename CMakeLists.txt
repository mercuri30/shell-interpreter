cmake_minimum_required(VERSION 2.8)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED OFF)

project(shell_interpreter)

add_executable(shell_interpreter
    src/shell_interpreter.c
)
