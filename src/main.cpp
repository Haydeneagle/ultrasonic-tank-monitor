
/*
this is the file 
*/
#if defined(ESP8266) // for esp8266 libs
  #include <ESP8266WiFi.h>
  #include <ESPAsyncTCP.h>
  #include <WiFiClient.h>
  #include <WiFiManager.h>
#elif defined(ESP32) // for esp32 libs
  #include <WiFi.h>
  #include <WiFiManager.h>
  #include <AsyncTCP.h>
#endif

#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <NewPing.h>  //for sonar modules
#include <config.h>
#include <secret.h>

RTC_DATA_ATTR int bootCount = 0; // set boot count so we know not to transmit on initial load

//set wifi parameters and create instances, credentials stores in secret.h
WiFiClient wifiClient;
PubSubClient client(wifiClient);
long lastMsg = 0;
char msg[50];
int value = 0;

//create variables for both trig and echo
#define trigPin 22
#define echoPin 21

//declare object of sonar
NewPing sonar[SONAR_NUM] = {   // Sensor object array.
  NewPing(22, 21, MAX_DISTANCE), // Each sensor's trigger pin, echo pin, and max distance to ping. 
};

//create dynamic variables
int tankDistance;
int tankVol;
int tankCapacity;
float tankPercent;

AsyncWebServer server(80); //for ota updates


void startOTA(){
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Hi! This is ElegantOTA AsyncDemo.");
  });

  ElegantOTA.begin(&server);
  server.begin();
  Serial.println("HTTP server started");
}

void printWakeupReason(){
  esp_sleep_wakeup_cause_t wakeupReason;

  wakeupReason = esp_sleep_get_wakeup_cause();

  switch(wakeupReason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeupReason); break;
  }
}

void deepSleep() {
    // use this to wake for troubleshooting
    //esp_sleep_enable_ext0_wakeup(GPIO_NUM_4,1); //if wake gpio goes high, exit

    Serial.println("entering Deep sleep mode");
    Serial.flush();
    client.disconnect(); //disconnect from broker, flush wifi before sleep
    wifiClient.flush();

    while( client.state() != -1){  
    Serial.println(client.state());
    delay(10);
    }
    //adc_power_release(); // doesnt exist for some reason but would likely drop my sleep current
    esp_wifi_stop(); //force close wifi to be sure its off, drops from 1.6ma to 0.1ma
    esp_deep_sleep_start();
}

// Wifi Manager
void setupWifi() {
  Serial.println("Connecting to WiFi");
  WiFiManager wifiManager;

  String HOTSPOT = id;
  wifiManager.setSaveConnectTimeout(10); //set for redundancy due to constant connect errors. Might have been USB power supply rated, as no problemms on abttery
  wifiManager.setConnectTimeout(10); 
  wifiManager.setConnectRetries(5); // we have plenty of battery life and this will rule out any issues with long range connections, in future move away from wifiManager
  wifiManager.setConfigPortalTimeout(120);
  if (!wifiManager.autoConnect((const char * ) HOTSPOT.c_str())) {
    Serial.println("failed to connect and hit timeout");
    delay(10000);
    ESP.restart();
    delay(5000);
  }

}

void disableWiFi(){
    WiFi.disconnect(true);  // Disconnect from the network
    WiFi.mode(WIFI_OFF);    // Switch WiFi off
}

void sendMQTTPercentDiscoveryMsg() {
  String discoveryTopic = "homeassistant/sensor/" + name + "/tank_percent/config";

  DynamicJsonDocument doc(2048);
  char buffer[2048];

  doc["uniq_id"] = name + "_tank_percent";
  doc["name"] = name + " tank Tank Percentage";
  doc["stat_t"]   = stateTopic;
  doc["unit_of_meas"] = "%";
  doc["frc_upd"] = true;
  doc["val_tpl"] = "{{ value_json.tank_percent|default(0) }}";
  
  JsonObject dev = doc.createNestedObject("dev");
  dev["ids"] = name;
  dev["name"] = name;
  dev["mf"] = "Pidgeon Systems";
  dev["sw"] = "1.0";

  size_t n = serializeJson(doc, buffer);

  client.publish(discoveryTopic.c_str(), buffer, n);
}

