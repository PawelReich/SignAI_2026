# SignAI 2026 - Smart Obstacle Detection System

Embedded system for visually impaired people that uses Time-of-Flight sensing,
AI classification, and IMU-based context awareness to provide audio
feedback about surrounding obstacles.

## Hardware

| Board | Role |
|---|---|
| STM32L476 (Nucleo) | Main board - ToF + AI + buzzers |
| STM32F401 (Nucleo) | Sensor board - IMU + IR presence |

### Sensors & Peripherals
- *VL53L8CX* (8x8 ToF) - obstacle detection up to 2m
- *LSM6DSV16X* - accelerometer + gyroscope (IMU)
- *STHS34PF80* - IR presence/heat sensor
- *2x Piezo buzzers* - left and right feedback

## Architecture
[STM32F4 - Sensor Board]          [STM32L476 - Main Board]
  LSM6DSV16X (IMU)                  VL53L8CX (ToF 8x8)
  STHS34PF80 (IR)                   NanoEdge AI
       |                                   |
       |------- UART (115200) ------------>|
       |   header: DD CC BB AA             |
       |   payload: gyro/accel/presence    |
                                           |
                                    MotionController
                                           |
                                    Buzzer L + Buzzer R

## How It Works

### 1. AI Classification (NanoEdge AI)
The 8x8 ToF sensor feeds a 64-float buffer into NanoEdge AI classifier every frame:

| Class | Meaning | Buzzer |
|---|---|---|
| 0 — wall | obstacle ahead | both |
| 1 — left | obstacle on left | left |
| 2 — right | obstacle on right | right |
| 3 — free | clear path | silent |

### 2. Proximity Scaling
Buzzer_Proximity(dist_mm, dist_max_mm, buzzer) scales beep frequency with distance.

### 3. Motion Controller (IMU-based context)
IMU data arrives over UART from the sensor board. The MotionController provides sit/stand detection.

## Project Structure
```
├── Core/
│   └── Src/
│       └── main.c          # Main board firmware
├── Drivers/
│   ├── BSP/
│   │   ├── 53L8A1/         # VL53L8CX BSP
│   │   └── Components/
│   │       ├── vl53l8cx/   # ToF driver
│   │       └── ...
│   └── STM32L4xx_HAL_Driver/
├── NEAI_Lib/
│   ├── Inc/
│   │   └── NanoEdgeAI.h
│   └── Lib/
│       └── libneai.a       # Pre-trained classifier
├── Top/
│   ├── Inc/
│   │   └── Buzzer.h
│   │   └── Imu_Reader.h
│   │   └── MotionController.h
│   │   └── Tof.h
│   │   └── Top.h
│   └── Src/
│   │   └── Buzzer.c
│   │   └── Imu_Reader.c
│   │   └── MotionController.c
│   │   └── Tof.c
│   │   └── Top.c
└── X-CUBE-TOF1
```
## Build & Flash

Requires max *STM32CubeIDE 1.18.2*.

1. Clone the repo
2. Open STM32CubeIDE → File > Open Projects from File System
3. Select stm32_project/
4. Build: Project > Build All (or Ctrl+B)
5. Flash: Run > Debug (or F11)

## Configuration

Key parameters in respective files:
// Motion Controller
#define VIB_THRESHOLD     100.0f   // mg — vibration detection sensitivity
#define FREEZE_TIME_MS    500      // ms — freeze duration after shock
#define SIT_DOWN_THRESHOLD 200.0f  // mg — sitting detection threshold
#define STAND_UP_THRESHOLD -200.0f // mg — standing detection threshold

// ToF / AI
#define TOF_SENSOR_ODR    15u      // Hz — ToF update rate
#define DISTANCE_MAX      2000u    // mm — max sensing distance

## Dependencies

- STM32CubeL4 HAL
- X-CUBE-TOF1 (VL53L8CX driver)
- NanoEdge AI Studio — pre-trained .a library
- LSM6DSV16X driver (ST)
- STHS34PF80 driver (ST)

## Team
Paweł Reich
Kamil Ratajczyk
Jan Landecki
Jakub Sawicki
Built at *SignAI Hackathon 2026*.
