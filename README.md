# 🦾 6-DOF Robot Arm Hotkey & Web Controller

A high-performance robotic arm control suite featuring zero-glitch ESP32 firmware, linear slew motion profiling, browser-based hotkey control (USB WebSerial & WiFi WebSockets), and complete 3D CAD models.

---

## 🌟 Highlights

- **Zero-Glitch Direct PCA9685 Driver**: Custom bare-metal I2C driver eliminating comparator glitches, phase-staggered pulse timing to prevent peak power surges, and automatic register recovery without resetting joint positions.
- **Linear Slew Profile**: Smooth, non-blocking motion ramping at user-selectable speeds (20°/s to 120°/s) to protect gearboxes and eliminate jerks.
- **Dual Connectivity Modes**:
  - **USB Mode**: Direct low-latency WebSerial communication via Chrome/Edge/Opera.
  - **WiFi AP Mode**: Self-hosted Access Point (`RobotArm-Hotkeys`) with embedded WebSockets server and mobile-friendly web controller served directly from ESP32 PROGMEM.
- **Full Keyboard Hotkeys**: Control all 6 axes + claw simultaneously with configurable step sizes and continuous drive.
- **Complete CAD Package**: Includes 12 universal 3D-printable `.STEP` models and the SolidWorks master assembly (`RobotArm_Assembly.SLDASM`).

---

## 📂 Repository Structure

```text
├── cad/
│   ├── assembly/
│   │   └── RobotArm_Assembly.SLDASM    # SolidWorks master assembly
│   └── step/                           # 12 3D-printable STEP models
│       ├── Base.STEP
│       ├── Claw.STEP
│       ├── Claw 1.STEP
│       ├── Claw Holder.STEP
│       ├── Claw Link.STEP
│       ├── Gear.STEP
│       ├── Gear 1.STEP
│       ├── Rod 0.STEP
│       ├── Rod 1.STEP
│       ├── Rod2.STEP
│       ├── Shoulder.STEP
│       └── Wrist.STEP
├── firmware/
│   ├── robot_arm_v3/                   # USB WebSerial firmware
│   │   └── robot_arm_v3.ino
│   └── robot_arm_v3_wifi/              # WiFi AP + WebSockets firmware
│       ├── robot_arm_v3_wifi.ino
│       └── web_page.h
├── controller/
│   ├── servo_hotkeys_v2.html           # USB WebSerial controller UI
│   └── servo_hotkeys_wifi.html         # WiFi / WebSocket controller UI
├── start_hotkey_controller.bat         # 1-click launcher for USB mode
├── start_hotkey_wifi.bat               # 1-click launcher for WiFi mode
├── .gitignore
└── README.md
```

---

## ⚡ Hardware & Wiring

### Components
- **Microcontroller**: ESP32 Development Board (30-pin or 38-pin)
- **PWM Driver**: PCA9685 16-Channel 12-Bit I2C Servo Driver
- **Servos**: MG996R / MG995 (High-Torque for Base, Shoulder, Elbow) + SG90 (Wrist & Gripper)
- **Power Supply**: Dedicated 5V-6V 4A-10A DC Power Supply (connected to PCA9685 green terminal)

> ⚠️ **Warning**: Never power the servos from the ESP32 5V/3.3V pins. Always use a dedicated power supply for the PCA9685 screw terminal with common GND shared with the ESP32.

### Pinout
| ESP32 Pin | PCA9685 Pin | Description |
|-----------|-------------|-------------|
| **GPIO 21** | **SDA** | I2C Data Line |
| **GPIO 22** | **SCL** | I2C Clock Line |
| **3V3** | **VCC** | Logic Power (3.3V) |
| **GND** | **GND** | Common Ground |
| — | **V+ (Terminal)** | 5V - 6V DC High-Current Power |

