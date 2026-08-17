# A Small Comprehensible Understanding

## What Strategies Should Be Prepared

The decompiler was developed incrementally, starting from ELF validation, section parsing, function discovery, and machine-code extraction. The extracted bytes are decoded into structured x86-64 instructions, then lifted into a low-level semantic IR instead of being translated directly from assembly text.

A few things to be noted are how hard it is to recover the original source code of a binary file from C++ compilated program, due to its reason like inlining, templates, compile-time execution, and lack of comprehensive runtime reflection. I tried searching up base knowledge (before vibecoding ts) and have several findings. What we should consider to follow through and design are as follow.

### Control-flow recovery
For this part, we need to recover the control flow of the binary file.

### Function signature recovery
For this part, we need to recover the function signatures of the binary file.

### Type inference
For this part, we need to infer the types of the binary file.

I figure this should be the foundational rule of design to build the decompiler. But of course, the most crucial part is control flow recovery as it was the main thing that might reconstruct the program's code structure. At that time, I was suggested to use a hybrid of SSA, which I implemented as simpliistic as I could, since the target here isn't complex structures like inheritances and such. 

SSA helps track where values are defined, how they propagate, and how values from different control-flow paths merge.From here, I can try implement bonus.Control flow is also represented using address-keyed basic blocks and typed CFG edges forfunction calling. 

High-level control-flow structures such as if, if/else, and loops are recovered using a hybrid pattern-based approach that can later be extended with more advanced analysis. Function signatures are inferred using CFG-aware System V AMD64 argument and return-value analysis, while types are reconstructed conservatively from collected usage evidence rather than assigned immediately. 

The GUI uses Qt 6 Widgets, PIE support keeps both ELF virtual and image-relative addresses, and binary patching allows same-size or shorter instruction replacements with NOP padding so the original ELF layout remains unchanged.

## Overall On How The Project Works

The project is organized as a pipeline with three primary layers. Each layer has a specific responsibility and passes structured data to the next layer. 

```text
Compiled x86-64 ELF
        ↓
Binary Frontend
ELF loader -> function discovery -> byte extraction -> x86-64 decoder
        ↓
Structured Instruction[]
        ↓
Decompiler Core
canonical IR -> CFG -> symbolic values -> expression DAG
             -> signatures/types -> structured control flow -> AST
        ↓
Presentation Layer
AST -> C++-like source
Instruction[] -> assembly/opcode table
CallGraph -> interactive graph
```

### 1. Binary Frontend

The Binary Frontend is responsible for understanding the physical binary format and the x86-64 instruction encoding. Its output is structured machine-level information, in this case similar to assembly.

#### ELF loading and validation

The ELF loader first checks that the input matches the supported baseline:

- ELF magic is valid,
- ELF class is 64-bit,
- endianness is little-endian,
- architecture is x86-64,
- ELF metadata and section/program-header ranges remain inside the file.

It then reads relevant sections such as `.text`, `.symtab`, `.strtab`, `.rodata`, and `.data`. Program headers are also read to identify loadable segments, PIE status, and the image base.

#### Function discovery and byte extraction

Functions are initially discovered from symbols whose ELF type is `STT_FUNC`. For each discovered function, the frontend stores:

```text
name
ELF virtual address
image-relative address
symbol size
machine-code bytes
```

A function's virtual address is mapped back to a file offset through its containing allocated ELF section. This mapping is reused later by binary patching.

The current baseline intentionally does not invent aggressive function boundaries for stripped binaries. If a reliable function symbol is unavailable, the frontend prefers an explicit limitation over guessing incorrect boundaries.

#### x86-64 decoding

The decoder converts raw function bytes into `Instruction` objects. Each instruction can retain:

- instruction address and raw bytes,
- opcode and condition code,
- register, immediate, relative, or memory operands,
- operand width and access mode,
- REX, ModR/M, SIB, scale, and displacement information,
- control-flow properties such as call, jump, conditional branch, and return.

The following relationship is important:

```text
machine bytes -> Instruction[] -> formatted assembly
```

Decompiler analysis consumes `Instruction[]` directly. It never parses the formatted assembly text.

### 2. Decompiler Core

The Decompiler Core converts machine-specific instructions into increasingly high-level, architecture-independent representations. This is where the project becomes a decompiler rather than only a disassembler.

#### Canonical IR lifting

Decoded instructions are lifted into a canonical intermediate representation. These operations include:

```text
assign
load-effective-address
binary operation
compare/test
conditional select
call
branch/jump
return
```

The purpose of canonical IR is to normalize different machine-code forms. For example, `xor eax, eax` can be normalized to constant zero, while `cmovge` can become a conditional-select value. Later passes therefore reason about semantics rather than exact opcode spelling.

#### Basic blocks and CFG

