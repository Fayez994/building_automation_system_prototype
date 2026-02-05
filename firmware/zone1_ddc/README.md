# 🏢 Smart VAV Control System (IIoT Building Management)

A full-stack Building Management System (BMS) prototype for Single-Zone VAV (Variable Air Volume) control. This project integrates embedded hardware control, industrial communication protocols (Modbus TCP), real-time visualization (Node-RED), and historical data logging (MySQL).

![Dashboard Screenshot](path/to/your/dashboard_screenshot.png)
*(Add your Node-RED Dashboard screenshot here)*

## 🚀 Key Features
* **Closed-Loop Control:** PID algorithm running on ESP32 for precise damper positioning based on temperature setpoints.
* **Industrial Protocol:** Uses **Modbus TCP** over WiFi to communicate between the Edge Device (ESP32) and the Server.
* **Real-Time Monitoring:** Node-RED Dashboard for live tracking of Room Temp, Setpoint, Damper Position, and System Mode.
* **Data Historian:** Automated logging of all sensor data to a **MySQL** database (XAMPP) every second.
* **Tri-Mode Logic:**
    * **Manual:** Direct control of damper angle.
    * **Select:** User chooses target angle.
    * **Auto:** PID controller manages temperature automatically.

## 🛠️ System Architecture

**[Sensor Layer]** DHT11 (Temp) + Rotary Encoder (Setpoint)  
       ⬇️  
**[Edge Control Layer]** ESP32 (PID Logic + Modbus TCP Server)  
       ⬇️ *(WiFi / Port 502)* **[Supervisory Layer]** Node-RED (Modbus Master + Data Parsing)  
       ⬇️ *(Port 3307)* **[Storage Layer]** MySQL Database (XAMPP) -> Table: `zone_logs`

## 🧰 Hardware & Software Stack

### Hardware
* **Microcontroller:** ESP32 Dev Module
* **Sensors:** DHT11 (Temperature), Rotary Encoder (User Input)
* **Actuator:** DC Motor with L298N Driver (Simulated Damper)
* **Display:** OLED SSD1306 (Local Status)

### Software
* **Firmware:** C++ (Arduino IDE) with `ModbusIP_ESP8266` & `AiEsp32RotaryEncoder` libraries.
* **Backend:** Node-RED (Low-code flow-based programming).
* **Database:** MySQL (via XAMPP).
* **Tools:** phpMyAdmin, Modbus Poll (for testing).

## ⚙️ Setup & Installation

### 1. Firmware (ESP32)
1.  Install the required libraries in Arduino IDE.
2.  Update `ssid` and `password` in `VAV_Controller.ino`.
3.  Flash the code to the ESP32.
4.  Verify IP address on the OLED screen.

### 2. Database (MySQL)
1.  Start MySQL in XAMPP (Ensure Port 3307 or 3306).
2.  Run the following SQL script to initialize the logger:
    ```sql
    CREATE DATABASE rovisys_bms;
    USE rovisys_bms;
    CREATE TABLE zone_logs (
        id INT AUTO_INCREMENT PRIMARY KEY,
        timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
        zone_name VARCHAR(50),
        room_temp FLOAT,
        setpoint FLOAT,
        damper_pos INT,
        system_mode VARCHAR(20)
    );
    ```

### 3. Node-RED Flow
1.  Install dependencies: `node-red-contrib-modbus`, `node-red-node-mysql`, `node-red-dashboard`.
2.  Import `flows.json`.
3.  Configure the **Modbus Read** node with the ESP32's IP address.
4.  Configure the **MySQL** node with your XAMPP settings (User: `root`, No Password, Port: `3307`).
5.  Deploy and view the dashboard at `http://localhost:1880/ui`.

## 📊 Data Visualization
The system logs the following data points for analysis:
* **Room Temp vs Setpoint** (Tracking PID performance)
* **Damper Actuation %** (Monitoring mechanical stress)
* **System Mode Changes** (User interaction logs)

![Database Screenshot](path/to/your/database_data.png)
*(Add your phpMyAdmin data list screenshot here)*

## 📝 License
[MIT](https://choosealicense.com/licenses/mit/)

---
---
---
"The hardest part was integrating the IT side (Database) with the OT side (Hardware). I had to resolve port conflicts between IPv4/IPv6 and handle real-time Modbus array parsing to ensure the data stored in SQL exactly matched the physical state of the motor."

VAV_Controller.ino (Your final Arduino code).

flows.json (Export your Node-RED flow).

database_schema.sql (The CREATE TABLE command).

Screenshots folder: Put your Dashboard image and that phpMyAdmin data list there.


----
----
---
Chat
## Zone 1 — Smart VAV (Edge DDC)

### Purpose
A standalone “edge” HVAC controller that runs locally (closed-loop motor position) while exposing key BAS points over Modbus TCP for supervisory control and trending.

### Hardware
- ESP32 (Wi-Fi)
- L298N motor driver (12V domain)
- DC gear motor with incremental encoder
- DHT11 temperature sensor
- OLED (I2C) + KY-040 rotary encoder (local operator interface)
- Bench PSU (12V for motor), USB/5V for ESP32
- Common ground between ESP32 and L298N is required

### Operating Modes
- **MANUAL (Mode=0):** Knob commands damper target position (0–90°).
- **SELECT (Mode=1):** Knob edits temperature setpoint (15–30°C). Setpoint is published to Modbus.
- **AUTO (Mode=2):** Damper target is computed from temperature error using deadband + minimum ventilation and capped to 0–90°.

### Modbus TCP Points (Holding Registers)
| Address | Tag | R/W | Scaling | Description |
|---:|---|---|---|---|
| 100 | Z1_RoomTemp_C10 | R | x10 | Room temperature (°C * 10) |
| 102 | Z1_Setpoint_C10 | R/W | x10 | Cooling setpoint (°C * 10) |
| 103 | Z1_DamperPos_Deg | R | 1:1 | Damper position feedback in degrees (0–90) |
| 104 | Z1_Mode | R | enum | 0=MANUAL, 1=SELECT, 2=AUTO |

### Node-RED Dashboard (Minimum)
- Gauge: Room Temp (Hreg 100 ÷ 10)
- Slider/Numeric: Setpoint (writes Hreg 102 = TempC * 10)
- Gauge: Damper Position (Hreg 103)
- Text/LED: Mode (Hreg 104)

### Verification (Key Tests)
- Setpoint write from Node-RED updates Hreg 102 and influences AUTO mode behavior.
- SELECT mode edits setpoint locally and updates Hreg 102.
- AUTO mode changes damper target within 0–90° and stops at target (no continuous hunting).
