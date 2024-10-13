#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <DHT.h>
#include <vector>
#include <time.h>  // Include time.h for NTP functionality

#define DHTPIN 15  // Pin for DHT11
#define DHTTYPE DHT11
#define HEATER_PIN 25 //Pin for Heater
#define COOLER_PIN 33 //Pin for Cooler
#define ROOM_LIGHT_PIN 19 //Pin for Room LIght
#define BLUE_LED_PIN 2 // Pin for onboard Blue LED

DHT dht(DHTPIN, DHTTYPE);
AsyncWebServer server(80);

// Global variables
int scheduleActiveDuration = 5; // Duration (in minutes) to keep the schedule active
time_t lastScheduledTime = 0;   // Stores the last time a schedule was applied

bool isScheduleActive = false;  // Flag to track if the schedule is active
bool systemPower = false;
bool roomLightState = false;
int requiredTemp = 27; // manual required temperature
int scheduledTemp = 27; //scheduled temperarture
float currentTemp;

struct Schedule {
  int hour;
  int minute;
  int scheduledTemp;
  int duration;  // Duration in minutes
};

// Vector to hold schedules
std::vector<Schedule> schedules;

// NTP server settings
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800;  // GMT +5:30 (Sri Lanka time)
const int daylightOffset_sec = 0;  // No daylight saving time

// Function to initialize NTP and fetch time
void initNTP() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Synchronizing time with NTP...");
  
  // Wait for time to be set
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time, retrying...");
    delay(2000);
  }
  Serial.println("Time synchronized successfully.");
}

// Function to check and apply schedules
void checkAndApplySchedule() {
    time_t now = time(nullptr);
    struct tm *currentTime = localtime(&now);

    isScheduleActive = false;  // Assume no schedule is active by default

    for (const Schedule &schedule : schedules) {
        // Convert schedule time to a time_t value for easier comparison
        struct tm scheduleTime = *currentTime;
        scheduleTime.tm_hour = schedule.hour;
        scheduleTime.tm_min = schedule.minute;
        scheduleTime.tm_sec = 0;  // Reset seconds
        time_t scheduleStartTime = mktime(&scheduleTime);

        // Check if current time is within the schedule's active window
        if (now >= scheduleStartTime && (now - scheduleStartTime) / 60 < schedule.duration) {
            scheduledTemp = schedule.scheduledTemp;  // Override manual setting
            isScheduleActive = true;  // Schedule is now active
            lastScheduledTime = scheduleStartTime;  // Record the start time of the schedule
            scheduleActiveDuration = schedule.duration;
            Serial.printf("Applied scheduled temperature: %d at %02d:%02d\n", schedule.scheduledTemp, schedule.hour, schedule.minute);
        }
    }

    // If a schedule was recently applied, keep it active for the defined duration
    if (lastScheduledTime > 0 && (now - lastScheduledTime) / 60 < scheduleActiveDuration) {
        isScheduleActive = true;
        Serial.println("Schedule is still in control (within active duration).");
    }
}

