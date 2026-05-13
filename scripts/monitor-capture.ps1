param(
    [string]$ComPort = "COM3",
    [int]$BaudRate = 115200,
    [int]$DurationSeconds = 20
)

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$outputFile = Join-Path (Get-Location) "monitor_${timestamp}.txt"

Write-Host "Capturing serial output from $ComPort at $BaudRate baud for ${DurationSeconds}s..."
Write-Host "Output: $outputFile"

$port = New-Object System.IO.Ports.SerialPort $ComPort, $BaudRate, None, 8, One
$port.ReadTimeout = 500
$port.Open()

$lines = @()
$start = Get-Date
while ((Get-Date) - $start -lt [TimeSpan]::FromSeconds($DurationSeconds)) {
    try {
        $line = $port.ReadLine()
        $lines += $line
    } catch [System.TimeoutException] {}
}

$port.Close()
$lines | Out-File -FilePath $outputFile -Encoding utf8

Write-Host "Done. Saved to: $outputFile ($($lines.Count) lines)"
