# A Small Comprehensible Understanding

## Implementation Strategy

A few things to be noted are how hard it is to recover the original source code of a binary file from C++ compilated program, due to its reason like inlining, templates, compile-time execution, and lack of comprehensive runtime reflection.

What we should consider to follow through and design are as follow.
Design these in this order:
### Control-flow recovery

For this part, we need to recover the control flow of the binary file.

### Function signature recovery

For this part, we need to recover the function signatures of the binary file.

### Type inference

For this part, we need to infer the types of the binary file.


## How the Project Works

The intended architecture is separated into three layers:

```text
Binary Frontend
ELF loader -> function discovery -> x86-64 decoder -> structured instructions

Decompiler Core
instructions -> IR lifting -> data-flow analysis -> CFG -> expressions -> high-level AST

Presentation Layer
AST -> C++-like code
instructions -> assembly/opcode display
```

Milestone 1 implemented ELF validation inside the Binary Frontend.

Milestone 2 adds ELF section-header parsing for `.text`, `.symtab`, `.strtab`, `.rodata`, and `.data`.

Milestone 3 parses `.symtab` and `.strtab` to discover symbols whose ELF type is `STT_FUNC`.

Milestone 4 maps function virtual addresses back to ELF file offsets using allocated sections and extracts the corresponding machine-code bytes.

Milestone 5 defines the x86-64 structured instruction representation. The selected approach is Option C: start compact, but keep fields such as raw bytes, address, operand width, memory addressing details, REX, ModR/M, and SIB so the decoder and later binary patching work can grow without replacing the model.

The frontend deliberately does not perform instruction decoding, IR lifting, or GUI work yet.

## Guide to Run

From `my-kisah/`:

```bash
cmake -S . -B build
cmake --build build
g++ tests/corpus/constant.cpp -O1 -fno-inline -fno-omit-frame-pointer -no-pie -o build/constant_O1_nopie
./build/my-kisah build/constant_O1_nopie
```

Expected current ELF-frontend behavior:

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

## Generated Testcases

----
> Isn't this just TBFO, what. Man.