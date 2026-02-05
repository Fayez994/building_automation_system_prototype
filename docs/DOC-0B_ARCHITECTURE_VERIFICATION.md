# Architecture & Verification
**Project:** Distributed BAS Prototype

## 1) Network Layout
Star topology over Wi-Fi/LAN:

- Supervisor PC acts as Modbus **Client** (Master)
- ESP32 nodes act as Modbus **Servers** (Slaves)

| Device | Role | IP Address (Static/Reserved) |
| --- | --- | --- |
| **Supervisor PC** | OpenPLC + Node-RED + MySQL | `192.168.197.52` |
| **Zone 1 ESP32** | Smart VAV controller | `192.168.197.203` |
| **Zone 2 ESP32** | Remote Safety/I-O | `192.168.197.60` |

> Note: Modbus/TCP commonly uses TCP port **502**.

## 2) ICD — Modbus Register / Coil Map (Interface Contract)
This section is the **Interface Control Document (ICD)** for integrating:
- Node-RED dashboard widgets
- OpenPLC ladder logic
- ESP32 firmware

### Zone 1 (Smart VAV) — Holding Registers (16-bit)
| Address | Name | R/W | Description |
| ---: | --- | :---: | --- |
| **40100** | `Z1_RoomTemp` | R | Room temperature (x10 scaling). Example: 22.5°C → 225 |
| **40102** | `Z1_Setpoint` | R/W | Target temperature (writable from HMI) |
| **40103** | `Z1_Position` | R | Current damper angle (0–90° or mapped range used by firmware) |
| **40104** | `Z1_Mode` | R | 0 = Manual, 1 = Auto |

### Zone 2 (Remote Safety/I-O) — Discrete Inputs + Coils
| Address | Name | Type | Description |
| ---: | --- | --- | --- |
| **10001** | `Z2_Occupied` | Discrete Input | PIR status (1 = motion) |
| **10002** | `Z2_ManualBtn` | Discrete Input | Manual/Reset input (if implemented) |
| **00001** | `Z2_Relay_Light` | Coil | Main light output |
| **00002** | `Z2_Relay_Aux` | Coil | Auxiliary output |
| **00003** | `Z2_Sys_Active` | Coil | Safety armed state (0 = E-Stopped, 1 = armed) |

## 3) PLC Logic Summary (OpenPLC)
### Safety Latch
- E-Stop forces `Z2_Sys_Active = 0`
- System remains blocked until manual reset condition is satisfied

### Occupancy Hold Timer (Off-Delay)
- Occupancy remains “true” for a configured hold time after motion clears
- Used to drive lighting coil(s)

## 4) Verification / Test Results
| Test ID | Description | Result | Evidence |
| --- | --- | :---: | --- |
| **TC-01** | Comm loss indication | ✅ PASS | Node-RED shows OFFLINE within ~2 s after node disconnect |
| **TC-02** | Zone 1 closed-loop response | ✅ PASS | Setpoint change results in settle within target time (measured) |
| **TC-03** | E-Stop latch behavior | ✅ PASS | Outputs drop immediately; do not re-energize until reset |
| **TC-04** | MySQL logging | ✅ PASS | Time-series rows visible in `zone_logs` table |
