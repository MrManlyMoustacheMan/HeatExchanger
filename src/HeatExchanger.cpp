// Thermostat controlled heat exchanger with web interface

// include libraries
#include <vector>
#include <Arduino.h>
#include "FS.h"
#include "LittleFS.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <WebServer.h>


// setup WiFi credentials and web server
const char* station_ssid = "HeatExchangerStation";
const char* station_password = "12345678";
const char* AP_ssid = "HeatExchangerAP";
const char* AP_password = "12345678";
WebServer server(80);


//setup in/out pins
int LEDbrd = 2;
// int LEDgrn = 13;
// int LEDred = 14;
// int relayPin = _; // GPIO pin for the relay

// setup temp timing cycle
unsigned long now = millis();
unsigned long lastTempRead = 0; // timestamp of last temperature reading
unsigned long lastTempPrint = millis();

struct Thermo 
{ // structure to hold thermocouple readings
  const char* name;
  int pin;
  float curTempC;
  OneWire oneWire;
  DallasTemperature sensor;
  // Constructor initializes OneWire and DallasTemperature with the struct's pin
  Thermo(const char* n, int p, float initialTemp = 0.0f)
    : name(n), pin(p), curTempC(initialTemp), oneWire(p), sensor(&oneWire) {}
  Thermo(const Thermo& other)
    : name(other.name), pin(other.pin), curTempC(other.curTempC), oneWire(other.pin), sensor(&oneWire) {}
};
std::vector<Thermo> CurThermo = 
{ // name, current temperature, setpoint temperature, availability
  {"Thermocouple 1", 32, 0.0f},
  {"Thermocouple 2", 33, 0.0f}
};

bool storageFull = false;
bool hasSpace()
{
  if (storageFull) return false;

  size_t total = LittleFS.totalBytes();
  size_t used  = LittleFS.usedBytes();

  // Safe subtraction check + 16 KB buffer
  if (total <= used || (total - used) <= 16384)
  {
    storageFull = true;
    Serial.println("Storage full! Halting CSV logging.");
    return false;
  }
  return true;
}

void writeTemp()
{
  File file = LittleFS.open("/data.csv", FILE_APPEND);
  file.print(lastTempRead/1000);
  file.print(", ");
  file.print(CurThermo[0].curTempC);
  file.print(", ");
  file.println(CurThermo[1].curTempC);
  file.close();
  return;
}

void updateTemp() // update temperature readings from thermocouples
{
  if (millis() - lastTempRead < 1000) // only read temperature every TempWait milliseconds
  {
    return;
  }
  lastTempRead = millis(); // update timestamp

  for(size_t i = 0; i < CurThermo.size(); i++)
  {
    if (CurThermo[i].sensor.getDeviceCount() == 0)
    {
      CurThermo[i].curTempC = NAN;
    }
    else
    {
      CurThermo[i].sensor.requestTemperatures();
      CurThermo[i].curTempC = CurThermo[i].sensor.getTempCByIndex(0);
    }
  }
  if (hasSpace())
  {
    writeTemp();
  }
}

void printCSV()
{
  lastTempPrint = millis();
  Serial.println();
  File file = LittleFS.open("/data.csv", FILE_READ);
  while (file.available()) 
  {
    Serial.write(file.read());
  }
  file.close();
  return;
}

void setup() 
{
  // initialize serial communication
  Serial.begin(115200); // declare baud rate for serial communication
  delay(1000); // wait for sensors to stablize
  CurThermo[0].sensor.begin();
  CurThermo[1].sensor.begin();

  // Write the CSV header once at startup
  LittleFS.begin(true); // Mount flash memory (formats automatically on first use)
  File file = LittleFS.open("/data.csv", FILE_WRITE);
  file.print("Time_s, "); file.print(String(CurThermo[0].name)); file.print(", "); file.println(String(CurThermo[1].name));
  file.close();

  WiFi.mode(WIFI_AP); // set WiFi mode to access point
  WiFi.softAP(AP_ssid, AP_password); // start access point
  Serial.print("\n\nAccess Point Mode\n"); // print IP address of access point mode
  delay(100);
  Serial.print("AP IP address: "); Serial.println(WiFi.softAPIP()); Serial.println();
  Serial.print("Beginning data collection...\n\n");
  
  server.on("/download", HTTP_GET, 
    []() {
      File file = LittleFS.open("/data.csv", FILE_READ);
      if (!file) {
        server.send(404, "text/plain", "Error: /data.csv not found");
        return;
      }
  // Tells the browser to save the response as a file download instead of displaying text
    server.sendHeader("Content-Disposition", "attachment; filename=\"data.csv\"");
    server.streamFile(file, "text/csv");
    file.close();
});
  server.begin(); // start the web server
  pinMode(LEDbrd, OUTPUT); // set LED pin as output
  digitalWrite(LEDbrd, LOW); // turn off LED initially
}

void loop()
{
  server.handleClient();
  if(millis() < 20UL*60UL*1000UL)
  {
    updateTemp();
    if(millis() - lastTempPrint > 30000)
    {
      printCSV();
    }
  }
  else if(digitalRead(LEDbrd) == LOW)
  {
    printCSV();
    Serial.print("\nData Collection Finished\n");
    Serial.print("Connect to "); Serial.println(station_ssid);
    Serial.print("Password: "); Serial.println(station_password);
    Serial.print("Retrieve Data at "); Serial.print(WiFi.softAPIP()); Serial.print("/download");
    digitalWrite(LEDbrd, HIGH); // indicates test is finished and data is ready'
  }  
}