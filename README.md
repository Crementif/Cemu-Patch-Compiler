# Cemu (C++) Patch Compiler

`Cemu Patch Compiler` is a bridge compiler utility that compiles C/C++ source files into PowerPC (PPC) assembly patches compatible with Cemu's patching framework. It processes the compiled assembly to adapt register syntax, labels, and import relocations into a format Cemu can inject.

---

## 1. PowerPC Compiler Support

`Cemu Patch Compiler` has a built-in, statically-linked LLVM and Clang compiler targeting 32-bit PowerPC (PPC32). 

There is **no setup required**! You do not need to download or install external GCC or Clang toolchains. The compiler is fully self-contained.

---

## 2. Usage Modes

`Cemu Patch Compiler` supports four execution modes for compiling files:

### Drag & Drop
You can drag a source folder or a single C/C++ file and drop it directly onto the `CemuPatchCompiler.exe` executable:
- **Folder**: Compiles all `.cpp`, `.c`, `.cc`, and `.cxx` files in the folder into `patch_compiled.asm`. Each source file becomes its own ModuleGroup inside that file. Internal per-file symbols are namespaced automatically, weak/ODR definitions such as `inline` functions or `inline` variables are coalesced across files, and duplicate strong exports are reported as errors.
- **File**: Compiles that file into `patch_compiled.asm` in the parent directory by default.

### Portable Folder Compilation
If you place `CemuPatchCompiler.exe` next to some C/C++ files (such as `.cpp`, `.c`, `.h`, `.hpp` files) and run it:
- The tool first looks for compilable files under `src/` and `source/`.
- If neither exists, it falls back to compilable files in the current folder.
- It compiles them into a local `patch_compiled.asm`, with one ModuleGroup per source file.

### Command Line Interface (CLI)
You can run the utility from a command prompt or script:
```powershell
# Compile all source files in a specific directory
CemuPatchCompiler.exe "C:\Path\To\Source"

# Compile all source files into a specific output file
CemuPatchCompiler.exe "C:\Path\To\Source" "C:\Path\To\Output\final_patch.asm"

# Compile a single source file into a specific output file
CemuPatchCompiler.exe "C:\Path\To\Source\main.cpp" "C:\Path\To\Output\main.asm"
```

Each compilable source file can optionally begin with a metadata comment that overrides the Cemu module checksum:

```cpp
// moduleMatches = 0x6267BFD0
```

If that first-line directive is absent, the compiler uses the built-in BotW checksum `0x6267BFD0`.

For local emulator smoke tests, the repository also includes `testing/run_homebrew_test.ps1`. That script launches the homebrew title in `testing/`, rewrites the sample source checksum directive in `examples/testing/` to match the module checksum found in Cemu's log, compiles the sample patch, and then relaunches Cemu to verify the patch loads cleanly.

### Config File (`config.ini`)
If run without command-line arguments and no compilable sources are found automatically, the tool falls back to the paths defined in `config.ini`. If `config.ini` is missing, it first tries the built-in default paths and only writes a default `config.ini` if that fallback still does not produce a patch.

Example `config.ini`:
```ini
[Paths]
; Directory containing C/C++ source files to compile (relative to config or absolute)
SourceDir = examples/camera

; Output file path for the compiled assembly patch
OutFile = examples/camera/patch_compiled.asm
```
