# Bundle CRT libs for linking user programs on Windows
$crtDir = "$ROOT\lib\windows"
New-Item -ItemType Directory -Path $crtDir -Force | Out-Null
$bundled = @()
# Find Windows SDK lib paths
$sdkLibs = @()
$kitsRoot = "${env:ProgramFiles(x86)}\Windows Kits\10\"
if (Test-Path $kitsRoot) {
	$sdkVersions = Get-ChildItem "$kitsRoot\Lib" -Directory | Sort-Object Name -Descending
	foreach ($ver in $sdkVersions) {
		$um = "$($ver.FullName)\um\x64"
		$ucrt = "$($ver.FullName)\ucrt\x64"
		if ((Test-Path $um) -and (Test-Path $ucrt)) {
			$sdkLibs = @($um, $ucrt)
			break
		}
	}
}
# Find MSVC lib path
$msvcLib = ""
try {
	$vsPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
	if ($vsPath) {
		$msvcDirs = Get-ChildItem "$vsPath\VC\Tools\MSVC\*\lib\x64" -ErrorAction SilentlyContinue | Sort-Object Name -Descending
		if ($msvcDirs) { $msvcLib = $msvcDirs[0].FullName }
	}
} catch {}
# Dynamic CRT libs (small, for default mode)
$dynamicLibs = @("msvcrt.lib", "ucrt.lib", "legacy_stdio_definitions.lib", "kernel32.lib")
foreach ($lib in $dynamicLibs) {
	foreach ($dir in $sdkLibs) {
		$p = Join-Path $dir $lib
		if (Test-Path $p) { Copy-Item $p $crtDir -Force; $bundled += $lib; break }
	}
	if ($lib -notin $bundled -and $msvcLib) {
		$p = Join-Path $msvcLib $lib
		if (Test-Path $p) { Copy-Item $p $crtDir -Force; $bundled += $lib }
	}
}
# Static CRT libs (large, for -static mode)
if ($msvcLib) {
	$staticLibs = @("libcmt.lib", "libucrt.lib", "libvcruntime.lib")
	foreach ($lib in $staticLibs) {
		$p = Join-Path $msvcLib $lib
		if (Test-Path $p) { Copy-Item $p $crtDir -Force; $bundled += $lib }
	}
}
if ($bundled.Count -gt 0) {
	Write-Host "  Bundled: $($bundled -join ', ')"
} else {
	Write-Host "  Warning: no CRT libs bundled"
}