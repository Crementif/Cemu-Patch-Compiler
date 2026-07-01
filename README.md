# Cemu (C++) Patch Compiler

`Cemu Patch Compiler` is a bridge compiler utility that compiles C/C++ source files into PowerPC (PPC) assembly patches compatible with Cemu's patching framework. It processes the compiled assembly to adapt register syntax, labels, and import relocations into a format Cemu can inject.

---

## 1. Setup: PowerPC Compiler Installation

Before using `Cemu Patch Compiler`, you must place a PowerPC compiler toolchain in the `compilers/` directory (located next to the executable):

### PowerPC GCC Toolchain (Recommended)
1. Download the PowerPC EABI toolchain from [SysProgs PowerPC Toolchains](https://gnutoolchains.com/powerpc-eabi/) (e.g., `powerpc-eabi-gcc4.9.0.exe` or newer).
2. Install or extract it directly into the `compilers/GCC/` directory next to the `CemuPatchCompiler.exe`. You can make this folder yourself, or run the program once.
3. Verify that the files are located at:
   - `compilers/GCC/bin/powerpc-eabi-gcc.exe`
   - `compilers/GCC/bin/powerpc-eabi-objcopy.exe`

### PowerPC LLVM/Clang (Alternative)
- Place a target-compatible LLVM/Clang toolchain in the `compilers/clang/` directory:
  - `compilers/clang/bin/clang++.exe` (or `compilers/clang/clang++.exe`)

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
If run without command-line arguments and there are no C/C++ files in the current working directory, the tool falls back to the paths defined in `config.ini` (which is generated automatically with defaults if missing).

Example `config.ini`:
```ini
[Paths]
; Directory containing C/C++ source files to compile (relative to config or absolute)
SourceDir = examples/camera

; Output file path for compiled assembly patch
OutFile = examples/camera/patch_compiled.asm
```
