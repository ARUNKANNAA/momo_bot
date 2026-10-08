// ============================================================
// MOMO ROBOT
// Wemos D1 mini (ESP8266)
//
// RoboEyes + Touch + WiFi Setup + Browser Location
// NTP + Weather + Offline Mode
//
// CONTROLS
// ------------------------------------------------------------
// NORMAL FACE
//   Single tap       -> HAPPY for 5 sec
//   Triple tap       -> ANGRY for 5 sec
//   5 sec long press -> TIME
//
// TIME
//   Single tap       -> WEATHER
//   5 sec long press -> NORMAL FACE
//
// WEATHER
//   5 sec long press -> NORMAL FACE
//
// NO TOUCH
//   60 sec -> SAD
//   20 sec -> NORMAL
//
// STARTUP
//   MOMO (3 sec) -> NORMAL FACE
//
// SETUP AP
//   SSID     : Momo-Setup
//   Password : 12345678
//   IP       : 192.168.4.1
//
// WIFI PERSISTENCE
//   Credentials are saved in EEPROM. On every boot Momo
//   tries the saved network first. Only if that fails (or
//   nothing is saved) does it open Setup AP.
// ============================================================

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiClientSecureBearSSL.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <time.h>

#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <FluxGarage_RoboEyes.h>


// ============================================================
// OLED
// ============================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

#define OLED_SDA D2
#define OLED_SCL D1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

RoboEyes<Adafruit_SSD1306> roboEyes(display);


// ============================================================
// TOUCH
// ============================================================

#define TOUCH_PIN D5
#define TOUCH_ACTIVE HIGH


// ============================================================
// WIFI SETUP AP
// ============================================================

const char* SETUP_AP_NAME = "Momo-Setup";
const char* SETUP_AP_PASS = "12345678";

ESP8266WebServer server(80);


// ============================================================
// SAVED CONFIGURATION
// ============================================================

String wifiSSID;
String wifiPassword;

float latitude = 13.0827;
float longitude = 80.2707;


// ============================================================
// NETWORK / OFFLINE MODE
// ============================================================

bool offlineMode = false;

unsigned long lastWiFiRetry = 0;

const unsigned long SETUP_TIMEOUT = 20000;
const unsigned long WIFI_RETRY_INTERVAL = 60000;


// ============================================================
// NTP (India = UTC + 5:30)
// ============================================================

const long GMT_OFFSET_SEC = 19800;
const int DAYLIGHT_OFFSET_SEC = 0;


// ============================================================
// WEATHER
// ============================================================

float temperature = 0;
float humidity = 0;
float windSpeed = 0;

int weatherCode = -1;

String weatherText = "Unknown";

unsigned long lastWeatherUpdate = 0;

const unsigned long WEATHER_INTERVAL = 10UL * 60UL * 1000UL;


// ============================================================
// FACE STATES
// ============================================================

enum FaceState {
  FACE_NORMAL,
  FACE_HAPPY,
  FACE_ANGRY,
  FACE_SAD
};

FaceState currentFace = FACE_NORMAL;


// ============================================================
// DISPLAY / MENU
// ============================================================

enum MenuState {
  MENU_CLOSED,
  SHOW_TIME,
  SHOW_WEATHER
};

MenuState menuState = MENU_CLOSED;

unsigned long menuLastInteraction = 0;

const unsigned long MENU_TIMEOUT = 10000;


// ============================================================
// TOUCH STATE
// ============================================================

bool lastTouch = false;

unsigned long touchStartTime = 0;
unsigned long lastTouchTime = 0;

bool longPressHandled = false;

const unsigned long LONG_PRESS_TIME = 5000;


// ============================================================
// TRIPLE TAP
// ============================================================

int tapCount = 0;

unsigned long lastTapTime = 0;

const unsigned long TAP_GAP = 700;
const unsigned long TAP_MAX_TIME = 500;


// ============================================================
// FACE TIMERS
// ============================================================

