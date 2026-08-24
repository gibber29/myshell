cmake_minimum_required(VERSION 3.16)
project(myshell CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Turn on strong warnings — helps catch mistakes with raw fds/pointers early.
add_compile_options(-Wall -Wextra -Wpedantic -g)

add_executable(myshell
    src/main.cpp
    src/Shell.cpp
    src/Tokenizer.cpp
)

target_include_directories(myshell PRIVATE src)
