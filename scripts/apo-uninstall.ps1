$ErrorActionPreference = "Stop"

$clsid = "{B22EF1FC-57E7-4E77-AC91-1D232BA20CF2}"
$installDir = Join-Path $env:ProgramFiles "CanaryEQ\apo"
$stateFile = Join-Path $installDir "install-state.json"

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    throw "Run this script from an administrator PowerShell."
}

if (Test-Path $stateFile) {
    $state = Get-Content $stateFile -Raw | ConvertFrom-Json
    $fxWritable = [Microsoft.Win32.Registry]::LocalMachine.OpenSubKey(($state.EndpointKey -replace "^HKLM:\\", ""),
        [Microsoft.Win32.RegistryKeyPermissionCheck]::ReadWriteSubTree,
        [System.Security.AccessControl.RegistryRights]"SetValue, QueryValues")
    try {
        foreach ($entry in $state.Previous.PSObject.Properties) {
            $name = $entry.Name
            $previous = $entry.Value
            if ($previous.Existed) {
                $value = $previous.Value
                if ($previous.Kind -eq "MultiString") { $value = [string[]]@($value) }
                if ($previous.Kind -eq "DWord") { $value = [int]$value }
                $fxWritable.SetValue($name, $value, [Microsoft.Win32.RegistryValueKind]$previous.Kind)
            }
            else {
                $fxWritable.DeleteValue($name, $false)
            }
        }
    }
    finally {
        $fxWritable.Close()
    }
    Write-Host "Restored the original effect settings of the device."
}
else {
    Write-Host "No install state found; removing the registration only."
}

Remove-Item -Path "HKLM:\SOFTWARE\Classes\AudioEngine\AudioProcessingObjects\$clsid" -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -Path "HKLM:\SOFTWARE\Classes\CLSID\$clsid" -Recurse -Force -ErrorAction SilentlyContinue

Restart-Service audiosrv -Force
Start-Sleep -Seconds 2

Remove-Item (Join-Path $installDir "canary_apo.dll") -Force -ErrorAction SilentlyContinue
Remove-Item $stateFile -Force -ErrorAction SilentlyContinue
Write-Host "CanaryEQ APO removed. The registry backup stays in $installDir in case you need it."