unsigned long happyStartTime = 0;
unsigned long sadStartTime = 0;
unsigned long angryStartTime = 0;

const unsigned long HAPPY_TIME = 5000;
const unsigned long ANGRY_TIME = 5000;
const unsigned long NO_TOUCH_TIME = 60000;
const unsigned long SAD_TIME = 20000;


// ============================================================
// STARTUP SPLASH
// ============================================================

bool startupSplash = true;
bool splashDrawn = false;

unsigned long splashStartTime = 0;

const unsigned long SPLASH_TIME = 3000;


// ============================================================
// WEATHER CODE
// ============================================================

// ============================================================
// CENTERED TEXT HELPERS
// ============================================================

// Horizontally centers text at row y.
void drawCenteredText(const char* text, uint8_t size, int y) {

  int16_t x1, y1;
  uint16_t w, h;

  display.setTextSize(size);
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - (int)w) / 2 - x1, y);
  display.print(text);
}

// Centers text both horizontally and vertically.
void drawCenteredBoth(const char* text, uint8_t size) {

  int16_t x1, y1;
  uint16_t w, h;

  display.setTextSize(size);
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  display.setCursor(
    (SCREEN_WIDTH - (int)w) / 2 - x1,
    (SCREEN_HEIGHT - (int)h) / 2 - y1
  );
  display.print(text);
}

// "SEARCHING..." screen shown while connecting to saved Wi-Fi.
void showSearchingScreen(int dots) {

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  drawCenteredText("SEARCHING", 2, 10);

  const char* dotStr[] = { "", ".", "..", "..." };
  drawCenteredText(dotStr[dots % 4], 2, 30);

  String ssid = wifiSSID;
  if (ssid.length() > 21)
    ssid = ssid.substring(0, 21);

  drawCenteredText(ssid.c_str(), 1, 54);

  display.display();
}


String weatherCodeToText(int code) {

  if (code == 0) return "Clear";
  if (code == 1 || code == 2) return "Cloudy";
  if (code == 3) return "Overcast";
  if (code >= 45 && code <= 48) return "Fog";
  if (code >= 51 && code <= 57) return "Drizzle";
  if (code >= 61 && code <= 67) return "Rain";
  if (code >= 71 && code <= 77) return "Snow";
  if (code >= 80 && code <= 82) return "Showers";
  if (code >= 95 && code <= 99) return "Storm";

  return "Unknown";
}


// ============================================================
// SERIAL BANNER HELPERS
// ============================================================

void printCentered(const String &text, int width) {

  int padding = (width - (int)text.length()) / 2;

  if (padding < 0)
    padding = 0;

  for (int i = 0; i < padding; i++) {
    Serial.print(" ");
  }

  Serial.println(text);
}


void printBanner(const String &title) {

  const int width = 32;

  String line = "";

  for (int i = 0; i < width; i++) {
    line += "=";
  }

  Serial.println();
  Serial.println(line);
  printCentered(title, width);
  Serial.println(line);
}


// ============================================================
// FACE CONTROL
// ============================================================

void setFace(FaceState state) {

  if (currentFace == state)
    return;

  currentFace = state;

  switch (state) {

    case FACE_NORMAL:
      roboEyes.setMood(DEFAULT);
      Serial.println("FACE: NORMAL");
      break;

    case FACE_HAPPY:
      roboEyes.setMood(HAPPY);
      happyStartTime = millis();
      Serial.println("FACE: HAPPY");
      break;

    case FACE_ANGRY:
      roboEyes.setMood(ANGRY);
      angryStartTime = millis();
      Serial.println("FACE: ANGRY");
      break;

    case FACE_SAD:
      roboEyes.setMood(TIRED);
      sadStartTime = millis();
      Serial.println("FACE: SAD");
      break;
  }
}


// ============================================================
// SAVE CONFIG
// ============================================================

