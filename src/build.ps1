$SRC = Split-Path -Parent $MyInvocation.MyCommand.Path
$ROOT = Split-Path -Parent $SRC
$BIN = "$ROOT\bin"
if (-not (Test-Path $BIN)) { New-Item -ItemType Directory -Path $BIN -Force | Out-Null }

$LLVM = "$SRC\llvm-project\"
if (-not (Test-Path "$LLVM\bin\clang++.exe")) {
    $LLVM = $env:LLVM_DIR
}
if (-not $LLVM -or -not (Test-Path $LLVM)) {
    Write-Host "Error: LLVM not found. Set LLVM_DIR environment variable or edit this script."
    Write-Host "Example: `$env:LLVM_DIR = 'C:\Program Files\LLVM'"
    exit 1
}

$CXX = "$LLVM\bin\clang++.exe"
$CC = "$LLVM\bin\clang.exe"
$INC = "$LLVM\include"
$LIB = "$LLVM\lib"

# Chocolatey's llvm package may not install the include directory.
# Search for it under the LLVM tree.
if (-not (Test-Path "$INC\llvm\IR\IRBuilder.h")) {
    $found = Get-ChildItem "$LLVM" -Recurse -Directory -Filter "IR" -ErrorAction SilentlyContinue |
        Where-Object { Test-Path "$($_.FullName)\IRBuilder.h" } |
        Select-Object -First 1
    if ($found) {
        $INC = Split-Path -Parent $found.FullName
        Write-Host "Found include at: $INC"
    } else {
        Write-Host "Warning: LLVM include directory not found"
    }
}

if (-not (Test-Path "$INC\llvm\IR\IRBuilder.h")) {
    Write-Host "Error: LLVM headers not found. Please install LLVM with development headers."
    Write-Host "  choco install llvm --version=21.1.0"
    exit 1
}

Write-Host "Building libxml2 stub..."
& $CXX -c "$SRC\libxml2_stub.cpp" -o "$SRC\libxml2_stub.obj"
if ($LASTEXITCODE -ne 0) {
    Write-Host "Stub compilation failed"
    exit 1
}
$LIB_EXE = "$LLVM\bin\llvm-lib.exe"
if (-not (Test-Path $LIB_EXE)) {
    $LIB_EXE = "$LLVM\bin\lib.exe"
}
& $LIB_EXE /out:"$SRC\libxml2_stub.lib" "$SRC\libxml2_stub.obj"
if ($LASTEXITCODE -ne 0) {
    Write-Host "Stub archiving failed"
    exit 1
}
Remove-Item "$SRC\libxml2_stub.obj" -Force -ErrorAction SilentlyContinue

$libs = Get-ChildItem "$LIB\*.lib" | ForEach-Object { $_.BaseName }

Write-Host "Building mioc.exe..."
$clangArgs = @(
    "-std=c++17",
    "-I", "$INC",
    "-L", "$LIB",
    "$SRC\main.cpp",
    "-o", "$BIN\mioc.exe",
    "-Wl,/FORCE:MULTIPLE"
)
$clangArgs += ($libs | ForEach-Object { "-l$_" })
$clangArgs += @(
    "$SRC\libxml2_stub.lib",
    "-Wl,/DEFAULTLIB:ucrt",
    "-Wl,/DEFAULTLIB:libcmt",
    "-Wl,/nodefaultlib:msvcrt",
    "-lntdll",
    "-ladvapi32"
)
& $CXX @clangArgs

if ($LASTEXITCODE -eq 0) {
    Write-Host "Build successful: $BIN\mioc.exe"
} else {
    Write-Host "Build failed with exit code $LASTEXITCODE"
}