This part is very crucial, especially for recovery. CFG is known for its ability to represent control flow of a program and we can use it to reconstruct the original's control flow. A function is split into basic blocks using:

- the function entry,
- branch targets,
- instructions after control-flow terminators.

The blocks are stored in an address-keyed Control-Flow Graph. Directed edges describe fallthrough, true branch, false branch, or unconditional jump. The CFG allows analysis to reason about paths rather than assuming every instruction executes linearly.

```text
block_0
 ├── true  -> block_2
 └── false -> block_1
```

#### Symbolic value analysis and expression DAG

The symbolic pass tracks the meaning of values instead of preserving every register assignment as a separate C++ variable. Register aliases such as `eax` and `rax` belong to one canonical register family. Incoming System V argument registers are seeded as symbolic values such as `arg0` and `arg1`.

For example:

```asm
lea eax, [rdi + rsi]
ret
```

can produce the expression:

```text
arg0 + arg1
```

Expressions are represented as a DAG so repeated values can be shared and optimization-created temporary registers do not have to appear in final C++ output. At CFG joins, phi-like merge nodes are the intended representation when different paths produce different values.

#### Signature and type recovery

Function parameters are inferred using the System V AMD64 ABI. An incoming ABI register is considered a parameter when a reachable CFG path reads it before that path definitely overwrites it. Type inference is evidence-based. It considers operand width, arithmetic use, comparisons, address use, pointer arithmetic, and dereference behavior. The inferred categories are deliberately coarse:

```text
Unknown
Integer
Pointer
Boolean-like
Float
```

The original C++ type is not claimed when the binary does not contain enough evidence.

#### Control-flow recovery and AST

The structuring pass examines CFG shapes and attempts to recover high-level constructs such as `if` and `if/else`. Unsupported or irreducible structures retain block/comment/goto-style fallbacks instead of being forced into an incorrect high-level structure.

Recovered semantics are stored in a High-Level AST. The AST contains structural nodes such as function, block, return, if, while, goto, label, and comment. Keeping an AST separate from output formatting allows future analyses to modify program structure without editing generated C++ strings.

### 3. Presentation Layer

The Presentation Layer only displays analysis results. It must not perform instruction decoding, data-flow analysis, CFG recovery, or type inference.

#### C++-like emitter

The emitter traverses the High-Level AST and produces readable C++-like code. The output is intended to preserve the main operations and control flow, not reproduce the exact original source code. Generated names such as `arg0`, `local_4`, or `function_401230` are valid when original names are unavailable.

#### Assembly/opcode view

The assembly view formats the original structured instructions as:

```text
Address             Bytes                     Assembly
0x0000000000401116  b8 05 00 00 00            mov eax, 0x5
0x000000000040111b  c3                        ret
```

Keeping this view alongside C++-like output lets the user compare recovered semantics with the source machine instructions.

#### Qt desktop interface

The Qt GUI provides:

- binary file selection,
- searchable function list,
- function address and size,
- C++-like source display,
- assembly/opcode display,
- direct-call navigation,
- call graph zoom and navigation,
- instruction-level binary patching.

`DecompilerSession` coordinates the complete pipeline and stores results for the GUI. The session is an application-level coordinator, not a fourth analysis layer: it invokes the frontend and core, then gives immutable-style result data to presentation widgets.

### Why The Layer Boundaries Matter

The separation prevents several common design problems:

- The GUI cannot accidentally become the only place where decompilation works.
- The decompiler does not depend on assembly formatting strings.
- ELF parsing remains reusable by CLI, GUI, tests, and patching.
- Unsupported instructions can degrade locally without invalidating unrelated functions.
- Call graph and patch refresh can rerun the same analysis pipeline.
- PIE and optimized binaries can extend address and semantic analysis without rewriting presentation code.

In short, data becomes more abstract as it moves through the pipeline:

```text
bytes
-> machine instructions
-> semantic IR
-> control/data-flow graphs
-> expressions and structured statements
-> AST
-> human-readable C++-like output
```

## Guide to Run

From the `my-kisah/` project directory, build the decompiler applications:

```bash
cmake -S . -B build
cmake --build build
```

Prepare any supported C++ input program as an ELF64 x86-64 binary. For the baseline configuration:

```bash
g++ <input-source.cpp> \
    -O1 \
    -fno-inline \
    -fno-omit-frame-pointer \
    -no-pie \
    -o <output-binary>
```

Run the command-line decompiler:

```bash
./build/my-kisah <output-binary>
```

Run the desktop application and select a binary through **Open Binary**:

```bash
./build/my-kisah-gui
```

A binary can also be opened directly when starting the GUI:

```bash
./build/my-kisah-gui <output-binary>
```

For example, one of the included corpus programs can be prepared and opened with:

