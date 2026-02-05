# Zone 1 — Final Engineering Deliverable (Smart VAV Edge Device)

This is the Final Engineering Deliverable for **Zone 1**.

We are building a **Smart VAV (Variable Air Volume) Unit**. This is an **Edge Device** in our Distributed Control System: it is **autonomous** (runs its own local PID) but **connected** (reports to and is supervised by Node-RED).

---

## 1. Hardware Architecture (Wiring Spec)

### Power Domain

- **12V Power Supply**
  - Connects to L298N **`12V`** and **`GND`**.

- **Common Ground (CRITICAL)**
  - You **MUST** connect **ESP32 GND** to **L298N GND** (shared reference).

- **5V Logic**
  - The **encoder requires 5V**.
  - Use **ESP32 VIN** *(only if the board is powered by USB and VIN provides 5V)* or use a **separate 5V regulator**.
  - If powering encoder with **5V**, you must protect ESP32 inputs:
    - Use a **voltage divider** (example: **1k/2k**) on Encoder **A/B** lines before they reach ESP32 GPIO (ESP32 is **3.3V** logic).

---

### Pinout Map

| Component | Pin Name | ESP32 Pin | Notes |
|---|---:|---:|---|
| Motor Driver (L298N) | ENA (PWM) | 25 | Speed Control |
|  | IN1 | 26 | Direction A |
|  | IN2 | 27 | Direction B |
| Encoder (Motor) | Phase A | 34 | Input Only (Use Level Shifter/Divider) |
|  | Phase B | 35 | Input Only (Use Level Shifter/Divider) |
| OLED | SDA | 21 | I2C Data |
|  | SCL | 22 | I2C Clock |
| DHT11 | Data | 4 | Temperature/Humidity |
| Knob (KY-040) | DT | 16 | Rotary Data |
|  | CLK | 17 | Rotary Clock |
|  | SW | 5 | Button (Mode Select) |

---

## 2. The Modbus & SQL Strategy

You asked what to send and why. In a professional industrial control stack, data is divided into:

- **Control (Real-time)**: values needed for command and closed-loop behavior.
- **Analytics (Historical)**: values logged for trends, troubleshooting, and fault detection.

---

### The Register Map (The Language)

We will use **Holding Registers (Hreg)** because they are **read/write friendly**.

| Register ID | Name | Access | Value Scaling | Why |
|---:|---|---|---|---|
| 100 | Room Temp | Read Only | x10 (22.5°C = 225) | HVAC decisions depend on accurate temperature. |
| 101 | Humidity | Read Only | x10 (45.2% = 452) | Too humid = mold risk; must be monitored. |
| 102 | Setpoint | Read / Write | x10 (22.0°C = 220) | Primary command variable; Node-RED writes here to change target temperature. |
| 103 | Damper Pos | Read Only | 0–100 (%) | Confirms the motor/damper actually moved as commanded. |
| 104 | Override | Read Only | 0 = Auto, 1 = Manual | Tells Node-RED whether a human is controlling locally (knob/manual mode). |

---

### The Node-RED & SQL Strategy

Yes — you should do **monitoring and controlling** from **Node-RED**.

- **Control**
  - Node-RED writes **Setpoint** (e.g., **21°C**) to **Register 102**.

- **SQL Logging**
  - Log:
    - **Register 100 (Room Temp)**
    - **Register 103 (Damper Pos)**
  - Frequency: **every 5 minutes**.

**Engineering rationale (fault detection):**  
If the **damper is 100% open** but the **temperature isn’t dropping**, something upstream is wrong (e.g., cooling not available). This is a basic, defensible **fault detection** pattern used in real building automation analytics.

---
The "RoviSys Interview" Pitch

## When they ask "So, what did you build?", do not just say "I made a motor spin." Say this:

## "I built a prototype VAV (Variable Air Volume) Controller with a fully distributed architecture.
- On the Edge: I have an ESP32 running a real-time PID loop on Core 1 to position the damper, while Core 0 handles the HMI and Safety logic asynchronously.
    - The Logic: I implemented a State Machine with three modes: Manual Maintenance, Local Setpoint, and Auto-VAV (Thermostat).
    - The Network: I integrated an industrial Modbus TCP Server, allowing the device to communicate with a SCADA system.
    - The SCADA: I built a Node-RED Dashboard to visualize the telemetry and provide remote supervisory control of the setpoints."

You have successfully built a functional Industrial IoT prototype that mimics exactly what RoviSys engineers do in the field: connecting physical machines (Motors/HVAC) to digital brains (SCADA/Node-RED).

Here is your Interview Cheat Sheet. Screenshot this or print it out. It summarizes everything technical about your project so you can answer their questions confidently.
# The "Demo" Flow (How to present it)
Don't just turn it on. Tell a story.

1) Power Up: Plug in 12V. Wait 5-10 seconds for the "WIFI:OK" on the OLED.
2) Phase 1: Maintenance Mode (Direct):
- Action: Turn the knob. The motor moves.
- Say: "This is the manual override for field technicians to test the damper linkage."
3) Phase 2: Safety Mode (Select):
- Action: Click knob once. Turn to a new angle. Click again to confirm.
- Say: "This prevents accidental bumps. It requires operator confirmation before moving."
4) Phase 3: Automation Mode (Auto VAV):
- Action: Click knob to AUTO. Set temp to 15°C.
- Say: "Now the PID loop is active. It calculates the error between the Room Sensor and the Setpoint to modulate the airflow."
5) Phase 4: Supervisory Control (Node-RED):
- Action: Move the Slider on your laptop to 28°C.
- Say: "And here is the SCADA layer using Modbus TCP. I can remotely override the setpoint, and the edge device responds instantly."

# Technical Specs (The Numbers)

    Controller: ESP32 (Dual Core).

        Core 1: 1kHz PID Loop + Modbus Server (Real-time task).

        Core 0: OLED Display + DHT11 Sensor + Knob (HMI task).

    Protocol: Modbus TCP (Port 502, Unit ID 1).

    Control Algorithm: PID (Proportional-Integral-Derivative) with Anti-Windup and Output Limits.

    Actuator: DC Motor with Quadrature Encoder (360° position control).

3. Modbus Register Map

If they ask "How is the data structured?", here is your map:




    Register,Type,Description,Scaling
100,Read Only,Room Temperature,"Divide by 10 (e.g., 245 = 24.5°C)"
102,Read/Write,Active Setpoint,"Divide by 10 (e.g., 220 = 22.0°C)"
103,Read Only,Damper Position,Raw Degrees (0 - 360)
104,Read Only,System Mode,"0=Manual, 1=Select, 2=Auto"