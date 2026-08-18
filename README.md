# ResQBot: Dual-Node Multi-Sensor Reconnaissance & Search Platform

## Overview

ResQBot is an integrated search and rescue reconnaissance platform designed for real-time human detection, thermal mapping, and remote navigation in hazardous or restricted environments. The system utilizes a dual-microcontroller architecture (ESP32-CAM and ESP32-Main) communicating over a local Wi-Fi network with a central Python desktop station. 

Key capabilities include:
* Real-time visual reconnaissance via ESP32-CAM streaming JPEG frames over HTTP POST.
* Computer vision human detection utilizing YOLO object detection (`person.pt`).
* Infrared thermal telemetry using an MLX90640 array sensor (768-pixel temperature grid) rendered as JET heatmaps.
* Differential propulsion drive utilizing L9110 motor drivers with software-controlled PWM ramping.
* Onboard status monitoring via SSD1306 OLED display and active acoustic buzzer alarms.
* Provisioning interface via captive Wi-Fi Access Point portals on both microcontrollers.
* An interactive web-based technical manual and troubleshooting suite.

---

## Repository Structure

```
resqbot-final/
├── 3D Parts/
│   ├── Compartment.stl          # 3D printable electronics compartment chassis
│   ├── Hinge.stl                # 3D printable door hinge assembly
│   ├── Latch.stl                # 3D printable mechanical enclosure latch
│   ├── Left Hull.stl            # 3D printable left hull section
│   ├── Right Hull.stl           # 3D printable right hull section
│   ├── 1.png                    # Assembly preview / render 1
│   ├── 2.png                    # Assembly preview / render 2
│   ├── 3.png                    # Assembly preview / render 3
│   └── 4.png                    # Assembly preview / render 4
├── esp32cam/
│   └── esp32cam.ino             # ESP32-CAM firmware (OV2640 camera capture, HTTP video uploader)
├── esp32main/
│   └── esp32main.ino            # ESP32-Main station firmware (MLX90640, SSD1306 OLED, L9110, Buzzer)
├── manual_web/
│   ├── index.html               # Web-based operations and technical manual interface
│   ├── styles.css               # Manual stylesheet
│   ├── script.js                # Interactive hotspots, zoom controls, and simulation logic
│   └── perfboard_view.png       # Annotated circuit wiring diagram image
├── python/
│   ├── app.py                   # Central Python desktop app (Flask HTTP server + Tkinter GUI)
│   └── person.pt                # Trained YOLO neural network weights for human detection
├── Project_Price_Quotation_ResqBot_Final.pdf  # Bill of materials and cost breakdown documentation
├── ResqBot Schematics.pdf        # Schematic electrical wiring diagrams
└── README.md                    # System documentation
```

---

## Hardware Subsystems

### 1. ESP32-CAM Node
* **Processor:** ESP32 Dual-Core 240 MHz MCU.
* **Sensor:** OV2640 Camera Module (VGA / QVGA resolutions).
* **Role:** Continuous capture of JPEG image frames transmitted via HTTP POST requests to `/upload_cam` on the central server.
* **Provisioning:** Hosts captive access point `ResQBot-CAM-AP` at `192.168.4.1` for configuring Wi-Fi credentials and target host server IP into non-volatile storage (NVS).

### 2. ESP32-Main Control Unit
* **Processor:** ESP32 Dual-Core 240 MHz MCU.
* **Thermal Sensor:** GY-MCU90640 (MLX90640 32x24 IR array) over UART (115200 baud, 1544-byte frame buffer).
* **Display:** 0.96-inch SSD1306 OLED (128x64 pixels, I2C bus at `0x3C`/`0x3D`).
* **Motor Driver:** L9110 Dual Channel H-Bridge Motor Driver supporting PWM speed ramping for Motor A and Motor B.
* **Alarm:** Active 5V Piezo Buzzer triggered on detection alerts or server requests.
* **Provisioning:** Hosts captive access point `ResQBot-Main-AP` at `192.168.4.1` for Wi-Fi and host IP configuration into NVS.

### 3. Power and Electrical Regulation
* **Primary Source:** 3S Li-ion Battery pack.
* **Voltage Regulation:** LM2596 DC-DC Buck Converter calibrated to 5.00V DC.
* **Charging Interface:** Integrated 3S Type-C charging circuit with status LED indication.
* **Switching Isolation:** Dual-position Rocker Switch toggling between System Power ON (Position I) and Charge Mode (Position O) to protect onboard microcontrollers during charging.

### 4. Structural Hull and Mechanical CAD
* STL files for full hull assembly are provided in the `3D Parts/` folder, including Left and Right Hulls, Electronics Compartment, Hinge, and Latch mechanisms.

---

## Software Architecture

### Central Desktop Control Station (`python/app.py`)
The primary operator interface combines a multi-threaded Flask HTTP server with a Tkinter dashboard interface:

1. **Flask API Endpoints:**
   * `POST /upload_cam`: Ingests binary JPEG camera frames from the ESP32-CAM.
   * `POST /upload_mlx`: Ingests 768-element floating-point temperature arrays from the ESP32-Main. Returns motor target speeds and active person detection counts.
   * `GET/POST /motor`: Provides query and command interfaces for differential motor channels.
   * `GET/POST /motorStop`: Issues immediate emergency stop commands to both motor channels.

