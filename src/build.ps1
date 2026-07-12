$SRC = Split-Path -Parent $MyInvocation.MyCommand.Path

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
& $CC -c "$SRC\libxml2_stub.c" -o "$SRC\libxml2_stub.obj"
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

$libs = @(
    "LLVMCore", "LLVMSupport", "LLVMTargetParser", "LLVMBinaryFormat",
    "LLVMRemarks", "LLVMBitstreamReader", "LLVMDemangle", "LLVMTransformUtils",
    "LLVMAnalysis", "LLVMProfileData", "LLVMDebugInfoDWARF", "LLVMDebugInfoMSF",
    "LLVMDebugInfoCodeView", "LLVMDebugInfoPDB", "LLVMDebugInfoBTF", "LLVMDebugInfoGSYM",
    "LLVMDebugInfoDWARFLowLevel", "LLVMObject", "LLVMMCParser", "LLVMIRReader",
    "LLVMTextAPI", "LLVMMC", "LLVMBitReader", "LLVMBitWriter", "LLVMAsmParser",
    "LLVMCodeGen", "LLVMCodeGenTypes", "LLVMScalarOpts", "LLVMInstCombine",
    "LLVMAggressiveInstCombine", "LLVMipo", "LLVMInstrumentation", "LLVMVectorize",
    "LLVMObjCARCOpts", "LLVMCoroutines", "LLVMCFGuard", "LLVMHipStdPar", "LLVMLinker",
    "LLVMIRPrinter", "LLVMSandboxIR", "LLVMPasses", "LLVMExtensions", "LLVMTarget",
    "LLVMSelectionDAG", "LLVMGlobalISel", "LLVMAsmPrinter", "LLVMMCDisassembler",
    "LLVMExecutionEngine", "LLVMRuntimeDyld", "LLVMJITLink", "LLVMOrcJIT",
    "LLVMOrcTargetProcess", "LLVMOrcShared", "LLVMOrcDebugging", "LLVMX86CodeGen",
    "LLVMX86Desc", "LLVMX86Info", "LLVMX86AsmParser", "LLVMX86Disassembler",
    "LLVMCGData", "LLVMFrontendDriver", "LLVMFrontendOpenMP", "LLVMFrontendOffloading",
    "LLVMFrontendDirective", "LLVMFrontendAtomic", "LLVMFrontendHLSL", "LLVMMCJIT",
    "LLVMInterpreter", "LLVMSymbolize", "LLVMCoverage", "LLVMXRay", "LLVMMCA",
    "LLVMDWARFLinker", "LLVMDWARFLinkerClassic", "LLVMDWARFLinkerParallel", "LLVMDWP",
    "LLVMObjectYAML", "LLVMOption", "LLVMWindowsDriver", "LLVMWindowsManifest",
    "LLVMLTO", "LLVMDTLTO", "LLVMTableGen", "LLVMFileCheck", "LLVMMIRParser",
    "LLVMInterfaceStub", "LLVMLineEditor", "LLVMSupportLSP", "LLVMABI", "LLVMCAS",
    "LLVMTelemetry", "LLVMDiff", "LLVMCFIVerify", "LLVMExegesis", "LLVMExegesisX86",
    "LLVMDebuginfod", "LLVMDebugInfoLogicalView", "LLVMObjCopy", "LLVMDlltoolDriver",
    "LLVMLibDriver", "LLVMPlugins", "LLVMOptDriver", "LLVMTextAPIBinaryReader",
    "LLVMTableGenBasic", "LLVMTableGenCommon", "LLVMNVPTXCodeGen", "LLVMNVPTXDesc",
    "LLVMNVPTXInfo", "LLVMARMCodeGen", "LLVMARMDesc", "LLVMARMInfo", "LLVMARMUtils",
    "LLVMARMAsmParser", "LLVMARMDisassembler", "LLVMAArch64CodeGen", "LLVMAArch64Desc",
    "LLVMAArch64Info", "LLVMAArch64Utils", "LLVMAArch64AsmParser", "LLVMAArch64Disassembler",
    "LLVMRISCVCodeGen", "LLVMRISCVDesc", "LLVMRISCVInfo", "LLVMRISCVAsmParser",
    "LLVMRISCVDisassembler", "LLVMRISCVTargetMCA", "LLVMWebAssemblyCodeGen",
    "LLVMWebAssemblyDesc", "LLVMWebAssemblyInfo", "LLVMWebAssemblyUtils",
    "LLVMWebAssemblyAsmParser", "LLVMWebAssemblyDisassembler", "LLVMBPFCodeGen",
    "LLVMBPFDesc", "LLVMBPFInfo", "LLVMBPFAsmParser", "LLVMBPFDisassembler",
    "LLVMExegesisRISCV", "LLVMExegesisAArch64", "LLVMX86TargetMCA", "LLVMFuzzMutate",
    "LLVMFuzzerCLI", "LLVMDWARFCFIChecker",
    "lldCommon", "lldCOFF", "lldELF", "lldMachO"
)

$libFlags = ($libs | ForEach-Object { "-l$_" }) -join " "

Write-Host "Building mioc.exe..."
$cmd = "& `"$CXX`" -std=c++17 -I `"$INC`" -L `"$LIB`" `"$SRC\main.cpp`" -o `"$SRC\mioc.exe`" $libFlags `"$SRC\libxml2_stub.lib`" -lntdll -ladvapi32"
Write-Host $cmd
Invoke-Expression $cmd

if ($LASTEXITCODE -eq 0) {
    Write-Host "Build successful: $SRC\mioc.exe"
} else {
    Write-Host "Build failed with exit code $LASTEXITCODE"
}