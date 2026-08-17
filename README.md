# My-Mine-Gweh

![Mine](./asset/chiaki.jpg)

---

> I don't really have joy in this anymore. Goodluck to the rest^^

A simplified x86-64 ELF decompiler for C++ binaries, built with C++17 and Qt 6. The application provides C++-like reconstruction, assembly/opcode display, function navigation, call graph visualization, PIE-aware addresses, and instruction-level binary patching.

## Features

- ELF64 little-endian x86-64 loading
- ELF section and function-symbol parsing
- Incremental x86-64 instruction decoding
- C++-like function reconstruction
- Function signature and basic type hints
- Basic control-flow recovery
- Assembly and opcode display
- Searchable Qt function browser
- Direct-call navigation
- Interactive call graph
- PIE and initial `-O2`/`-O3` support
- Same-size or shorter binary patches with NOP padding

Unsupported instructions and uncertain semantics are shown explicitly instead of being silently invented.

## Project Structure

```text
src/
├── application/                 Pipeline coordination and binary patching
├── frontend/
│   ├── elf/                     ELF loading and address handling
│   └── x86/                     Instruction model, decoder, and formatter
├── decompiler/
│   ├── ir/                      Canonical IR and lifting
│   ├── analysis/                Symbolic, signature, and type analysis
│   ├── controlflow/             Basic blocks, CFG, and structuring
│   ├── ast/                     High-level AST
│   └── graph/                   Call graph model
└── presentation/
    ├── cpp/                     C++-like emitter
    ├── cli/                     CLI application
    └── gui/                     Qt desktop application

tests/
└── corpus/                      C++ regression sources
```

## Requirements

- C++17 compiler
- CMake 3.16 or newer
- Qt 6 Core, Gui, and Widgets development packages

## Build

```bash
cmake -S . -B build
cmake --build build
```

Generated applications:

```text
build/my-kisah       CLI decompiler
build/my-kisah-gui   Qt desktop decompiler
```

## Run

Start the desktop application:

```bash
./build/my-kisah-gui
```

Open an ELF binary directly:

```bash
./build/my-kisah-gui <binary>
```

Run the CLI:

```bash
./build/my-kisah <binary>
```

The baseline input configuration is:

```bash
g++ <input.cpp> \
    -O1 \
    -fno-inline \
    -fno-omit-frame-pointer \
    -no-pie \
    -o <binary>
```

Generated corpus binaries are test inputs and are not part of the CMake application build.

## Documentation

Detailed documentation is available in [`docs/README.md`](docs/README.md), including:

- complete decompiler process
- implementation strategies and design decisions
- build and usage guide
- generated test cases and results
- bonus implementations
- supported behavior and known limitations

## References

- [Why is there no accurate C decompiler?](https://stackoverflow.com/questions/36525356/why-is-there-no-accurate-c-decompiler)
- [Is there a C++ decompiler?](https://stackoverflow.com/questions/205059/is-there-a-c-decompiler)
- [tiny-dec](https://github.com/ZhangZhuoSJTU/tiny-dec)
- Static Single Assignment for Decompilation — Michael James Van Emmerik
- [Combining SSA instructions into statements](https://stackoverflow.com/questions/73245312/how-decompilers-combine-multiple-ssa-form-instructions-into-one-statement)
