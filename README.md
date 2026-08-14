# My-Mine-Gweh

![Mine](./asset/chiaki.jpg)

---

>I don't really have joy in this anymore. Goodluck to the rest^^

Decompiler with disassembly, control-flow reconstruction, C++ code generation, function navigation, call graph visualization, and binary patching.

This project will target on C++ executable binary file and create a similar decompiled output as a C++ source code. A unique feature that C++ has is optimization of the code generation process, hence making it difficult to reverse engineer.

---
## Project Structure

```text
src/
├── elf/          Binary frontend ELF parsing/validation
├── x86/          Future x86-64 instruction decoder
└── main.cpp      Milestone CLI entry point

tests/corpus/     Generated C++ test programs
```

___
## Status on Task

### Milestone 1 — ELF Validation

Implemented a baseline ELF validator that reads a file and reports whether it is:

- an ELF file,
- ELF64,
- little endian,
- x86-64.

This was intentionally only the first binary-frontend step.

### Milestone 2 — ELF Sections

Implemented section-header parsing and display for the baseline sections needed by later milestones:

- `.text`
- `.symtab`
- `.strtab`
- `.rodata`
- `.data`

The CLI now prints each discovered section's name, file offset, virtual address, and size.

### Milestone 3 — Function Symbols

Implemented `.symtab` parsing for ELF symbols whose type is `STT_FUNC`. Function symbols are represented as:

```cpp
struct Function {
    std::string name;
    uint64_t address;
    uint64_t size;
    std::vector<uint8_t> bytes;
};
```

### Milestone 4 — Function Byte Extraction

Implemented virtual-address to file-offset mapping using allocated ELF sections. Each discovered function now receives its machine-code bytes in `Function::bytes`.

The frontend still does not use function-discovery heuristics, decode instructions, lift IR, or decompile yet.

### Milestone 5 — Instruction Representation

Decision chosen: **Option C**, a compact structured instruction representation designed to grow into a richer x86 model later.

Implemented in `src/x86/Instruction.h` and `src/x86/Instruction.cpp`:

- `Opcode`
- `Operand`
- `Register`
- operand width
- immediate operands
- memory operands with base/index/scale/displacement/RIP-relative support
- relative branch/call targets
- raw instruction bytes
- instruction address
- retained REX/ModR/M/SIB fields for future decoder/debugging work
- basic control-flow flags

This decision should eventually be documented in the README implementation-strategy section with the reason: the decompiler core must consume structured instructions, not parse assembly strings.

Build:

```bash
cmake -S . -B build
cmake --build build
```

Create the first generated test binary:

```bash
g++ tests/corpus/constant.cpp -O1 -fno-inline -fno-omit-frame-pointer -no-pie -o build/constant_O1_nopie
```

Run:

```bash
./build/my-kisah build/constant_O1_nopie
```

Expected output includes:

```text
ELF magic: yes
ELF64: yes
Endian: little
Architecture: x86-64

Sections:
Name           File Offset   Virtual Address          Size
.text       0x...
.symtab     0x...
.strtab     0x...
.rodata     0x...
.data       0x...

Functions:
Name                                       Address          Size   Bytes Extracted
constant                            0x0000000000401106  0x00000000000b                11
main                                0x0000000000401111  0x00000000000b                11
```

---
## Notes on Documentation
Documentation are available in `my-kisah/docs/`.

---
## References:
- C++ Compilers?: https://stackoverflow.com/questions/205059/is-there-a-c-decompiler
- Github Example on C Decompiler: https://github.com/ZhangZhuoSJTU/tiny-dec
- Static Single Assignment for Decompilation by Michael James Van Emmerik
- Just Why: https://stackoverflow.com/questions/36525356/why-is-there-no-accurate-c-decompiler
