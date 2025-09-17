#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// ================== USER SETTINGS ==================
const char* ssid     = "Your Wifi name";       // 🔹 WiFi SSID
const char* password = "Wifi Password";        // 🔹 WiFi Password

String apiKey       = "Your API Key";     // OpenWeatherMap API Key
String defaultCity  = "Default city";     // Your Hometown or preferred location as default
String currentCity  = defaultCity;

const long gmtOffset_sec     = 19800; // 🔹 Default: IST (UTC+5:30) → 5*3600 + 30*60
const int daylightOffset_sec = 0;

bool blinkColon   = true;  // Toggle colon for clock blinking effect
int cityTimeOffset = 0;    // Offset (in seconds) from API

// ================== OBJECTS ==================
LiquidCrystal_I2C lcd(0x27, 16, 2);  // LCD @ I2C address (0x27 or 0x3F)
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000); // Sync every 60s

unsigned long lastOverrideTime = 0;  // Track city override timer
bool overrideActive = false;         // Flag for city override state

// ================== FUNCTIONS ==================

// 🔹 Connect to WiFi and show status on LCD
void connectWiFi() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");

  WiFi.begin(ssid, password);

  // Wait until WiFi connects
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    lcd.setCursor(0, 1);
    lcd.print(".");
  }

  // Show success message
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connected!");
  delay(1500);
  lcd.clear();
}

// 🔹 Fetch weather data from OpenWeatherMap
String getWeather(String city) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String serverPath = "http://api.openweathermap.org/data/2.5/weather?q=" 
                        + city + "&appid=" + apiKey + "&units=metric";

    http.begin(serverPath.c_str());
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      String payload = http.getString();
      http.end();

      // Parse JSON response
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);

      float temp       = doc["main"]["temp"];
      int humidity     = doc["main"]["humidity"];
      String weather   = doc["weather"][0]["main"].as<String>();
      cityTimeOffset   = doc["timezone"]; // City timezone offset (in seconds)

      // Format: "25.4 95% Rain"
      char buffer[32];
      snprintf(buffer, sizeof(buffer), "%.1f %d%% %s", temp, humidity, weather.c_str());
      return String(buffer);
    } 
    else {
      http.end();
      return "HTTP Error";
    }
  }
  return "WiFi Error";
}

// 🔹 Display city, time, and weather on LCD
void displayWeather(String city) {
  timeClient.update();

  // --- Get weather string ---
  String weatherData = getWeather(city);

  // Extract temperature, humidity, condition
  int space1    = weatherData.indexOf(' ');
  int space2    = weatherData.indexOf(' ', space1 + 1);

  String temp      = weatherData.substring(0, space1);               
  String humidity  = weatherData.substring(space1 + 1, space2);  
  String condition = weatherData.substring(space2 + 1);         

  // --- Get local city time ---
  time_t utcTime   = timeClient.getEpochTime();
  time_t localTime = utcTime + cityTimeOffset;      // Adjust for city offset
  struct tm* timeInfo = gmtime(&localTime);         // Convert to time struct

  // Format HH:MM with blinking colon
  char timeStr[6]; 
  if (blinkColon) {
      strftime(timeStr, sizeof(timeStr), "%H:%M", timeInfo);
  } else {
      strftime(timeStr, sizeof(timeStr), "%H %M", timeInfo);
  }
  blinkColon = !blinkColon;

  // --- ROW 1: City + Time ---
  String cityDisplay = city;
  if (cityDisplay.length() > 10) cityDisplay = cityDisplay.substring(0, 10);

  char row1[17];
  snprintf(row1, sizeof(row1), "%-10s %5s", cityDisplay.c_str(), timeStr);
  lcd.setCursor(0, 0);
  lcd.print(row1);

  // --- ROW 2: Temp + Humidity + Condition ---
  char row2[17];      
  char degree = 223;   // LCD degree symbol
  snprintf(row2, sizeof(row2), "%4s%cC %s %s", temp.c_str(), degree, humidity.c_str(), condition.c_str());
  
  lcd.setCursor(0, 1);
  lcd.print(row2);
}

// ================== MAIN ==================
void setup() {
  Serial.begin(115200);

  // Initialize LCD
  lcd.init();
  lcd.backlight();

  // --- Splash Screen ---
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("   WeatherSync   ");   // Project name
  lcd.setCursor(0, 1);
  lcd.print("  Initializing   ");   // Subtitle
  delay(2000);
  lcd.clear();

  // --- Connect WiFi & Start NTP ---
  connectWiFi();
  timeClient.begin();
}

void loop() {
  // --- Check for Serial Input (City Override) ---
  if (Serial.available()) {
    String inputCity = Serial.readStringUntil('\n');
    inputCity.trim();

    if (inputCity.length() > 0) {
      currentCity     = inputCity;
      overrideActive  = true;
      lastOverrideTime = millis();

      Serial.println("Showing weather for: " + currentCity);
    }
  }

  // --- Reset to default city after timeout (10 sec) ---
  if (overrideActive && (millis() - lastOverrideTime > 10000)) {
    currentCity    = defaultCity;
    overrideActive = false;
  }

  // --- Update LCD with weather & clock ---
  displayWeather(currentCity);
  delay(1000); // Update every 1 second
}
