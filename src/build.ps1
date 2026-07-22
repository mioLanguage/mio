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
    $found = Get-ChildItem "$LLVM" -Recurse -File -Filter "IRBuilder.h" -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match "llvm\\IR\\IRBuilder.h" } |
        Select-Object -First 1
    if ($found) {
        $INC = Split-Path -Parent (Split-Path -Parent $found.FullName)
        Write-Host "Found LLVM include directory: $INC"
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
$libs = $libs | Where-Object { $_ -notmatch 'lldb' -and $_ -notmatch 'clang' -and $_ -notmatch 'LLVM-C' -and $_ -notmatch '^LTO$' -and $_ -notmatch '^Remarks$' -and $_ -notmatch 'LLVMWindowsManifest' }

Write-Host "Building mioc.exe..."
$useMsvcLink = ($env:PROCESSOR_ARCHITECTURE -eq "ARM64")
if ($useMsvcLink) {
    Write-Host "ARM64: using MSVC link.exe to handle LLVM LTO bitcode"
    # Find link.exe
    $vsPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
    $linkExe = ""
    if ($vsPath) {
        $linkCandidates = @(
            "$vsPath\VC\Tools\MSVC\*\bin\Hostx64\arm64\link.exe",
            "$vsPath\VC\Tools\MSVC\*\bin\Hostx64\x64\link.exe"
        )
        foreach ($pattern in $linkCandidates) {
            $found = Get-ChildItem $pattern -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($found) { $linkExe = $found.FullName; break }
        }
    }
    if (-not $linkExe) { Write-Host "error: link.exe not found"; exit 1 }
    Write-Host "Using linker: $linkExe"
    # Compile to .obj
    & $CXX "-std=c++17" "-I$INC" "-L$LIB" "$SRC\main.cpp" "-c" "-o" "$BIN\mioc.obj" @($libs | ForEach-Object { "-l$_" })
    if ($LASTEXITCODE -ne 0) { Write-Host "Compilation failed"; exit 1 }
    # Find Windows SDK lib
    $winSdkLib = ""
    foreach ($pattern in @("$env:ProgramFiles (x86)\Windows Kits\10\Lib\*\um\arm64", "$env:ProgramFiles (x86)\Windows Kits\10\Lib\*\um\x64")) {
        $found = Get-ChildItem $pattern -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($found) { $winSdkLib = $found.FullName; break }
    }
    if (-not $winSdkLib) { Write-Host "error: Windows SDK lib not found"; exit 1 }
    # Find UCRT lib
    $ucrtLib = ""
    foreach ($pattern in @("$env:ProgramFiles (x86)\Windows Kits\10\Lib\*\ucrt\arm64", "$env:ProgramFiles (x86)\Windows Kits\10\Lib\*\ucrt\x64")) {
        $found = Get-ChildItem $pattern -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($found) { $ucrtLib = $found.FullName; break }
    }
    # Find MSVC lib
    $msvcLibPath = ""
    if ($vsPath) {
        $found = Get-ChildItem "$vsPath\VC\Tools\MSVC\*\lib\arm64" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($found) { $msvcLibPath = $found.FullName }
    }
    if (-not $msvcLibPath) { Write-Host "error: MSVC lib not found"; exit 1 }
    # Link
    $libArgs = @("/OUT:$BIN\mioc.exe", "$BIN\mioc.obj", "/LIBPATH:$LIB", "/LIBPATH:$winSdkLib", "/LIBPATH:$msvcLibPath")
    if ($ucrtLib) { $libArgs += "/LIBPATH:$ucrtLib" }
    $libArgs += ($libs | ForEach-Object { "$_.lib" }) + @("$SRC\libxml2_stub.lib", "ntdll.lib", "advapi32.lib", "kernel32.lib", "user32.lib", "shell32.lib", "/FORCE:MULTIPLE")
    & $linkExe @libArgs
} else {
    $clangArgs = @(
        "-std=c++17",
        "-I", "$INC",
        "-L", "$LIB",
        "$SRC\main.cpp",
        "-o", "$BIN\mioc.exe",
        "-fno-lto",
        "-Wl,/FORCE:MULTIPLE",
        "-Wl,/LTCG:OFF"
    )
    $clangArgs += ($libs | ForEach-Object { "-l$_" })
    $clangArgs += @(
        "$SRC\libxml2_stub.lib",
        "-lntdll",
        "-ladvapi32"
    )
    & $CXX @clangArgs
}

if ($LASTEXITCODE -eq 0) {
    Write-Host "Build successful: $BIN\mioc.exe"
    # Bundle MSVC runtime DLLs for machines without VC++ Redistributable
    Write-Host "Bundling MSVC runtime DLLs..."
    $deps = & "$LLVM\bin\llvm-objdump.exe" -p "$BIN\mioc.exe" 2>$null | Select-String "DLL Name:" | ForEach-Object { $_ -replace ".*DLL Name: ", "" }
    $msvcDlls = @("vcruntime140.dll", "vcruntime140_1.dll", "msvcp140.dll", "msvcp140_1.dll", "msvcp140_2.dll", "concrt140.dll")
    foreach ($dll in $msvcDlls) {
        if ($dll -in $deps) {
            $src = Join-Path $env:SystemRoot "System32\$dll"
            if (Test-Path $src) {
                Copy-Item $src "$BIN\" -Force
                Write-Host "  Bundled $dll"
            }
        }
    }
} else {
    Write-Host "Build failed with exit code $LASTEXITCODE"
}