void saveConfig() {

  const uint32_t MAGIC = 0x4D4F4D4FUL; // "MOMO"
  const int EEPROM_SIZE = 256;
  const int SSID_ADDR = 4;
  const int PASS_ADDR = 68;
  const int LAT_ADDR  = 132;
  const int LON_ADDR  = 136;

  EEPROM.begin(EEPROM_SIZE);

  EEPROM.put(0, MAGIC);

  char ssidBuf[64] = {0};
  char passBuf[64] = {0};

  wifiSSID.toCharArray(ssidBuf, sizeof(ssidBuf));
  wifiPassword.toCharArray(passBuf, sizeof(passBuf));

  for (int i = 0; i < 64; i++) {
    EEPROM.write(SSID_ADDR + i, ssidBuf[i]);
    EEPROM.write(PASS_ADDR + i, passBuf[i]);
  }

  EEPROM.put(LAT_ADDR, latitude);
  EEPROM.put(LON_ADDR, longitude);

  EEPROM.commit();
  EEPROM.end();

  Serial.println("Configuration saved to EEPROM.");
}


// ============================================================
// LOAD CONFIG
// ============================================================

bool loadConfig() {

  const uint32_t MAGIC = 0x4D4F4D4FUL; // "MOMO"
  const int EEPROM_SIZE = 256;
  const int SSID_ADDR = 4;
  const int PASS_ADDR = 68;
  const int LAT_ADDR  = 132;
  const int LON_ADDR  = 136;

  EEPROM.begin(EEPROM_SIZE);

  uint32_t storedMagic = 0;
  EEPROM.get(0, storedMagic);

  if (storedMagic != MAGIC) {
    EEPROM.end();
    wifiSSID = "";
    wifiPassword = "";
    latitude = 13.0827;
    longitude = 80.2707;
    return false;
  }

  char ssidBuf[65] = {0};
  char passBuf[65] = {0};

  for (int i = 0; i < 64; i++) {
    ssidBuf[i] = EEPROM.read(SSID_ADDR + i);
    passBuf[i] = EEPROM.read(PASS_ADDR + i);
  }

  ssidBuf[64] = '\0';
  passBuf[64] = '\0';

  wifiSSID = String(ssidBuf);
  wifiPassword = String(passBuf);

  EEPROM.get(LAT_ADDR, latitude);
  EEPROM.get(LON_ADDR, longitude);

  EEPROM.end();

  if (isnan(latitude) || isnan(longitude)) {
    latitude = 13.0827;
    longitude = 80.2707;
  }

  return wifiSSID.length() > 0;
}


// ============================================================
// WEB SETUP PAGE (no emoji characters)
// ============================================================

