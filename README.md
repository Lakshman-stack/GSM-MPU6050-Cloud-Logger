# GSM-MPU6050-Cloud-Logger
GSM-based IoT system with MPU6050 sensor, SMS control, and cloud data logging

# GSM MPU6050 Cloud Logger

## 📌 Project Overview
This project is an IoT-based system using Arduino UNO, GSM module, and MPU6050 sensor to monitor data and send it to a cloud server. The device can also be controlled remotely using SMS.

## 🚀 Key Features
- Real-time data transmission using GSM (GPRS)
- SMS-based configuration (update Device ID and Server IP)
- EEPROM backup system (Box A + Box B) for power failure safety
- Priority-based SMS handling (SMS interrupts cloud operation)
- Automatic SIM detection (Airtel, BSNL, VI)

## 🛠️ Hardware Used
- Arduino UNO
- GSM Module (SIM800/SIM900)
- MPU6050 Sensor
- SIM Card

## ⚙️ Working Principle
1. MPU6050 sensor reads motion data.
2. Data is processed and sent to a cloud server via HTTP.
3. User can send SMS commands:
   - `ID:<device_id>` → updates device ID
   - `IP:<ip:port>` → updates server URL
   - `STATUS` → returns current configuration
4. Data is stored safely in EEPROM to handle power loss.

## 📩 Example SMS Commands
ID:E20-751BS
IP:172.105.44.142:80
STATUS

## 🔐 Reliability Features
- Dual EEPROM storage (Box A + Box B)
- Safe update mechanism using flag system
- Recovery from power failure during updates

## ⚠️ Notes
- GSM module requires stable power supply
- SIM must have SMS and data plan enabled
- SoftwareSerial may limit performance on Arduino UNO

## 👨‍💻 Author
Lakshman
