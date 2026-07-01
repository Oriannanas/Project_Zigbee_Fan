# Flash and Pair Checklist

- Connect the ESP32-C6 SuperMini by USB and note its COM port.
- `COMx` means your actual serial port (for example `COM5` or `COM11`).
- On Windows, find it with PowerShell: `Get-CimInstance Win32_SerialPort | Select-Object DeviceID, Description`.
- In the project folder, set target once: `idf.py set-target esp32c6`.
- Optional but recommended for first flash: `idf.py -p COMx erase-flash`.
- Build, flash, and open logs: `idf.py -p COMx flash monitor`.
- In Zigbee2MQTT/Home Assistant, put the coordinator in pairing mode.
- Power-cycle or reset the ESP and wait for it to join.
- If it was paired before, remove/re-interview it so new capabilities are detected.
- In Home Assistant, use the discovered light entity (`state` + `brightness`).
