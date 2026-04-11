# AI Room Cleanliness Judge

An ESP32 camera project that captures images of your room and uses AI vision to judge how clean it is. Runs on the **XIAO ESP32S3 Sense** with its onboard camera.

![Room Judge AI](https://img.shields.io/badge/Platform-XIAO%20ESP32S3-blue) ![License](https://img.shields.io/badge/License-MIT-green [![CopperPilot](https://img.shields.io/badge/CopperPilot-Project-b87333)](https://copperpilot.ai/)
[![Visitors](https://visitor-badge.laobi.icu/badge?page_id=copperpilot)](https://copperpilot.ai/)

## What It Does

1. **Captures** a photo of your room using the ESP32 camera
2. **Sends** the image to OpenAI's vision API for analysis
3. **Returns** a cleanliness score (0-100), a witty comment, and a to-do list of cleaning tasks

All through a clean web interface served directly from the microcontroller.

## Hardware

- [Seeed Studio XIAO ESP32S3 Sense](https://www.seeedstudio.com/XIAO-ESP32S3-Sense-p-5639.html) (with onboard OV2640 camera)

## Setup

### 1. Clone the repo

```bash
git clone https://github.com/Gangwa-Labs/Ai_cleaning.git
```

### 2. Configure credentials

Create a `config.h` file in the project directory:

```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* openai_api_key = "YOUR_OPENAI_API_KEY";
```

Get an OpenAI API key at [platform.openai.com/api-keys](https://platform.openai.com/api-keys).

### 3. Install dependencies

In the Arduino IDE:

- **Board**: Install the ESP32 board package and select `XIAO_ESP32S3`
- **Libraries**: Install `ArduinoJson` from the Library Manager

### 4. Upload & Run

1. Connect your XIAO ESP32S3 via USB
2. Select the correct board and port in Arduino IDE
3. Upload the sketch
4. Open Serial Monitor (115200 baud) to see the device IP address
5. Open the IP address in your browser

## Web Interface

The device hosts a responsive web UI with:

- **Capture** button to take a photo
- **Judge Room** button to send the image for AI analysis
- A cleanliness **score bar** (color-coded red/yellow/green)
- A **to-do checklist** of suggested cleaning tasks

## API Endpoints

| Endpoint | Method | Description |
|-----------|--------|-------------|
| `/` | GET | Web interface |
| `/capture` | GET | Capture a new image |
| `/last_image` | GET | Get the last captured JPEG |
| `/analyze` | GET | Send captured image to AI for analysis |

## License

MIT
