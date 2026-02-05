# System Requirements
**Project:** Distributed BAS Prototype

## Purpose
The following requirements define expected behavior for a distributed building automation prototype using **Modbus TCP** field nodes, a **central PLC runtime**, and a **supervisory HMI + historian**.

## 1) Global System Requirements (SYS)
| ID | Requirement | Verification / Success Criteria |
| --- | --- | --- |
| **SYS-01** | The system **shall** initialize all actuators to a **SAFE/OFF** state on startup or after power restoration. | On boot: motor drive disabled (PWM=0); relays de-energized (open). |
| **SYS-02** | The Supervisor PC **shall** provide a unified HMI that visualizes all zones simultaneously. | One dashboard shows live tags for Zone 1 + Zone 2 together. |
| **SYS-03** | All field devices **shall** communicate using **Modbus TCP** over the network. | No proprietary packet formats; only Modbus function codes/frames. |
| **SYS-04** | The system **shall** log critical telemetry to a SQL database for historical review. | MySQL receives timestamped rows during operation. |

## 2) Zone 1 Requirements — Smart VAV (Z1)
| ID | Requirement | Verification / Success Criteria |
| --- | --- | --- |
| **Z1-01** | Zone 1 **shall** maintain actuator position using closed-loop control. | Step from 0° to 45° reaches setpoint and settles without instability. |
| **Z1-02** | Zone 1 **shall** accept local setpoint adjustments via a physical knob/encoder. | Rotating the knob updates local setpoint without PC intervention. |
| **Z1-03** | Zone 1 display **shall** reflect remote setpoint writes from the Supervisor within 1 second. | Writing to holding register **40102** updates OLED setpoint ≤ 1 s. |

## 3) Zone 2 Requirements — Remote Safety / Lighting I/O (Z2)
| ID | Requirement | Verification / Success Criteria |
| --- | --- | --- |
| **Z2-01** | Zone 2 **shall** publish motion detection to the Supervisor with latency < 1.0 s. | Dashboard updates within ~1 s of motion. |
| **Z2-02** | The system **shall** implement a **Safety Latch**: E-Stop forces outputs OFF and prevents restart until manual reset. | Releasing E-Stop does not restore relays; reset action is required. |

## 4) Supervisor PC Requirements (PC)
| ID | Requirement | Verification / Success Criteria |
| --- | --- | --- |
| **PC-01** | The Supervisor **shall** poll field nodes at 500 ms rate. | Stable operation at 500 ms without timeouts/disconnect storms. |
| **PC-02** | The HMI **shall** indicate a communication fault when a node stops responding. | “OFFLINE/COMM ERROR” appears within 2 seconds of disconnect. |