### Servo Channels & Calibrated Defaults
| PCA9685 Channel | Joint Name | Min Limit | Max Limit | Home Angle | Typical Servo |
|:---------------:|------------|:---------:|:---------:|:----------:|:-------------:|
| **CH 15** | Base Rotation | 0° | 180° | **90°** | MG996R |
| **CH 14** | Shoulder Pitch | 10° | 170° | **44°** | MG996R |
| **CH 13** | Elbow Pitch | 10° | 170° | **130°** | MG996R |
| **CH 12** | Forearm Pitch | 10° | 170° | **90°** | MG996R / SG90 |
| **CH 11** | Wrist Roll | 0° | 180° | **90°** | SG90 |
| **CH 10** | Wrist Pitch | 10° | 170° | **90°** | SG90 |
| **CH 9** | Gripper Claw | 0° | 90° | **0°** (Open) | SG90 |

---

## ⌨️ Keyboard Hotkey Map

| Key Binding | Function | Direction / Action |
|:-----------:|----------|-------------------|
| <kbd>Q</kbd> / <kbd>A</kbd> | Base | Rotate Left / Right |
| <kbd>W</kbd> / <kbd>S</kbd> | Shoulder | Tilt Up / Down |
| <kbd>E</kbd> / <kbd>D</kbd> | Elbow | Tilt Up / Down |
| <kbd>R</kbd> / <kbd>F</kbd> | Forearm | Tilt Up / Down |
| <kbd>T</kbd> / <kbd>G</kbd> | Wrist Roll | Rotate CW / CCW |
| <kbd>Y</kbd> / <kbd>H</kbd> | Wrist Pitch | Tilt Up / Down |
| <kbd>Space</kbd> / <kbd>C</kbd> | Gripper | Close / Open Claw |
| <kbd>1</kbd> – <kbd>5</kbd> | Speed | Preset Speeds (1 = 20°/s, 3 = 80°/s, 5 = 120°/s) |
| <kbd>Home</kbd> / UI Button | Home Pose | Auto-align arm to default posture |
| <kbd>Esc</kbd> | **E-Stop** | Instantly freeze all joints |

---

## 🚀 Getting Started

### 1. Flash the ESP32 Firmware
1. Open the [Arduino IDE](https://www.arduino.cc/en/software).
2. Install the **esp32** board package (`Tools` > `Board` > `Boards Manager...`).
3. For WiFi mode, install the **WebSockets** library by Markus Sattler via Library Manager.
4. Select your ESP32 board and COM port.
5. Open either:
   - `firmware/robot_arm_v3/robot_arm_v3.ino` (for USB WebSerial)
   - `firmware/robot_arm_v3_wifi/robot_arm_v3_wifi.ino` (for WiFi WebSockets)
6. Click **Upload**.

### 2. Launch the Controller

#### Option A: USB WebSerial
1. Connect ESP32 to your PC via USB.
2. Double-click `start_hotkey_controller.bat` (opens `controller/servo_hotkeys_v2.html` in Chrome/Edge/Opera).
3. Click **Connect Serial**, select your ESP32 COM port (115200 baud).
4. Use your keyboard or on-screen sliders to control the arm.

#### Option B: Wireless WiFi AP
1. Power up the ESP32.
2. Connect your PC, tablet, or phone to the WiFi network:
   - **SSID**: `RobotArm-Hotkeys`
   - **Password**: `12345678`
3. Either:
   - Open your browser to `http://192.168.4.1`, or
   - Double-click `start_hotkey_wifi.bat` on PC and click **Connect WiFi**.

---

## 🖨️ 3D Printing & CAD

All 12 `.STEP` models in `cad/step/` can be directly imported into any modern slicer:
- **Recommended Material**: PLA+ or PETG
- **Infill**: 30% - 50% Gyroid or Grid (higher infill recommended for `Base.STEP`, `Shoulder.STEP`, and `Rod 0.STEP`)
- **Perimeters/Walls**: 4-5 walls for structural rigidity
- **Assembly CAD**: `cad/assembly/RobotArm_Assembly.SLDASM` can be opened in Dassault Systèmes SolidWorks.

---

## 📄 License

MIT License. Free to use, modify, and distribute for personal and commercial robotics projects.