void handleSetupPage() {

  String page = R"rawliteral(

<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Momo Setup</title>

<style>

* { box-sizing: border-box; }

body {
  margin: 0;
  min-height: 100vh;
  font-family: Arial, Helvetica, sans-serif;
  color: white;
  background:
    radial-gradient(circle at 10% 10%, #243b55, transparent 40%),
    radial-gradient(circle at 90% 90%, #6a3093, transparent 40%),
    #090d18;
  padding: 20px;
  display: flex;
  align-items: center;
  justify-content: center;
}

.container { width: 100%; max-width: 500px; }

.header { text-align: center; margin-bottom: 22px; }

.logo { font-size: 42px; font-weight: 800; letter-spacing: 9px; }

.subtitle { margin-top: 7px; font-size: 14px; opacity: 0.6; }

.ap-info {
  text-align: center;
  padding: 15px;
  margin-bottom: 17px;
  border-radius: 17px;
  background: rgba(0,130,255,0.12);
  border: 1px solid rgba(0,150,255,0.22);
}

.ap-label {
  font-size: 11px;
  text-transform: uppercase;
  letter-spacing: 2px;
  opacity: 0.55;
}

.ap-name { font-size: 20px; font-weight: 700; margin-top: 6px; }

.ap-details { font-size: 12px; opacity: 0.5; margin-top: 5px; }

.card {
  background: rgba(255,255,255,0.08);
  border: 1px solid rgba(255,255,255,0.14);
  border-radius: 22px;
  padding: 23px;
  margin-bottom: 17px;
  box-shadow: 0 18px 45px rgba(0,0,0,0.25);
  backdrop-filter: blur(15px);
}

.card-title { font-size: 19px; font-weight: 700; margin-bottom: 20px; }

label {
  display: block;
  font-size: 13px;
  font-weight: 600;
  margin-bottom: 7px;
  opacity: 0.75;
}

input {
  width: 100%;
  padding: 13px 14px;
  margin-bottom: 16px;
  border-radius: 12px;
  border: 1px solid rgba(255,255,255,0.13);
  outline: none;
  background: rgba(0,0,0,0.22);
  color: white;
  font-size: 15px;
}

input::placeholder { color: rgba(255,255,255,0.35); }

input:focus {
  border-color: #718cff;
  box-shadow: 0 0 0 3px rgba(113,140,255,0.15);
}

.location-button {
  width: 100%;
  padding: 14px;
  border: none;
  border-radius: 13px;
  background: linear-gradient(135deg, #4776e6, #8e54e9);
  color: white;
  font-size: 15px;
  font-weight: 700;
  cursor: pointer;
}

.location-status {
  text-align: center;
  min-height: 18px;
  margin: 12px 0 15px;
  font-size: 12px;
  opacity: 0.65;
}

.location-row {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 12px;
}

.info { font-size: 12px; line-height: 1.5; opacity: 0.55; }

.save-button {
  width: 100%;
  padding: 16px;
  border: none;
  border-radius: 14px;
  background: linear-gradient(135deg, #00c6ff, #0072ff);
  color: white;
  font-size: 16px;
  font-weight: 700;
  cursor: pointer;
  box-shadow: 0 12px 30px rgba(0,114,255,0.25);
}

.footer { text-align: center; font-size: 12px; opacity: 0.4; margin-top: 17px; }

@media(max-width:450px) {
  .location-row { grid-template-columns: 1fr; gap: 0; }
  .logo { font-size: 34px; }
}

</style>
</head>

<body>

<div class="container">

  <div class="header">
    <div class="logo">MOMO</div>
    <div class="subtitle">Smart Desktop Robot Setup</div>
  </div>

  <div class="ap-info">
    <div class="ap-label">Connect to Momo</div>
    <div class="ap-name">Momo-Setup</div>
    <div class="ap-details">
      Password: 12345678 &nbsp; | &nbsp; 192.168.4.1
    </div>
  </div>

  <div class="card">

    <div class="card-title">Wi-Fi Connection</div>

    <form action="/save" method="POST">

      <label>Wi-Fi Name</label>
      <input name="ssid" required placeholder="Enter your Wi-Fi name" autocomplete="off">

      <label>Wi-Fi Password</label>
      <input name="password" type="password" placeholder="Wi-Fi password" autocomplete="off">

      <div class="card-title" style="margin-top:8px;">Location</div>

      <button type="button" class="location-button" onclick="detectLocation()">
        Detect My Location
      </button>

      <div class="location-status" id="locationStatus">
        Location not detected
      </div>

      <div class="location-row">

        <div>
          <label>Latitude</label>
          <input id="lat" name="lat" type="number" step="any" required placeholder="13.0827">
        </div>

        <div>
          <label>Longitude</label>
          <input id="lon" name="lon" type="number" step="any" required placeholder="80.2707">
        </div>

      </div>

      <div class="info">
        Automatic location uses your device's browser location.
        If it does not work, enter latitude and longitude manually.
      </div>

      <button type="submit" class="save-button" style="margin-top:20px;">
        SAVE & CONNECT
      </button>

    </form>

  </div>

  <div class="footer">Momo - ESP8266</div>

</div>

<script>

function detectLocation() {

  const status = document.getElementById("locationStatus");

  status.innerText = "Detecting your location...";

  if (!navigator.geolocation) {
    status.innerText = "Geolocation is not supported.";
    return;
  }

  navigator.geolocation.getCurrentPosition(

    function(position) {
      document.getElementById("lat").value = position.coords.latitude.toFixed(6);
      document.getElementById("lon").value = position.coords.longitude.toFixed(6);
      status.innerText = "Location detected successfully.";
    },

    function(error) {
      if (error.code === 1) status.innerText = "Location permission denied.";
      else if (error.code === 2) status.innerText = "Location unavailable.";
      else if (error.code === 3) status.innerText = "Location request timed out.";
      else status.innerText = "Could not detect location.";
    },

    { enableHighAccuracy: true, timeout: 15000, maximumAge: 0 }

  );
}

</script>

</body>
</html>

)rawliteral";

  server.send(200, "text/html", page);
}


// ============================================================
// SAVE FROM WEB
// ============================================================

void handleSave() {

  wifiSSID = server.arg("ssid");
  wifiPassword = server.arg("password");

  latitude = server.arg("lat").toFloat();
  longitude = server.arg("lon").toFloat();

  Serial.println();
  Serial.println("Configuration received.");

  Serial.print("SSID: ");
  Serial.println(wifiSSID);

  Serial.print("Latitude: ");
  Serial.println(latitude, 6);

  Serial.print("Longitude: ");
  Serial.println(longitude, 6);

  saveConfig();

  server.send(

    200,

    "text/html",

    "<html>"
    "<head><meta charset='UTF-8'></head>"
    "<body style='font-family:Arial;text-align:center;"
    "background:#111;color:white;padding:40px'>"
    "<h1>MOMO</h1>"
    "<p>Configuration saved.</p>"
    "<p>Momo is restarting...</p>"
    "</body>"
    "</html>"

  );

  delay(1500);

  ESP.restart();
}


// ============================================================
// AP MODE OLED SCREEN
// ============================================================

void drawSetupAPScreen() {

  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(38, 0);
  display.println("MOMO SETUP");

  display.setCursor(0, 16);
  display.print("WiFi: ");
  display.println(SETUP_AP_NAME);

  display.setCursor(0, 30);
  display.print("Pass: ");
  display.println(SETUP_AP_PASS);

  display.setCursor(0, 44);
  display.print("IP: ");
  display.println(WiFi.softAPIP());

  display.display();
}


// ============================================================
// START SETUP AP
//
// Stays here until the user presses SAVE on the web page.
// handleSave() saves the config and restarts the board.
// No timeout, no roboEyes.update() (it would overwrite the
// setup info on the OLED).
// ============================================================

void startSetupAP() {

  printBanner("MOMO SETUP MODE");

  offlineMode = false;

  WiFi.mode(WIFI_AP);

  WiFi.softAP(SETUP_AP_NAME, SETUP_AP_PASS);

  server.on("/", handleSetupPage);
  server.on("/save", HTTP_POST, handleSave);

  server.begin();

  Serial.println();
  Serial.print("SSID: ");
  Serial.println(SETUP_AP_NAME);

  Serial.print("PASSWORD: ");
  Serial.println(SETUP_AP_PASS);

  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());

  Serial.println("Waiting for Wi-Fi credentials...");

  drawSetupAPScreen();

  while (true) {

    server.handleClient();

    delay(2);
  }
}


// ============================================================
// CONNECT WIFI (saved credentials)
// ============================================================

bool connectWiFi() {

  if (wifiSSID.length() == 0) {
    Serial.println("No Wi-Fi credentials.");
    return false;
  }

  Serial.println();
  Serial.println("MOMO CONNECTING TO SAVED WIFI");

  Serial.print("SSID: ");
  Serial.println(wifiSSID);

  WiFi.mode(WIFI_STA);

  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

  unsigned long start = millis();

  int dots = 0;

  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    showSearchingScreen(dots++);
    Serial.print(".");
    delay(300);
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("WiFi connected.");

    Serial.print("IP ADDRESS: ");
    Serial.println(WiFi.localIP());

    offlineMode = false;

    return true;
  }

  Serial.println("WiFi connection failed.");

  return false;
}


// ============================================================
// NTP
// ============================================================

void setupNTP() {

  configTime(
    GMT_OFFSET_SEC,
    DAYLIGHT_OFFSET_SEC,
    "pool.ntp.org",
    "time.nist.gov"
  );

  Serial.println("NTP configured.");
}


// ============================================================
// WEATHER
// ============================================================

void updateWeather() {

  if (offlineMode)
    return;

  if (WiFi.status() != WL_CONNECTED)
    return;

  String url =

    "https://api.open-meteo.com/v1/forecast?"

    "latitude=" + String(latitude, 6) +

    "&longitude=" + String(longitude, 6) +

    "&current="
    "temperature_2m,"
    "relative_humidity_2m,"
    "weather_code,"
    "wind_speed_10m";

  Serial.println();
  Serial.println("Getting weather...");
  Serial.println(url);

  BearSSL::WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;

  if (!http.begin(client, url)) {
    Serial.println("HTTPS connection setup failed.");
    lastWeatherUpdate = millis();
    return;
  }

  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {

    String payload = http.getString();

    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {

      JsonObject current = doc["current"];

      temperature = current["temperature_2m"];
      humidity    = current["relative_humidity_2m"];
      windSpeed   = current["wind_speed_10m"];
      weatherCode = current["weather_code"];

      weatherText = weatherCodeToText(weatherCode);

      Serial.println("Weather updated.");

      Serial.print("Temperature: ");
      Serial.println(temperature);

      Serial.print("Humidity: ");
      Serial.println(humidity);

      Serial.print("Condition: ");
      Serial.println(weatherText);

    } else {

      Serial.println("JSON parsing failed.");

    }

  } else {

    Serial.print("HTTP error: ");
    Serial.println(httpCode);

  }

  http.end();

  lastWeatherUpdate = millis();
}


// ============================================================
// TRY WIFI AGAIN
// ============================================================

void retryWiFi() {

  if (wifiSSID.length() == 0)
    return;

  Serial.println();
  Serial.println("Trying Wi-Fi again...");

  WiFi.mode(WIFI_STA);

  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - start < 5000) {
    delay(100);
  }

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("Internet restored.");

    offlineMode = false;

    setupNTP();

    updateWeather();

  } else {

    Serial.println("Still offline.");

  }
}


