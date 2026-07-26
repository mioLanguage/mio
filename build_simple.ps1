$ErrorActionPreference = "Stop"
$env:LLVM_DIR = "D:\github\clang+llvm-22.1.8-x86_64-pc-windows-msvc"
$CXX = "$env:LLVM_DIR\bin\clang++.exe"
$INC = "$env:LLVM_DIR\include"
$LIB = "$env:LLVM_DIR\lib"
$SRC = "f:\github\mio\src"
$BIN = "f:\github\mio\bin"

Write-Host "Building mioc.exe..."
& $CXX -std=c++17 -I $INC -L $LIB $SRC\main.cpp -o $BIN\mioc.exe -fno-lto "-Wl,/FORCE:MULTIPLE" "-Wl,/LTCG:OFF" $SRC\libxml2_stub.lib -lntdll -ladvapi32 2>&1 | Out-File -FilePath "f:\github\mio\build_log.txt" -Encoding UTF8
$rc = $LASTEXITCODE
Write-Host "Exit code: $rc"
Get-Content "f:\github\mio\build_log.txt"
exit $rc