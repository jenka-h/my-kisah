cmake_minimum_required(VERSION 3.16)

project(my_kisah_decompiler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

add_library(my-kisah-core STATIC
    # Application orchestration
    src/application/BinaryPatcher.cpp
    src/application/DecompilerSession.cpp

    # Binary frontend
    src/frontend/elf/ElfFile.cpp
    src/frontend/elf/ElfValidator.cpp
    src/frontend/x86/Decoder.cpp
    src/frontend/x86/Formatter.cpp
    src/frontend/x86/Instruction.cpp

    # Decompiler core: canonical IR
    src/decompiler/ir/IR.cpp
    src/decompiler/ir/IRLifter.cpp

    # Decompiler core: analyses
    src/decompiler/analysis/Expression.cpp
    src/decompiler/analysis/SSAAnalyzer.cpp
    src/decompiler/analysis/Signature.cpp
    src/decompiler/analysis/TypeInference.cpp

    # Decompiler core: control flow
    src/decompiler/controlflow/CFG.cpp
    src/decompiler/controlflow/ControlFlow.cpp

    # Decompiler core: high-level representation
    src/decompiler/ast/AST.cpp
    src/decompiler/graph/CallGraph.cpp

    # Non-GUI presentation
    src/presentation/cpp/CppEmitter.cpp
)

target_include_directories(my-kisah-core PUBLIC src)
target_compile_options(my-kisah-core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(my-kisah
    src/presentation/cli/main.cpp
)
target_link_libraries(my-kisah PRIVATE my-kisah-core)
target_compile_options(my-kisah PRIVATE -Wall -Wextra -Wpedantic)

add_executable(my-kisah-gui
    src/presentation/gui/main.cpp
    src/presentation/gui/CallGraphView.cpp
    src/presentation/gui/CallGraphView.h
    src/presentation/gui/MainWindow.cpp
    src/presentation/gui/MainWindow.h
)

target_link_libraries(my-kisah-gui PRIVATE
    my-kisah-core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

target_compile_options(my-kisah-gui PRIVATE -Wall -Wextra -Wpedantic)