// Web page definition with schedule section added
const char* webPage = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta charset="UTF-8">
  <title>Thermo Glow</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body {         background-image: url('https://hasaranga.com/sajana/background.jpg');
            background-size: cover; /* Make sure the background covers the whole area */
            background-position: center; /* Center the background image */
            background-repeat: no-repeat; /* Avoid repeating the image */
 font-family: 'Segoe UI', Tahoma, Geneva, sans-serif; background-color: #f4f4f9; color: #333; margin: 0; padding: 0; text-align: center; }
    h1 { color: #4CAF50; margin-bottom: 0; }
    h1 span.orange { 
      color: orange; 
    }
    h2{ color: #1aa0e8; margin-top: 5px; }
    h2 span.sky blue { 
      color: blue; 
    }
    label { font-size: 1.1em; margin-top: 15px; display: block; }
    input[type=checkbox] { transform: scale(1.5); margin-top: 10px; }
    input[type=number], input[type=time] { padding: 8px; width: 80px; font-size: 1em; margin: 10px 0; }
    .button { background-color: #4CAF50; color: white; padding: 12px 20px; border: none; cursor: pointer; font-size: 1.1em; border-radius: 5px; transition: 0.3s; }
    .button:hover { background-color: #45a049; }
    .indicator { width: 10px; height: 10px; border-radius: 50%; display: inline-block; margin: 0.5px;}
.off {
  background-color: LightSlateGray;
}
.light-on {
  background-color: Lime;
}
.cooler-on {
  background-color: DeepSkyBlue;
}
.heater-on {
  background-color: DarkOrange;
}
    .section { margin: 20px auto; padding: 20px; max-width: 400px; background-color: #fff; border-radius: 10px; box-shadow: 0 4px 10px rgba(0, 0, 0, 0.1); }
    ul { list-style-type: none; padding: 0; }
    li { margin: 10px 0; font-size: 1.1em; }
    #scheduleList { margin-top: 15px; text-align: center; }
    /* Flexbox styling for alignment */
    .status-container {
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    
    .status-item {
      flex: 1;
      text-align: center;
    }
  </style>
</head>
<body>
  <header>
    <h1><br>THERMO</br> <span class="orange">GLOW</span></h1>
    <h2><i><span class="sky blue">Comfort Your Own Way</span></i></h2>
  </header>

  <div class="section">
    <h3>System Status</h3>
    <div class="status-container">
      <div class="status-item">Heater <br> <span id="heaterIndicator" class="indicator off"></span></div></br>
      <div class="status-item">Room Light <br> <span id="lightIndicator" class="indicator off"></span></div></br>
      <div class="status-item">Cooler <br><span id="coolerIndicator" class="indicator off"></span></div></br>
    </div>
  </div>
  <div class="section">
    <h3>Room Controls</h3>
    <label>Room Light
    <input type="checkbox" id="roomLightToggle" onchange="toggleRoomLight()"></label>
    <label>Temp System Power
    <input type="checkbox" id="systemPowerToggle" onchange="toggleSystem()"></label>
    <label>Temperature Setup (°C)
    <input type="number" id="tempSetup" value="27" onchange="setTemperature()"></label>
  </div>

  <div class="section">
    <h3>Current Temperature</h3>
    <p id="currentTempDisplay">Current Temp: --°C</p>
  </div>

  <div class="section">
    <h3>Smart Schedule</h3>
    <label>Time</label>
    <input type="time" id="scheduleTime">
    <br><br>

    <label>Scheduled Temperature (°C)</label>
    <input type="number" id="scheduleTemp" value="27">
    <br><br>

    <label>Duration (minute/s)</label>
    <input type="number" id="scheduleDuration" value="5">
    <br><br>

    <button class="button" onclick="addSchedule()">Add Schedule</button>

    <h4>Scheduled List</h4>
    <ul id="scheduleList"></ul>
  </div>

  <script>
    function updateTemperature() {
      fetch('/temperature')
      .then(response => response.json())
      .then(data => {
        document.getElementById('currentTempDisplay').innerHTML = "Current Temp: " + data.currentTemp + "&#176;C";
      })
      .catch(() => {
        document.getElementById('currentTempDisplay').innerHTML = "Error fetching temp";
      });
    }

    // Update temperature every 2 seconds
    setInterval(updateTemperature, 2000);

    function toggleSystem() {
      let systemPower = document.getElementById("systemPowerToggle").checked ? "on" : "off";
      fetch("/control", {
        method: "POST",
        body: new URLSearchParams({ "systemPower": systemPower })
      });
    }

    function toggleRoomLight() {
      let roomLight = document.getElementById("roomLightToggle").checked ? "on" : "off";
      fetch("/control", {
        method: "POST",
        body: new URLSearchParams({ "roomLight": roomLight })
      });
    }

    function setTemperature() {
      let temp = document.getElementById("tempSetup").value;
      fetch("/control", {
        method: "POST",
        body: new URLSearchParams({ "requiredTemp": temp })
      });
    }

    function addSchedule() {
      let time = document.getElementById("scheduleTime").value;
      let temp = document.getElementById("scheduleTemp").value;
      let duration = document.getElementById("scheduleDuration").value;
      fetch("/addSchedule", {
        method: "POST",
        body: new URLSearchParams({ "time": time, "temp": temp, "duration": duration })
      }).then(response => response.text()).then(data => {
        document.getElementById("scheduleList").innerHTML = data;
      });
    }

    function updateStatus() {
    fetch('/status')
    .then(response => response.json())
    .then(data => {
      // Update the room light indicator
      const lightIndicator = document.getElementById('lightIndicator');
      if (data.roomLight) {
        lightIndicator.classList.remove('off');
        lightIndicator.classList.add('light-on');
      } else {
        lightIndicator.classList.remove('light-on');
        lightIndicator.classList.add('off');
      }

      // Update the cooler indicator
      const coolerIndicator = document.getElementById('coolerIndicator');
      if (data.cooler) {
        coolerIndicator.classList.remove('off');
        coolerIndicator.classList.add('cooler-on');
      } else {
        coolerIndicator.classList.remove('cooler-on');
        coolerIndicator.classList.add('off');
      }

      // Update the heater indicator
      const heaterIndicator = document.getElementById('heaterIndicator');
      if (data.heater) {
        heaterIndicator.classList.remove('off');
        heaterIndicator.classList.add('heater-on');
      } else {
        heaterIndicator.classList.remove('heater-on');
        heaterIndicator.classList.add('off');
      }
    });
}

// Call the updateStatus function every 0.5 seconds to refresh the status indicators
setInterval(updateStatus, 500);
  </script>
</body>
</html>)rawliteral";

// Function to format schedules as an HTML list
String formatScheduleList() {
    String list = "<ul>";
    for (const Schedule &schedule : schedules) {
        list += "<li>" + String(schedule.hour) + ":" + String(schedule.minute) + " - " + String(schedule.scheduledTemp) + "°C for " + String(schedule.duration) + " minute/s</li>";
    }
    list += "</ul>";
    return list;
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(HEATER_PIN, OUTPUT);
  pinMode(COOLER_PIN, OUTPUT);
  pinMode(ROOM_LIGHT_PIN, OUTPUT);
  pinMode(BLUE_LED_PIN, OUTPUT);

  WiFi.begin("xxxx", "zzzzz");

  // Blink the blue LED while connecting to Wi-Fi
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(BLUE_LED_PIN, HIGH);
    delay(500);
    digitalWrite(BLUE_LED_PIN, LOW);
    delay(500);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi!");
  digitalWrite(BLUE_LED_PIN, LOW);  // Turn off LED before NTP synchronization

  // Initialize NTP and synchronize time
  initNTP();
  
  // After NTP sync, keep the blue LED glowing
  digitalWrite(BLUE_LED_PIN, HIGH);  // Steady glow
  Serial.println(WiFi.localIP());

  // Web server route to load interface
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", webPage);
  });

  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{\"currentTemp\":" + String(currentTemp, 1) + "}"; // Limit to 1 decimal place
  request->send(200, "application/json", json);
});

  // API to control system power, room light, and temperature
  server.on("/control", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("systemPower", true)) {
      String power = request->getParam("systemPower", true)->value();
      systemPower = power == "on";
    }
    if (request->hasParam("roomLight", true)) {
      String light = request->getParam("roomLight", true)->value();
      roomLightState = light == "on";
      digitalWrite(ROOM_LIGHT_PIN, roomLightState ? HIGH : LOW);
    }
    if (request->hasParam("requiredTemp", true)) {
      requiredTemp = request->getParam("requiredTemp", true)->value().toInt();
    }
    request->send(200, "text/plain", "OK");
  });

  // API to add a new schedule
  server.on("/addSchedule", HTTP_POST, [](AsyncWebServerRequest *request){
  if (request->hasParam("time", true) && request->hasParam("temp", true) && request->hasParam("duration", true)) {
    String time = request->getParam("time", true)->value();
    int temp = request->getParam("temp", true)->value().toInt();
    int duration = request->getParam("duration", true)->value().toInt();

    int hour = time.substring(0, 2).toInt();
    int minute = time.substring(3, 5).toInt();

    schedules.push_back({hour, minute, temp, duration}); // Add duration to schedule
    request->send(200, "text/html", formatScheduleList());
  }
});

server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
  String json = "{\"roomLight\":" + String(roomLightState ? "true" : "false") + 
                ", \"heater\":" + String(digitalRead(HEATER_PIN) == HIGH ? "true" : "false") + 
                ", \"cooler\":" + String(digitalRead(COOLER_PIN) == HIGH ? "true" : "false") + "}";
  request->send(200, "application/json", json);
});

  server.begin();
}

void loop() {
  currentTemp = dht.readTemperature();
  if (systemPower) {
    
    checkAndApplySchedule();  // Check schedule every loop

    const int temp = isScheduleActive ? scheduledTemp : requiredTemp;

    if (currentTemp > temp) {
      digitalWrite(HEATER_PIN, LOW);  // Turn off heater
      digitalWrite(COOLER_PIN, HIGH); // Turn on cooler
    } 
    else if (currentTemp < temp) {
      digitalWrite(COOLER_PIN, LOW);  // Turn off cooler
      digitalWrite(HEATER_PIN, HIGH); // Turn on heater
    } 
    else {
      digitalWrite(HEATER_PIN, LOW); //Turn off heater
      digitalWrite(COOLER_PIN, LOW); //Turn off cooler
    }


  } else {
    digitalWrite(HEATER_PIN, LOW); //Turn off heater
    digitalWrite(COOLER_PIN, LOW); //Turn off cooler
  }

  delay(2000); // Check every 2 seconds
}
 