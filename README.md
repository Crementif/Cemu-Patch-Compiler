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
- There is no runtime / standard library. You have to bootstrap everything yourself, including `new` and `delete`
- Static constructors are not run and global variables are not automatically initialized. You can work around this by declaring them `constinit` or having a manually invoked init function that handles it.
- The VTable format is different. This is not an issue as long as your code is self-contained. But if you replace classes in a game then you have to model the game's VTables as structs with pointers and offset fields
- No real linking support. Rather this just appends one translation unit after another. This is not a huge issue but means there are side effects, like using a static function from a header in multiple .cpp files will not be deduplicated

---

## Building & Testing

```cmd
build.bat                  # Build executable (Release)
powershell .\test_compile_examples.ps1   # Run integration tests
```
