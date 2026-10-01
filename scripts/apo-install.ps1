param(
    [string]$EndpointId,
    [switch]$DisableEffectPack,
    [string]$DllPath = (Join-Path $PSScriptRoot "..\build\release\apo\canary_apo.dll")
)

$ErrorActionPreference = "Stop"

$clsid = "{B22EF1FC-57E7-4E77-AC91-1D232BA20CF2}"
$devices = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio"
$friendlyName = "{a45c254e-df1c-4efd-8020-67d146a850e0},2"
$interfaceName = "{b3f8fa53-0004-438e-9003-51a46e139bfc},6"
$installDir = Join-Path $env:ProgramFiles "CanaryEQ\apo"
$stateFile = Join-Path $installDir "install-state.json"
$touched = [ordered]@{
    "{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},5" = @{ Kind = "String"; Value = $clsid }
    "{d3993a3f-99c2-4402-b5ec-a92a0367664b},5" = @{ Kind = "MultiString"; Value = @("{C18E2F7E-933D-4965-B7D1-1EEF228D2AF3}") }
    "{1da5d803-d492-4edd-8c23-e0c0ffee7f0e},5" = @{ Kind = "DWord"; Value = 0 }
}
if ($DisableEffectPack) {
    $touched["{9e6136e0-57ab-4949-b57a-3627be142855},100"] = @{ Kind = "Delete" }
}

function Show-Devices {
    Write-Host "Active audio devices:"
    foreach ($flow in "Capture", "Render") {
        Get-ChildItem "$devices\$flow" | Where-Object { (Get-ItemProperty $_.PSPath).DeviceState -eq 1 } | ForEach-Object {
            $properties = Get-ItemProperty "$($_.PSPath)\Properties"
            Write-Host ("  {0,-8} {1}  {2} ({3})" -f $flow, $_.PSChildName, $properties.$friendlyName, $properties.$interfaceName)
        }
    }
}

if (-not $EndpointId) {
    Show-Devices
    Write-Host "`nUsage: apo-install.ps1 -EndpointId <id from the list>"
    exit 1
}

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    throw "Run this script from an administrator PowerShell."
}

if (Test-Path $stateFile) {
    throw "CanaryEQ APO is already installed. Run apo-uninstall.ps1 first."
}

$endpoint = @("Capture", "Render") | ForEach-Object { "$devices\$_\$EndpointId" } | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $endpoint) {
    throw "Endpoint $EndpointId not found."
}
$fxKey = "$endpoint\FxProperties"
if (-not (Test-Path $fxKey)) {
    throw "Endpoint has no FxProperties key; this test only supports endpoints that already have one."
}
$DllPath = (Resolve-Path $DllPath).Path

New-Item -ItemType Directory -Force $installDir | Out-Null
$logDir = Join-Path $env:ProgramData "CanaryEQ"
New-Item -ItemType Directory -Force $logDir | Out-Null
& icacls.exe $logDir /grant "*S-1-5-19:(OI)(CI)M" | Out-Null
$regPath = $fxKey -replace "^HKLM:\\", "HKLM\"
& reg.exe export $regPath (Join-Path $installDir "backup-FxProperties.reg") /y | Out-Null

$fx = Get-ItemProperty $fxKey
$previous = [ordered]@{}
foreach ($name in $touched.Keys) {
    $existing = $fx.PSObject.Properties[$name]
    if ($existing) {
        $kind = (Get-Item $fxKey).GetValueKind($name).ToString()
        $previous[$name] = @{ Existed = $true; Kind = $kind; Value = $existing.Value }
    }
    else {
        $previous[$name] = @{ Existed = $false }
    }
}

$dllTarget = Join-Path $installDir "canary_apo.dll"
Copy-Item $DllPath $dllTarget -Force

$comKey = "HKLM:\SOFTWARE\Classes\CLSID\$clsid"
New-Item -Path "$comKey\InprocServer32" -Force | Out-Null
Set-ItemProperty -Path $comKey -Name "(default)" -Value "CanaryEQ APO"
Set-ItemProperty -Path "$comKey\InprocServer32" -Name "(default)" -Value $dllTarget
New-ItemProperty -Path "$comKey\InprocServer32" -Name "ThreadingModel" -Value "Both" -PropertyType String -Force | Out-Null

$apoKey = "HKLM:\SOFTWARE\Classes\AudioEngine\AudioProcessingObjects\$clsid"
New-Item -Path $apoKey -Force | Out-Null
$apoValues = [ordered]@{
    FriendlyName = @("String", "CanaryEQ")
    Copyright = @("String", "Copyright (c) 2026 Mateus Cruz")
    MajorVersion = @("DWord", 0)
    MinorVersion = @("DWord", 1)
    Flags = @("DWord", 13)
    MinInputConnections = @("DWord", 1)
    MaxInputConnections = @("DWord", 1)
    MinOutputConnections = @("DWord", 1)
    MaxOutputConnections = @("DWord", 1)
    MaxInstances = @("DWord", [uint32]::MaxValue)
    NumAPOInterfaces = @("DWord", 1)
    APOInterface0 = @("String", "{FD7F2B29-24D0-4B5C-B177-592C39F9CA10}")
}
foreach ($name in $apoValues.Keys) {
    $kind, $value = $apoValues[$name]
    if ($kind -eq "DWord") { $value = [BitConverter]::ToInt32([BitConverter]::GetBytes([uint32]$value), 0) }
    New-ItemProperty -Path $apoKey -Name $name -Value $value -PropertyType $kind -Force | Out-Null
}

$fxWritable = [Microsoft.Win32.Registry]::LocalMachine.OpenSubKey(($fxKey -replace "^HKLM:\\", ""),
    [Microsoft.Win32.RegistryKeyPermissionCheck]::ReadWriteSubTree,
    [System.Security.AccessControl.RegistryRights]"SetValue, QueryValues")
try {
    foreach ($name in $touched.Keys) {
        $value = $touched[$name].Value
        if ($touched[$name].Kind -eq "Delete") {
            $fxWritable.DeleteValue($name, $false)
            continue
        }
        if ($touched[$name].Kind -eq "MultiString") { $value = [string[]]$value }
        $fxWritable.SetValue($name, $value, [Microsoft.Win32.RegistryValueKind]$touched[$name].Kind)
    }
}
finally {
    $fxWritable.Close()
}

[ordered]@{ EndpointKey = $fxKey; Previous = $previous } | ConvertTo-Json -Depth 5 | Set-Content -Encoding utf8 $stateFile

Restart-Service audiosrv -Force

$properties = Get-ItemProperty "$endpoint\Properties"
$deviceName = "$($properties.$friendlyName) ($($properties.$interfaceName))"
Write-Host "CanaryEQ APO installed on: $deviceName"
Write-Host "Backup of the original settings: $installDir"
Write-Host "Diagnostic log: $logDir\apo-log.txt"
Write-Host "To undo: apo-uninstall.ps1 (from an administrator PowerShell)"
