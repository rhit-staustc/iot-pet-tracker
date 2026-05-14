$stateFile = "$PSScriptRoot\.flash-state"
$trackerFile = "Production/Tracker-Node/Tracker-Node.ino"
$baseFile = "Production/Base-Station/Base-Station.ino"

function Get-Hash($path) {
    if (Test-Path $path) { return (Get-FileHash $path -Algorithm MD5).Hash }
    return $null
}

$state = @{}
if (Test-Path $stateFile) {
    Get-Content $stateFile | ForEach-Object {
        $parts = $_ -split '=', 2
        if ($parts.Count -eq 2) { $state[$parts[0]] = $parts[1] }
    }
}

$trackerHash = Get-Hash $trackerFile
$baseHash    = Get-Hash $baseFile

$trackerChanged = $trackerHash -and ($trackerHash -ne $state['tracker'])
$baseChanged    = $baseHash    -and ($baseHash    -ne $state['base'])

if (-not $trackerChanged -and -not $baseChanged) { exit 0 }

if ($trackerChanged) {
    Write-Host "Tracker Node firmware changed - compiling and flashing..."
    arduino-cli compile --fqbn Heltec-esp32:esp32:heltec_wifi_lora_32_V4:CDCOnBoot=cdc Production/Tracker-Node
    if ($LASTEXITCODE -eq 0) {
        arduino-cli upload --fqbn Heltec-esp32:esp32:heltec_wifi_lora_32_V4:CDCOnBoot=cdc -p COM3 Production/Tracker-Node
        if ($LASTEXITCODE -eq 0) {
            $state['tracker'] = $trackerHash
            powershell -ExecutionPolicy Bypass -File ./scripts/monitor-capture.ps1 -DurationSeconds 30
        }
    }
}

if ($baseChanged) {
    Write-Host "Base Station firmware changed - compiling and flashing..."
    arduino-cli compile --fqbn Heltec-esp32:esp32:heltec_wifi_lora_32_V4:CDCOnBoot=cdc Production/Base-Station
    if ($LASTEXITCODE -eq 0) {
        arduino-cli upload --fqbn Heltec-esp32:esp32:heltec_wifi_lora_32_V4:CDCOnBoot=cdc -p COM3 Production/Base-Station
        if ($LASTEXITCODE -eq 0) {
            $state['base'] = $baseHash
            powershell -ExecutionPolicy Bypass -File ./scripts/monitor-capture.ps1 -DurationSeconds 30
        }
    }
}

$state.GetEnumerator() | ForEach-Object { "$($_.Key)=$($_.Value)" } | Set-Content $stateFile