```bash
mkdir -p tests/generated

g++ tests/corpus/constant.cpp \
    -O1 \
    -fno-inline \
    -fno-omit-frame-pointer \
    -no-pie \
    -o tests/generated/constant_O1_nopie

./build/my-kisah-gui tests/generated/constant_O1_nopie
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

Assembly / Opcode:

<_Z8constantv>:
Address             Bytes                     Assembly
0x0000000000401116  b8 05 00 00 00            mov eax, 0x5
0x000000000040111b  c3                        ret

Low-Level IR:

<_Z8constantv>:
0x0000000000401116  eax = 0x5
0x000000000040111b  return rax

SSA / Symbolic Expressions:

<_Z3addii>:
0x0000000000401116  rax_1 = (arg0_0 + arg1_0)
0x0000000000401119  return (arg0_0 + arg1_0)

Basic Blocks / CFG:

<_Z6choosei>:
entry: block_0
block_0 [0x401122, 0x40112a)
  -> block_3 (true)
  -> block_1 (false)
block_1 [0x40112a, 0x40112f)
  -> block_2 (fallthrough)
block_2 [0x40112f, 0x401131) -> <exit>
block_3 [0x401131, 0x401138)
  -> block_2 (jump)
```

## Generated Testcases

Regression sources are stored in `tests/corpus/`. Generated ELF files should be placed in `tests/generated/` and are ignored by Git.

The filename records the compiler configuration:

```text
add_O1_nopie
│   │  └── non-PIE
│   └──── optimization level
└──────── source test
```

Generate the optimization and PIE matrix with:

```bash
mkdir -p tests/generated

g++ tests/corpus/add.cpp -O1 -fno-inline -fno-omit-frame-pointer -no-pie -o tests/generated/add_O1_nopie
g++ tests/corpus/add.cpp -O1 -fno-inline -fno-omit-frame-pointer -o tests/generated/add_O1_pie
g++ tests/corpus/add.cpp -O2 -fno-inline -fno-omit-frame-pointer -o tests/generated/add_O2_pie
g++ tests/corpus/add.cpp -O3 -fno-inline -fno-omit-frame-pointer -o tests/generated/add_O3_pie
```

| Source | Purpose | Observed result |
|---|---|---|
| `constant.cpp` | Constant return | Recovers `return 5` |
| `add.cpp` | ABI arguments and arithmetic | Recovers `arg0 + arg1` across tested optimization/PIE variants |
| `maximum.cpp` | Comparison and `cmovge` | Recovers a conditional maximum expression |
| `branch_calls.cpp` | Conditional branches and calls | Builds true/false/jump CFG edges and call graph links |

Example reconstructed output:

```cpp
int64_t _Z3addii(int64_t arg0, int64_t arg1) {
    return arg0 + arg1;
}

int64_t _Z7maximumii(int32_t arg0, int32_t arg1) {
    return arg0 >= arg1 ? arg0 : arg1;
}
```

`readelf` and `objdump` are used only as development oracles. The application performs its own ELF parsing and instruction decoding.

## Bonus Implementations

### Call Graph

The call graph is stored independently from Qt rendering. Discovered functions are nodes, while resolved direct calls and final direct-jump tail-call candidates become directed edges. The GUI supports pan, zoom, fit, and double-click function navigation.

### PIE and Optimized Binaries

Functions store both exact ELF virtual addresses and normalized image-relative addresses. ELF type and `PT_LOAD` headers provide PIE and image-base information.

The regression corpus is tested using `-O1 -no-pie`, `-O1 PIE`, `-O2 PIE`, and `-O3 PIE`. Current optimized semantic support includes `cmovcc`, `setcc`, zero idioms, conditional-select expressions, RIP-relative addressing, constant-propagated clones, and tail-call candidates.

### Binary Patching

The GUI patches one complete selected instruction at a time. Replacement bytes must be the same size or shorter; shorter replacements are padded with `0x90` NOP bytes.

The patcher:

1. maps the selected virtual address to an executable-section file offset,
2. writes to a separate output copy,
3. reloads the patched ELF,
4. re-runs decoding and decompilation,
5. refreshes assembly, C++ output, and the call graph.

The original ELF is never overwritten automatically.

## Known Limitations

- Function discovery primarily depends on ELF function symbols.
- The x86-64 decoder intentionally supports a limited subset.
- Floating-point and broad SIMD/vector instructions are incomplete.
- Complex templates, exceptions, RTTI, and virtual-class recovery are unsupported.
- Complex or irreducible CFGs may use comments, blocks, or goto-style fallback.
- Phi-like value merging and loop structuring remain partial.
- Indirect-call resolution and exact call argument counts are incomplete.
- `-O2`/`-O3` support is semantic but not exhaustive for every compiler transformation.
- Binary patching does not support code caves, trampolines, or larger replacement code.

Unsupported information remains explicit rather than being presented as certain source code.

----
> Isn't this just TBFO, what. Man.