2. **Computer Vision & Thermal Pipeline:**
   * **YOLO Person Detection:** Processes incoming visual frames against `person.pt` model weights, drawing bounding boxes and calculating total detected individuals.
   * **Thermal Heatmap Processing:** Normalizes raw temperature matrices and applies the OpenCV `COLORMAP_JET` transformation, displaying localized minimum, maximum, and average temperatures.

3. **WASD Steering & Speed Ramping Engine:**
   * Maps keyboard inputs (`W`, `A`, `S`, `D`, `Spacebar`) into differential motor targets for Forward, Reverse, Spin Left, Spin Right, and Emergency Stop operations.
   * Implements software ramping algorithms to ensure smooth acceleration and deceleration curves.

---

## Web Operations Manual (`manual_web/`)

The repository includes a web-based technical manual that can be rendered in any modern web browser. Features include:
* **System Architecture Breakdown:** Technical descriptions of hardware and software workflows.
* **Interactive Hardware Viewer:** Circuit diagram navigation with zoom, pan, and interactive hotspots describing pin connections.
* **Commissioning Timeline:** Step-by-step startup, calibration, provisioning, and charging procedures.
* **Filterable Troubleshooting Matrix:** Searchable resolution protocols for power, camera, thermal sensor, motor driver, display, and network issues.
* **Control Simulator:** Integrated keyboard steering and motor PWM output preview tool.

---

## Getting Started

### System Prerequisites
* **Python Environment:** Python 3.10 or higher.
* **Firmware Build Environment:** Arduino IDE with ESP32 board support packages (`esp32` by Espressif Systems).

### Python Dependencies Installation
Install required dependencies prior to launching the control station:
```bash
pip install ultralytics opencv-python pillow flask requests numpy
```

### Firmware Flashing
1. **ESP32-CAM Node:**
   * Open `esp32cam/esp32cam.ino` in Arduino IDE.
   * Select board: **AI Thinker ESP32-CAM**.
   * Enable PSRAM if supported by your module.
   * Flash firmware to the ESP32-CAM board.

2. **ESP32-Main Station:**
   * Open `esp32main/esp32main.ino` in Arduino IDE.
   * Select board: **ESP32 Dev Module**.
   * Required libraries: `Adafruit_GFX`, `Adafruit_SSD1306`, `Preferences`, `Wire`, `WiFi`, `HTTPClient`.
   * Flash firmware to the ESP32-Main board.

### Commissioning and Operation Workflow

1. **Power Supply Calibration:**
   * Set the Rocker Switch to Position O (Charge Mode).
   * Disconnect microcontrollers from the 5V rail.
   * Toggle Rocker Switch to Position I and verify LM2596 output reads exactly 5.00V DC using a multimeter.

2. **Launch Control Station Server:**
   * Navigate to the `python/` directory and run:
     ```bash
     python app.py
     ```
   * Record the host IP address displayed in the window title bar (e.g., `http://192.168.18.155:5000`).

3. **Network Provisioning:**
   * **ESP32-CAM:** Connect PC or mobile device to Wi-Fi AP `ResQBot-CAM-AP`. Open browser at `http://192.168.4.1`, input your Wi-Fi SSID, Password, and Server Host IP, then click **Connect & Continue Operations**.
   * **ESP32-Main:** Connect to Wi-Fi AP `ResQBot-Main-AP`. Open browser at `http://192.168.4.1`, input identical Wi-Fi SSID, Password, and Server Host IP, then click **Connect & Continue Operations**.

4. **Telemetry Verification and Control:**
   * Observe OLED status on the main node: verify Wi-Fi connection, Python app connection status, and MLX sensor readiness.
   * Monitor the desktop station GUI for video feeds, person bounding boxes, and thermal heatmaps.
   * Drive the robot using WASD keys or on-screen slider controls.

---

## Telemetry and API Endpoint Reference

| Endpoint | Method | Payload / Parameters | Description |
| :--- | :--- | :--- | :--- |
| `/upload_cam` | `POST` | Binary JPEG buffer | Receives image frame from ESP32-CAM node |
| `/upload_mlx` | `POST` | JSON array of 768 float values | Receives thermal telemetry array from ESP32-Main node |
| `/motor` | `GET / POST` | `ch` (A/B), `speed` (-255 to 255) | Queries or updates target channel speeds |
| `/motorStop` | `GET / POST` | None | Halts both motor channels immediately |
| `/beep` | `GET / POST` | `ms` (duration in milliseconds) | Triggers active buzzer alert |

---

## Safety Guidelines

* **Charging Isolation:** Always toggle the main Rocker Switch to Position O (Charge Mode) before connecting a Type-C charging cable. Charging while the system is powered on can damage sensitive electronics.
* **Reverse Polarity Warning:** Verify wiring connections against `ResqBot Schematics.pdf` before applying battery power. Reversed polarity will destroy microcontroller components.
* **Thermal Management:** Ensure LM2596 regulator and L9110 motor driver modules have sufficient airflow during high-load motor operations.
