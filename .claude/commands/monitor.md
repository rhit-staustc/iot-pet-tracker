Capture serial output from the board for 30 seconds and display the results.

Run the monitor capture script:
```
powershell -ExecutionPolicy Bypass -File ./scripts/monitor-capture.ps1 -DurationSeconds 30
```

If the user specifies a COM port (e.g. "monitor COM4"), pass `-ComPort COM4`. If they specify a duration (e.g. "monitor for 60 seconds"), pass `-DurationSeconds 60`. Defaults are COM3 and 30 seconds.

After the script finishes, read and display the captured output file (e.g. `monitor_*.txt`).
