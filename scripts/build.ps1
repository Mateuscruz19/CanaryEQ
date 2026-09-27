param(
    [ValidateSet("debug", "release")]
    [string]$Preset = "debug",
    [switch]$Test
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

if (-not (Get-Command cl -ErrorAction SilentlyContinue)) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    & "$vsPath\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null
}

Push-Location $root
try {
    cmake --preset $Preset
    if ($LASTEXITCODE) { exit $LASTEXITCODE }
    cmake --build --preset $Preset
    if ($LASTEXITCODE) { exit $LASTEXITCODE }
    if ($Test) {
        ctest --preset $Preset
        if ($LASTEXITCODE) { exit $LASTEXITCODE }
    }
}
finally {
    Pop-Location
}