void sendMQTTCapacityDiscoveryMsg() {
  String discoveryTopic = "homeassistant/sensor/" + name + "/tank_capacity/config";

  DynamicJsonDocument doc(2048);
  char buffer[2048];

  doc["uniq_id"] = name + "_tank_capacity";
  doc["name"] = name + " tank Tank Capacity";
  doc["stat_t"]   = stateTopic;
  doc["unit_of_meas"] = "L";
  doc["frc_upd"] = true;
  doc["val_tpl"] = "{{ value_json.tank_capacity|default(0) }}";
  
  JsonObject dev = doc.createNestedObject("dev");
  dev["ids"] = name;
  dev["name"] = name;
  dev["mf"] = "Pidgeon Systems";
  dev["sw"] = "1.0";

  size_t n = serializeJson(doc, buffer);

  client.publish(discoveryTopic.c_str(), buffer, n);
}

void sendMQTTDistanceDiscoveryMsg() {
  String discoveryTopic = "homeassistant/sensor/" + name + "/tank_distance/config";

  DynamicJsonDocument doc(2048);
  char buffer[2048];

  doc["uniq_id"] = name + "_tank_distance";
  doc["name"] = name + " tank Tank Distance";
  doc["stat_t"]   = stateTopic;
  doc["unit_of_meas"] = "cm";
  doc["frc_upd"] = true;
  doc["val_tpl"] = "{{ value_json.tank_distance|default(0) }}";
  
  JsonObject dev = doc.createNestedObject("dev");
  dev["ids"] = name;
  dev["name"] = name;
  dev["mf"] = "Pidgeon Systems";
  dev["sw"] = "1.0";

  size_t n = serializeJson(doc, buffer);

  client.publish(discoveryTopic.c_str(), buffer, n);
}

void setupMQTT() {
  Serial.println("Configuring MQTT....");
  client.setServer(mqtt_server, 1883);
    //client.setCallback(callback);
    client.setBufferSize(512);
    
    int counter = 0;
    while (!client.connected())
    {
      Serial.print("Attempting MQTT connection...");

      // Attempt to connect to mqtt
      if (client.connect(name.c_str(), mqttUser, mqttPassword))
      {
        Serial.println("connected");
      }      
      else if (counter < 5) // delay and try again
      {
        counter += 1;    
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
        // Wait 2 seconds before retrying
        delay(2000);
      }
      else // go to sleep, taking longer than 10 sec
      {
        Serial.println("MQTT connection is taking too long, go to sleep and try again later.");
        deepSleep();
      }
    }
}

void sendData(){
  DynamicJsonDocument doc(2048);
  char buffer[2048];

  tankDistance = sonar[0].convert_cm(sonar[0].ping_median(10)); //find median of 10 pings in cm

  //calculate volume
  tankVol =  (((tankDistance-tankAdjust)*10)*tankArea);
  tankCapacity = tankTotal - tankVol;
  tankPercent = ((float)tankCapacity/(float)tankTotal)*100;

  // Print the distance on the Serial Monitor (Ctrl+Shift+M):
  Serial.print("tank tank Distance = ");
  Serial.print(tankDistance);
  Serial.println(" cm");

  Serial.print("tank Litres = ");
  Serial.print(tankCapacity);
  Serial.print(" litres, aka ");
  Serial.print(tankPercent);
  Serial.println(" %");


  doc["tank_distance"] = tankDistance;
  doc["tank_percent"] = tankPercent;
  doc["tank_capacity"] = tankCapacity;
  size_t n = serializeJson(doc, buffer);

  bool published = client.publish(stateTopic.c_str(), buffer, n);
}

void setup() {
  //pinMode(powerPin, OUTPUT);
  //digitalWrite(powerPin, HIGH);
  delay(500);
  Serial.begin(115200);

  Serial.println("Configuring WiFi");
  setupWifi();

  Serial.println("Starting OTA");
  startOTA();
  /*
  if (bootCount == 0) { //manual reset or reload, dont transmit rain count
    Serial.println("Initial boot, connecting to wifi (to check for saved STA), mqtt and send discovery  then going straight to sleep");
    Serial.println("Configuring WiFi");
    setupWifi();
    
    setupMQTT();
    sendMQTTPercentDiscoveryMsg();
    sendMQTTCapacityDiscoveryMsg();
    sendMQTTDistanceDiscoveryMsg();
    ++bootCount;
    deepSleep();
  }

  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));
  printWakeupReason();
  setupWifi();
  setupMQTT();
  sendData();

  delay(1000);  //allow some time for data to transmit before jumping into deep sleep
  deepSleep();
  */
}

void loop() {
  ElegantOTA.loop();
}