// ============================================================
// TIME SCREEN
// ============================================================

void showTimeScreen() {

  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);

  struct tm timeinfo;

  if (!getLocalTime(&timeinfo, 100)) {

    drawCenteredBoth("No internet time", 1);
    display.display();

    return;
  }

  char timeString[10];
  strftime(timeString, sizeof(timeString), "%H:%M", &timeinfo);

  char dateString[20];
  strftime(dateString, sizeof(dateString), "%d/%m/%Y", &timeinfo);

  drawCenteredText(timeString, 3, 14);

  drawCenteredText(dateString, 1, 46);

  display.display();
}


// ============================================================
// WEATHER SCREEN
// ============================================================

void showWeatherScreen() {

  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("WEATHER");

  if (offlineMode) {

    display.setCursor(20, 25);
    display.setTextSize(2);
    display.println("OFFLINE");

    display.setTextSize(1);
    display.setCursor(10, 50);
    display.println("No internet");

    display.display();

    return;
  }

  display.setTextSize(2);
  display.setCursor(0, 16);
  display.print(temperature, 1);
  display.println(" C");

  display.setTextSize(1);

  display.setCursor(0, 40);
  display.print(weatherText);

  display.setCursor(0, 52);
  display.print("Humidity ");
  display.print(humidity, 0);
  display.print("%");

  display.display();
}


