🤖 MOMO — Desktop AI Robot
MOMO is a compact Wi-Fi-enabled desktop companion robot built around the Wemos D1 Mini (ESP8266) and a 0.96" OLED display. It provides an interactive animated face, time/date information, setup through a Wi-Fi access point, and a simple desktop-robot experience.
✨ Features
- 🤖 Animated robot face on 0.96" OLED
- 🕒 Real-time clock and date
- 📶 Wi-Fi connectivity
- 🌐 Web-based setup page
- 📡 Wi-Fi Access Point setup mode
- 🌤️ Weather information through online API
- 💾 Stores Wi-Fi and configuration settings
- 📴 Offline mode when Wi-Fi is unavailable
- 👆 Touch-based interaction
- 🎨 Animated eyes and expressions
- 🖥️ OLED-based user interface
- 🔧 Designed as a compact 3D-printed desktop robot
🧠 Hardware
Component	Description
Wemos D1 Mini	ESP8266 main controller
0.96" OLED	SSD1306 128×64 I²C display
LiPo Battery	3.7V 500mAh
TP4056	LiPo charging/protection
Slide Switch	Power control
HW-613	DC-DC buck converter
3D Printed Body	Custom MOMO enclosure


Note: The HW-613 is a DC-DC buck converter. It is not a touch sensor and should not be connected directly to a 3.7V LiPo as a power converter because its input requires a higher voltage.

🔌 OLED Wiring
OLED	Wemos D1 Mini
VCC	3.3V
GND	G
SDA	D2 / GPIO4
SCL	D1 / GPIO5


OLED address:
0x3C

📶 MOMO Setup Mode
When setup mode is enabled, MOMO creates its own Wi-Fi network:
Wi-Fi: Momo-Setup
Password: 12345678
IP: 192.168.4.1

Connect your phone or laptop to the Momo-Setup network and open:
192.168.4.1

You can then configure MOMO's Wi-Fi settings.
💻 Software
Required
- Arduino IDE
- ESP8266 Board Package
- Adafruit GFX Library
- Adafruit SSD1306 Library
- ArduinoJson
- FluxGarage RoboEyes library
Board
Select:
LOLIN(WEMOS) D1 & mini

Then select the appropriate COM port and upload the firmware.
📁 Project Structure
MOMO/
│
├── MOMO_Wemos_D1_Mini.ino
├── README.md
└── 3D Model/
    └── momo_bot final.3mf

🖨️ 3D Printed Body
The MOMO enclosure is designed as a custom desktop robot body for 3D printing.
The project includes the final .3mf CAD model for the robot enclosure.
Recommended printing settings can be adjusted according to the printer, filament and desired surface quality.
🚀 Getting Started
1. Install Arduino IDE.
2. Install the ESP8266 board package.
3. Install the required libraries.
4. Connect the Wemos D1 Mini through USB.
5. Open the MOMO .ino file.
6. Select LOLIN(WEMOS) D1 & mini.
7. Select the correct COM port.
8. Upload the firmware.
9. Connect the OLED and other hardware.
10. Power on MOMO.
11. Configure Wi-Fi through the setup AP if required.
