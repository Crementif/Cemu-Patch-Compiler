# Cemu (C++) Patch Compiler

`Cemu Patch Compiler` is a bridge compiler utility that compiles C/C++ source files into PowerPC (PPC) assembly patches compatible with Cemu's patching framework. It processes the compiled assembly to adapt register syntax, labels, and import relocations into a format Cemu can inject.

---

## 1. Setup: PowerPC Compiler Installation

Before using `Cemu Patch Compiler`, you must configure a PowerPC compiler toolchain (GCC is recommended).

### PowerPC GCC Toolchain (Recommended)
1. Download the PowerPC EABI toolchain from [SysProgs PowerPC Toolchains](https://gnutoolchains.com/powerpc-eabi/) (e.g., `powerpc-eabi-gcc4.9.0.exe` or newer).
2. Install or extract it on your system, then specify the path to the compiler in your `config.ini` file:
   ```ini
   [Compilers]
   GCC = C:/SysGCC/powerpc-eabi/bin/powerpc-eabi-gcc.exe
   ```
   *(Alternatively, you can install the toolchain to a local `compilers/GCC` directory next to `CemuPatchCompiler.exe` for automatic detection).*

### PowerPC LLVM/Clang (Alternative)
1. Install a target-compatible LLVM/Clang toolchain.
2. Specify the path to the compiler in your `config.ini` file:
   ```ini
   [Compilers]
   Clang = C:/Path/To/LLVM/bin/clang++.exe
   ```
   *(Alternatively, you can place the toolchain in a local `compilers/clang` directory next to `CemuPatchCompiler.exe` for automatic detection).*

---

## 2. Usage Modes

`Cemu Patch Compiler` supports four execution modes for compiling files:

### Drag & Drop
You can drag a source folder or a single C/C++ file and drop it directly onto the `CemuPatchCompiler.exe` executable:
- **Folder**: Compiles all `.cpp` and `.c` files in the folder and outputs to `patch_compiled.asm` in that folder.
- **File**: Compiles the file (and other files in its parent directory) and outputs to `patch_compiled.asm` in the parent directory.

### Portable Folder Compilation
If you place `CemuPatchCompiler.exe` next to some C/C++ files (such as `.cpp`, `.c`, `.h`, `.hpp` files) and run it:
- The tool automatically detects C/C++ files in the current folder.
- It compiles them and outputs to a local `patch_compiled.asm` file in the same directory.

### Command Line Interface (CLI)
You can run the utility from a command prompt or script:
```powershell
# Compile all source files in a specific directory
CemuPatchCompiler.exe "C:\Path\To\Source"

# Compile all source files and specify a custom output path
CemuPatchCompiler.exe "C:\Path\To\Source" "C:\Path\To\Output\final_patch.asm"
```

### Config File (`config.ini`)
If run without command-line arguments and there are no C/C++ files in the current working directory, the tool falls back to the paths defined in `config.ini` (which is generated automatically with defaults if missing, though the template directories themselves are not created).

Example `config.ini`:
```ini
[Paths]
; Directory containing C/C++ source files to compile (relative to config or absolute)
SourceDir = examples/camera

; Output file path for compiled assembly patch
OutFile = examples/camera/patch_compiled.asm

[Compilers]
; [Optional] Custom compiler paths (relative to config or absolute)
;GCC = compilers/GCC/bin/powerpc-eabi-gcc.exe
;Clang = compilers/clang/bin/clang++.exe
```