// ============================================================
// RETURN TO NORMAL FACE
// ============================================================

void closeMenu() {

  menuState = MENU_CLOSED;

  setFace(FACE_NORMAL);

  tapCount = 0;

  lastTouchTime = millis();

  Serial.println("RETURN -> NORMAL");
}


// ============================================================
// TOUCH HANDLER
// ============================================================

void handleTouch() {

  bool touched = digitalRead(TOUCH_PIN) == TOUCH_ACTIVE;

  // ---------------- TOUCH START ----------------

  if (touched && !lastTouch) {

    touchStartTime = millis();

    longPressHandled = false;
  }

  // ---------------- LONG PRESS ----------------

  if (
    touched &&
    !longPressHandled &&
    millis() - touchStartTime >= LONG_PRESS_TIME
  ) {

    longPressHandled = true;

    tapCount = 0;

    if (menuState == MENU_CLOSED) {

      // NORMAL FACE -> TIME

      menuState = SHOW_TIME;

      menuLastInteraction = millis();

      showTimeScreen();

      Serial.println("5 SEC -> TIME");

    } else {

      // TIME / WEATHER -> NORMAL

      closeMenu();

      Serial.println("5 SEC -> NORMAL");
    }
  }

  // ---------------- TOUCH RELEASE ----------------

  if (!touched && lastTouch) {

    unsigned long duration = millis() - touchStartTime;

    if (longPressHandled) {

      lastTouch = touched;

      return;
    }

    if (duration <= TAP_MAX_TIME) {

      unsigned long now = millis();

      lastTouchTime = now;

      if (menuState == SHOW_TIME) {

        // TIME -> WEATHER

        menuState = SHOW_WEATHER;

        menuLastInteraction = now;

        tapCount = 0;

        showWeatherScreen();

        Serial.println("SINGLE TAP -> WEATHER");

      } else if (menuState == SHOW_WEATHER) {

        // Tap does nothing; long press returns to normal.

        menuLastInteraction = now;

      } else {

        // NORMAL FACE

        if (tapCount > 0 && now - lastTapTime > TAP_GAP) {
          tapCount = 0;
        }

        tapCount++;

        lastTapTime = now;

        if (tapCount >= 3) {

          setFace(FACE_ANGRY);

          angryStartTime = millis();

          lastTouchTime = millis();

          tapCount = 0;

          Serial.println("TRIPLE TAP -> ANGRY");

        } else {

          setFace(FACE_HAPPY);

          happyStartTime = millis();

          Serial.println("TAP -> HAPPY");

        }
      }

    } else {

      // Held too long for a tap, but not a long press.

      tapCount = 0;
    }
  }

  lastTouch = touched;
}


