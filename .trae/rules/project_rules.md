# Mio Project Rules

## Build

### LLVM Path
LLVM is installed at:
```
D:\github\clang+llvm-22.1.8-x86_64-pc-windows-msvc
```

### Build Command
```powershell
$env:LLVM_DIR = "D:\github\clang+llvm-22.1.8-x86_64-pc-windows-msvc"
.\src\build.ps1
```

The compiler binary is at `bin\mioc.exe`.

### Test Command
```powershell
.\bin\mioc.exe test_minimal.mio
.\test_minimal.exe
```

## Code Style
- Use tab indentation, no spaces around operators
- Comments use `#` not `//`
- Function declarations use `i32 main()` not `fn main()`