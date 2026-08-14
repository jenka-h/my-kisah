cmake_minimum_required(VERSION 3.16)

project(my_kisah_decompiler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(my-kisah
    src/main.cpp
    src/elf/ElfFile.cpp
    src/elf/ElfValidator.cpp
    src/x86/Instruction.cpp
)

target_include_directories(my-kisah PRIVATE
    src
)

target_compile_options(my-kisah PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)
