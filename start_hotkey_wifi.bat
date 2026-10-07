@echo off
echo ========================================================
echo   Robot Arm Hotkey Controller (WiFi Edition)
echo ========================================================
echo.
echo Wireless Options:
echo   1. Connect PC to ESP32 WiFi: "RobotArm-Hotkeys" (Pass: 12345678)
echo      Then click "Connect WiFi" in the controller (IP: 192.168.4.1).
echo.
echo   2. Or on any phone/tablet, connect to WiFi and open:
echo      http://192.168.4.1
echo.
start "" "%~dp0controller\servo_hotkeys_wifi.html"
