// Thermostat controlled heat exchanger with web interface

// Need:
// temp readout
// relay control for fans and pumps
// conditional control based on temperature
// web interface for control and monitoring
// with heating/cooling modes and setpoints

// include libraries
#include <WiFi.h>
#include <WebServer.h>
#include <vector>

// setup WiFi credentials and web server
char* station_ssid = "HeatExchangerStation";
char* station_password = "12345678";
char* AP_ssid = "HeatExchangerAP";
char* AP_password = "12345678";
WebServer server(80);

//setup in/out pins
int LED = 2; // GPIO pin for the LED
// int relayPin = _; // GPIO pin for the relay
// int TempPin = _; // GPIO pin for the temperature sensor

// setup temp timing cycle
constexpr unsigned long TempWait = 1000; // milliseconds between temperature readings
unsigned long now = millis();
unsigned long lastTempRead = 0; // timestamp of last temperature reading

// structure to hold thermocouple readings
struct Thermo
{
  const char* name;
  float CurTempC;
  float SetTempC;
  bool available; // false for thermocouples programmed in but not connected
};
std::vector<Thermo> CurThermo = 
{ // name, current temperature, setpoint temperature, availability
  {"Thermocouple 1", 0.0f, 0.0f, false},
  {"Thermocouple 2", 0.0f, 0.0f, false}
};

void updateTemp() // update temperature readings from thermocouples
{
  now = millis();
  if (now - lastTempRead < TempWait) // only read temperature every TempWait milliseconds
  {return;}
  lastTempRead = now; // update timestamp
  for (int i = 0; i < CurThermo.size(); ++i)
  {
    //thermocouples[i].ReadTempC; // Placeholder for actual reading
    //return thermocouples[i].available; // Return availability status
  }
  return;
}

void showHomePage()
{
  server.send(200, "text/html",
    R"rawliteral(
<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Heat Exchanger Monitor</title>
  </head>
  <body>
    <h1>Heat Exchanger Monitor</h1>
    <ul id="temperatures"><li>Waiting for sensors</li></ul>
    <script>
      async function refreshTemperature() {
        const display = document.getElementById("temperatures");
        try {
          const response = await fetch("/api/status");
          const status = await response.json();
          display.replaceChildren();
          status.sensors.forEach((sensor) => {
            const item = document.createElement("li");
            item.textContent = sensor.available
              ? `${sensor.name}: ${sensor.temperatureC.toFixed(2)} \u00B0C`
              : `${sensor.name}: reading unavailable`;
            display.appendChild(item);
          });
        } catch (error) {
          display.replaceChildren();
          const item = document.createElement("li");
          item.textContent = "Unable to contact the controller";
          display.appendChild(item);
        }
      }
      refreshTemperature();
      setInterval(refreshTemperature, 1000);
    </script>
  </body>
</html>
)rawliteral");
}

void sendStatus()
{
  String json = "{\"sensors\":[";
  for (size_t i = 0; i < CurThermo.size(); ++i)
  {
    if (i > 0)
    {
      json += ",";
    }

    const Thermo& sensor = CurThermo[i];
    json += "{\"name\":\"";
    json += sensor.name;
    json += "\",\"available\":";
    json += sensor.available ? "true" : "false";
    json += ",\"temperatureC\":";
    if (sensor.available)
    {
      json += String(sensor.CurTempC, 2);
    }
    else
    {
      json += "null";
    }
    json += "}";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void setup() 
{
  // initialize serial communication and WiFi
  Serial.begin(115200); // declare bodrate for serial communication
  WiFi.mode(WIFI_AP_STA); // set WiFi mode to both station and access point
  WiFi.begin(station_ssid, station_password); // connect to WiFi network
  WiFi.softAP(AP_ssid, AP_password); // start access point

  // setup pins
  pinMode(LED, OUTPUT); // set LED pin as output
  digitalWrite(LED, LOW); // turn off LED initially
  // pinMode(relayPin, OUTPUT); // set relay pin as output
  // pinMode(TempPin, INPUT); // set temperature sensor pin as input

  // allow the AP to work if station Wi-Fi is unavailable
  const unsigned long stationStartMillis = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - stationStartMillis < 10000)
  {
    Serial.print(".");
    delay(500);
  }

  // print connection status and IP addresses if connected, otherwise print timeout message
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print("\n\nStation Mode\n\n");
    Serial.println("Connected to: " + String(station_ssid)); // print SSID of connected network
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP()); // print IP address of station mode
  }
  else
  {
    Serial.println("\nStation connection timed out; access point remains available.");
  }

  Serial.print("\n\nAccess Point Mode\n");
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP()); // print IP address of access point mode

  server.on("/", showHomePage);
  server.on("/api/status", sendStatus);
  // server.on("/LEDon", LEDon); // define route for turning on LED
  // server.on("/LEDoff", LEDoff); // define route for turning off LED
  server.begin(); // start the web server
  }

void loop() 
{
  server.handleClient();
  updateTemp();
}