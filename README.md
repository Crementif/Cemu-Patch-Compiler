# Cemu Patch Compiler

`Cemu Patch Compiler` is a tool that compiles C/C++ source files into PowerPC assembly patches that you can use inside Cemu's graphic packs to make complex modifications to the game's code.

---

## Usage Modes

- **Place Compiler In The Same Folder As Your Code**: Place `CemuPatchCompiler.exe` next to the C/C++ code that you want to compile, and just double click it to create a patch in that folder.
- **Drag & Drop**: Drag a source folder or `.cpp`/`.c` file directly onto `CemuPatchCompiler.exe`.
- **Command Line Arguments**: Run from terminal to pass custom input/output paths or flags. See [Command Line Options](#command-line-options).
- **Config File (`config.ini`)**: When run without arguments and no local sources are found, it looks for a nearby `config.ini`. See [Configuration File](#configuration-file-configini).

---

## Command Line Options

Running with CLI arguments explicitly overrides any other settings.

```powershell
# Display help and usage
CemuPatchCompiler.exe -h

# Compile a source directory to custom output file
CemuPatchCompiler.exe "C:\Path\To\Source" -o "C:\Path\To\Output\final_patch.asm"

# Compile with target module checksum override
CemuPatchCompiler.exe "C:\Path\To\Source" -c 0x6267BFD0

# Extend hardcoded instructions as .int (stop-gap workaround for Cemu's assembler)
CemuPatchCompiler.exe "C:\Path\To\Source" -i "SUBF, MULLW"
```

| Flag | Description |
|---|---|
| `-h`, `--help` | Show usage instructions and exit. |
| `-v`, `--version` | Show compiler version. |
| `-o`, `--output <file\|dir>` | Specify target output patch file or directory. |
| `-c`, `--checksum <hex>` | Override target module checksum (e.g. `0x6267BFD0`). |
| `-i`, `--hardcode-instructions <list>` | Extend instructions to hardcode as `.int` (e.g. `SUBF, MULLW`). |
| `-O0`, `-O1`, `-O2`, `-O3`, `-Os`, `-Oz` | Optimization level (default `-Os`). |

---

## Configuration File (`config.ini`)

```ini
[Paths]
; Format: sourceFolder = outputFolder (or outputFile)
examples/camera = examples/camera/patch_compiled.asm
examples/graphics = examples/graphics/patch_compiled.asm

[Settings]
; Stop-gap workaround for instructions unsupported by Cemu's assembler
hardcodeInstructions = SUBF, MULLW
```

---

## Missing Instructions Workaround

Sometimes you'll encounter that certain code will cause Clang (the compiler used internally) to use rare PowerPC instructions that Cemu's assembler isn't familiar with.
Consider adding support for this instruction and sending a PR to [Cemu](https://github.com/cemu-project/Cemu).

As it'll likely take a bit for this instruction support to reach Cemu users, there's also a useful built-in system to hardcode these instructions as raw hex values instead as `.int 0x[encoded opcode]` which bypasses Cemu's assembler.

You can find the list of hardcoded instructions in [InstructionAssembler.cpp](/src/InstructionAssembler.cpp), but you can use `config.ini` or command line arguments to add new instructions to this list. See the CLI and config file sections for more info.

---

## Limitations

Originally this was intended to only translate individual functions but over time it got expanded to handle larger projects. But there are still some limitations:
- No exceptions, RTTI, threads, iostreams, files or locales. Anything that would throw calls `OSFatal`.
- `new` and `delete` use the game's default heap, but this can be overridden by defining `cemu_patch_alloc` and `cemu_patch_free`.
- `sinf`, `cosf` and `tanf` return NaN for values above `4.22e8`.
- Static destructors don't work.
- The VTable format is different. This is not an issue as long as your code is self-contained. But if you replace classes in a game then you have to model the game's VTables as structs with pointers and offset fields
- Sources are compiled as C++20, also `.c` files.
- Only `extern "C"` symbols, `__attribute__((used))` symbols and symbols named in a patch directive or `asm` block stay visible to the graphic pack.

---

## Building & Testing

Clone with `--recursive`, or run `git submodule update --init`.
The first configure builds LLVM from the vcpkg submodule, which takes a while.

```cmd
build.bat                  # Build executable (Release)
powershell .\test_compile_examples.ps1   # Run integration tests
```

---

## Credits

- [LLVM](https://llvm.org) is used to build the compiler.
- The headers of [libc++](https://libcxx.llvm.org) 18.1.6 are included in [dependencies/libcxx](/dependencies/libcxx).
- clang's freestanding C headers are included.

All of these are licensed under the Apache License v2.0 with LLVM Exceptions, see [LICENSE.TXT](/dependencies/libcxx/LICENSE.TXT).