// ============================================================
// FACE TIMERS
// ============================================================

void handleFaceTimers() {

  if (menuState != MENU_CLOSED)
    return;

  // HAPPY -> NORMAL

  if (
    currentFace == FACE_HAPPY &&
    millis() - happyStartTime >= HAPPY_TIME
  ) {

    setFace(FACE_NORMAL);

    lastTouchTime = millis();

    tapCount = 0;

    Serial.println("HAPPY -> NORMAL");
  }

  // ANGRY -> NORMAL

  if (
    currentFace == FACE_ANGRY &&
    millis() - angryStartTime >= ANGRY_TIME
  ) {

    setFace(FACE_NORMAL);

    lastTouchTime = millis();

    tapCount = 0;

    Serial.println("ANGRY -> NORMAL");
  }

  // NO TOUCH -> SAD

  if (
    currentFace != FACE_SAD &&
    currentFace != FACE_ANGRY &&
    currentFace != FACE_HAPPY &&
    millis() - lastTouchTime >= NO_TOUCH_TIME
  ) {

    setFace(FACE_SAD);

    sadStartTime = millis();

    Serial.println("1 MINUTE -> SAD");
  }

  // SAD -> NORMAL

  if (
    currentFace == FACE_SAD &&
    millis() - sadStartTime >= SAD_TIME
  ) {

    setFace(FACE_NORMAL);

    lastTouchTime = millis();

    Serial.println("SAD -> NORMAL");
  }
}


