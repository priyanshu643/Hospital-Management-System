# 🏥 Smart Hospital Management & Alert System

> An edge-to-cloud IoT solution designed to monitor patient vitals, leverage AI for condition summarization, and instantly alert nursing staff to critical changes. 🏆 *Hackathon Winning Project*

[![Python](https://img.shields.io/badge/Python-3.10+-blue.svg?logo=python&logoColor=white)](#)
[![C++](https://img.shields.io/badge/C++-ESP32-00599C.svg?logo=c%2B%2B&logoColor=white)](#)
[![Firebase](https://img.shields.io/badge/Firebase-Realtime%20DB-FFCA28.svg?logo=firebase&logoColor=black)](#)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](#)

---

## 📖 Overview

High nursing loads can lead to delayed responses during sudden patient condition changes. This system acts as a highly responsive, automated bridge between the patient and the nursing station. By processing raw biometric data through a Python-driven AI backend, it eliminates alarm fatigue and ensures medical staff receive contextual, actionable alerts the moment a threshold is breached.

## 🏗️ System Architecture

The pipeline operates in three distinct stages:

1. **Edge (Patient Room):** An ESP32 interfaces with biometric sensors, pushing live data to the cloud.
2. **Cloud (Intelligence):** A Python backend listens to Firebase, evaluates thresholds, and generates an AI JSON summary of the patient's status.
3. **Action (Nursing Station):** A secondary ESP32 triggers a physical alarm, instantly notifying staff.

## 🛠️ Technology Stack

### Hardware
* **Microcontrollers:** 2x ESP32 Development Boards
* **Sensors:** [Insert specific sensors, e.g., MAX30102 Pulse Oximeter, DHT11]
* **Actuators:** Buzzer / Alarm Module

### Software
* **Embedded:** C++ (Arduino IDE / PlatformIO)
* **Backend:** Python
* **Database & Routing:** Firebase Realtime API
* **Data Payload:** JSON

---

## ✨ Key Features

* **⚡ Real-Time Edge Monitoring:** Zero-latency biometric tracking using ESP32.
* **🧠 AI-Powered Context:** Generates quick, readable patient summaries instead of raw, confusing data dumps.
* **🔕 Anti-Alarm Fatigue:** Threshold logic ensures the nursing station bell only rings during genuine emergencies.
* **📈 Highly Scalable:** Easily deploy additional patient nodes by flashing a new ESP32 and linking it to the Firebase DB.

---

## 🚀 Getting Started

### Prerequisites
* PlatformIO or Arduino IDE for flashing ESP32s.
* Python 3.10+ installed on your backend server.
* A Firebase Project with Realtime Database enabled.

### Hardware Setup
1. Wire the **Patient Node ESP32** to your sensors (Refer to `schematics/patient_node.pdf`).
2. Wire the **Nursing Node ESP32** to the alarm system (Refer to `schematics/nursing_node.pdf`).
