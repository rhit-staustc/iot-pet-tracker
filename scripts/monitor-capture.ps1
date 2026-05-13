param(
    [string]$ComPort = "COM3",
    [int]$BaudRate = 115200,
    [int]$DurationSeconds = 30
)

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$outputFile = "monitor_${timestamp}.txt"

Write-Host "Capturing serial output from $ComPort at $BaudRate baud for ${DurationSeconds}s..."
Write-Host "Output: $outputFile"

$job = Start-Job -ScriptBlock {
    param($port, $baud, $outFile)
    & arduino-cli monitor -p $port --config "baudrate=$baud" 2>&1 | Out-File -FilePath $outFile -Encoding utf8
} -ArgumentList $ComPort, $BaudRate, $outputFile

Start-Sleep -Seconds $DurationSeconds
Stop-Job $job
Remove-Job $job

Write-Host "Done. Saved to: $outputFile"
