
#ifdef ESP32
#include <WiFi.h>             // WIFI for ESP32
#include <WiFiManager.h>
#else
#include <ESP8266WiFi.h>      // WIFI for ESP8266
#include <WiFiClient.h>
#include <WiFiManager.h>
#endif

#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <NewPing.h>  //for sonar modules


#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  600        /* Time ESP32 will go to sleep (in seconds) */


#define SONAR_NUM 1      // Number of sensors.
#define MAX_DISTANCE 350 // Maximum distance (in cm) to ping.

RTC_DATA_ATTR int bootCount = 0; // set boot count so we know not to transmit on initial load

//function to generate chipId
uint32_t chipId = 0;
uint32_t getChipId(){
  for(int i=0; i<17; i=i+8) {
  chipId = ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  } 
  return chipId;
}

//declare mqtt info and set ID
const String name = "tank_monitor";
const String id = name + "_" + String(getChipId(), HEX);
String stateTopic = "home/" + id + "/state";
const char* mqtt_server = "10.0.0.23";
const char* mqttUser = "env";
const char* mqttPassword = "WqoXY96LZ8JYdz";

//set wifi parameters and create instances
const char* ssid = "EagleNetwork";
const char* password = "whiskas3136";
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

//physical measurements
const int mainTotal = 27250;
const float mainArea = 10.46347;

//provide adjustment distance to full water level
const int mainAdjust = 46;
const int unitAdjust = 50;

const int unitTotal = 22500;
const float unitArea = 9.953820;

//create dynamic variables

int mainDistance;
int mainVol;
int mainCapacity;
float mainPercent;

int unitDistance;
int unitVol;
int unitCapacity;
float unitPercent;

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
void connectToWifi() {
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
  String discoveryTopic = "homeassistant/sensor/" + name + "/main_percent/config";

  DynamicJsonDocument doc(2048);
  char buffer[2048];

  doc["uniq_id"] = name + "_main_percent";
  doc["name"] = name + " Main Tank Percentage";
  doc["stat_t"]   = stateTopic;
  doc["unit_of_meas"] = "%";
  doc["frc_upd"] = true;
  doc["val_tpl"] = "{{ value_json.main_percent|default(0) }}";
  
  JsonObject dev = doc.createNestedObject("dev");
  dev["ids"] = name;
  dev["name"] = name;
  dev["mf"] = "Pidgeon Systems";
  dev["sw"] = "1.0";

  size_t n = serializeJson(doc, buffer);

  client.publish(discoveryTopic.c_str(), buffer, n);
}

void sendMQTTCapacityDiscoveryMsg() {
  String discoveryTopic = "homeassistant/sensor/" + name + "/main_capacity/config";

  DynamicJsonDocument doc(2048);
  char buffer[2048];

  doc["uniq_id"] = name + "_main_capacity";
  doc["name"] = name + " Main Tank Capacity";
  doc["stat_t"]   = stateTopic;
  doc["unit_of_meas"] = "L";
  doc["frc_upd"] = true;
  doc["val_tpl"] = "{{ value_json.main_capacity|default(0) }}";
  
  JsonObject dev = doc.createNestedObject("dev");
  dev["ids"] = name;
  dev["name"] = name;
  dev["mf"] = "Pidgeon Systems";
  dev["sw"] = "1.0";

  size_t n = serializeJson(doc, buffer);

  client.publish(discoveryTopic.c_str(), buffer, n);
}

void sendMQTTDistanceDiscoveryMsg() {
  String discoveryTopic = "homeassistant/sensor/" + name + "/main_distance/config";

  DynamicJsonDocument doc(2048);
  char buffer[2048];

  doc["uniq_id"] = name + "_main_distance";
  doc["name"] = name + " Main Tank Distance";
  doc["stat_t"]   = stateTopic;
  doc["unit_of_meas"] = "cm";
  doc["frc_upd"] = true;
  doc["val_tpl"] = "{{ value_json.main_distance|default(0) }}";
  
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

  mainDistance = sonar[0].convert_cm(sonar[0].ping_median(10)); //find median of 10 pings in cm

  //calculate volume
  mainVol =  (((mainDistance-mainAdjust)*10)*mainArea);
  mainCapacity = mainTotal - mainVol;
  mainPercent = ((float)mainCapacity/(float)mainTotal)*100;

  // Print the distance on the Serial Monitor (Ctrl+Shift+M):
  Serial.print("Main tank Distance = ");
  Serial.print(mainDistance);
  Serial.println(" cm");

  Serial.print("Main Litres = ");
  Serial.print(mainCapacity);
  Serial.print(" litres, aka ");
  Serial.print(mainPercent);
  Serial.println(" %");


  doc["main_distance"] = mainDistance;
  doc["main_percent"] = mainPercent;
  doc["main_capacity"] = mainCapacity;
  size_t n = serializeJson(doc, buffer);

  bool published = client.publish(stateTopic.c_str(), buffer, n);
}

void setup() {
  //pinMode(powerPin, OUTPUT);
  //digitalWrite(powerPin, HIGH);
  delay(500);
  Serial.begin(115200);

  if (bootCount == 0) { //manual reset or reload, dont transmit rain count
    Serial.println("Initial boot, connecting to wifi (to check for saved STA), mqtt and send discovery  then going straight to sleep");
    Serial.println("Configuring WiFi");
    connectToWifi();
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
  connectToWifi();
  setupMQTT();
  sendData();

  delay(1000);  //allow some time for data to transmit before jumping into deep sleep
  deepSleep();
}

void loop() {
}