// ============================================================
// MENU DISPLAY UPDATE
// ============================================================

void updateMenuDisplay() {

  if (menuState == MENU_CLOSED)
    return;

  if (menuState == SHOW_TIME) {

    static unsigned long lastTimeDraw = 0;

    if (millis() - lastTimeDraw >= 500) {

      lastTimeDraw = millis();

      showTimeScreen();
    }

  } else if (menuState == SHOW_WEATHER) {

    static unsigned long lastWeatherDraw = 0;

    if (millis() - lastWeatherDraw >= 1000) {

      lastWeatherDraw = millis();

      showWeatherScreen();
    }
  }

  if (millis() - menuLastInteraction >= MENU_TIMEOUT) {

    closeMenu();
  }
}


// ============================================================
// STARTUP SPLASH
//
// Drawn from loop() so it appears AFTER setup/WiFi finishes
// and roboEyes.begin() can't wipe it. Timer starts when the
// splash is first drawn.
// ============================================================

void updateStartupSplash() {

  if (!startupSplash)
    return;

  if (!splashDrawn) {

    splashDrawn = true;

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    drawCenteredBoth("MOMO", 2);
    display.display();

    splashStartTime = millis();

    return;
  }

  if (millis() - splashStartTime >= SPLASH_TIME) {

    startupSplash = false;

    setFace(FACE_NORMAL);

    lastTouchTime = millis();

    Serial.println("MOMO STARTUP COMPLETE");
  }
}


// ============================================================
// SETUP
// ============================================================

void setup() {

  Serial.begin(115200);

  pinMode(TOUCH_PIN, INPUT);

  // I2C / OLED (Wemos D1 mini: SDA=D2, SCL=D1)

  Wire.begin(OLED_SDA, OLED_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {

    Serial.println("SSD1306 failed");

    while (1);
  }

  // ROBO EYES

  roboEyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, 100);

  roboEyes.setAutoblinker(ON, 3, 2);

  roboEyes.setIdleMode(ON, 2, 2);

  roboEyes.setMood(DEFAULT);

  currentFace = FACE_NORMAL;

  lastTouchTime = millis();

  // LOAD SAVED CONFIG

  bool configAvailable = loadConfig();

  if (!configAvailable) {

    Serial.println("No saved configuration.");

    startSetupAP(); // never returns; restarts after SAVE
  }

  // CONNECT TO SAVED WIFI

  if (!connectWiFi()) {

    Serial.println("Wi-Fi connection failed.");

    startSetupAP(); // never returns; restarts after SAVE
  }

  // INTERNET SERVICES

  setupNTP();

  updateWeather();

  printBanner("MOMO READY");

  Serial.println("MODE: ONLINE");
}


// ============================================================
// LOOP
// ============================================================

void loop() {

  // ROBO EYES

  if (menuState == MENU_CLOSED && !startupSplash) {
    roboEyes.update();
  }

  handleTouch();

  handleFaceTimers();

  updateStartupSplash();

  updateMenuDisplay();

  // WEATHER UPDATE

  if (
    !offlineMode &&
    WiFi.status() == WL_CONNECTED &&
    millis() - lastWeatherUpdate >= WEATHER_INTERVAL
  ) {
    updateWeather();
  }

  // WIFI LOST

  if (!offlineMode && WiFi.status() != WL_CONNECTED) {

    Serial.println("Wi-Fi connection lost.");

    offlineMode = true;

    WiFi.disconnect();

    WiFi.mode(WIFI_OFF);

    lastWiFiRetry = millis();
  }

  // OFFLINE WIFI RETRY

  if (
    offlineMode &&
    wifiSSID.length() > 0 &&
    millis() - lastWiFiRetry >= WIFI_RETRY_INTERVAL
  ) {

    lastWiFiRetry = millis();

    retryWiFi();
  }

  delay(2);
}
