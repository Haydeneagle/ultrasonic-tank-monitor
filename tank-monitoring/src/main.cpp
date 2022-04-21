#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <NewPing.h>

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  600        /* Time ESP32 will go to sleep (in seconds) */


#define SONAR_NUM 2      // Number of sensors.
#define MAX_DISTANCE 350 // Maximum distance (in cm) to ping.


// Define variables:
const String name = "tank_monitor";
const char* ssid = "EagleNetwork";
const char* password = "whiskas3136";
const char* mqtt_server = "10.0.0.23";
const char* mqttUser = "env";
const char* mqttPassword = "WqoXY96LZ8JYdz";


//RTC_DATA_ATTR int tempMainDistance = 0;
//RTC_DATA_ATTR int tempUnitDistance = 0;

String mqttName = name;
String stateTopic = "home/" + name + "/state";

WiFiClient wifiClient;
PubSubClient client(wifiClient);
long lastMsg = 0;
char msg[50];
int value = 0;


NewPing sonar[SONAR_NUM] = {   // Sensor object array.
  NewPing(22, 21, MAX_DISTANCE), // Each sensor's trigger pin, echo pin, and max distance to ping. 
  NewPing(33, 32, MAX_DISTANCE), 
};

//create variables for both main and unit tanks
#define mainTrigPin 22
#define mainEchoPin 21
#define powerPin 13

#define unitTrigPin 33
#define unitEchoPin 32

//physical measurements
const int mainTotal = 27000;
const float mainArea = 10.026649;

//provide adjustment distance to full water level
const int mainAdjust = 0;
const int unitAdjust = 0;

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

void setup_wifi() {
  
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  int counter = 0;

  while (WiFi.status() != WL_CONNECTED) {
    counter += 1;
    delay(500);
    Serial.print(".");
    if (counter >= 20) {
      Serial.println("Wifi connection is taking too long, go to sleep and try again later.");
      esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
      Serial.flush();
      esp_deep_sleep_start();
    }
  }
}

void disableWiFi(){
    WiFi.disconnect(true);  // Disconnect from the network
    WiFi.mode(WIFI_OFF);    // Switch WiFi off
}

void sendMQTTMainTankPercentDiscoveryMsg() {
  String discoveryTopic = "homeassistant/sensor/" + name + "/main_tank_percent/config";

  DynamicJsonDocument doc(2048);
  char buffer[2048];

  doc["uniq_id"] = name + "_main_tank_percent";
  doc["name"] = name + " Main Tank Percentage";
  doc["stat_t"]   = stateTopic;
  doc["unit_of_meas"] = "%";
  doc["frc_upd"] = true;
  doc["val_tpl"] = "{{ value_json.main_tank_percent|default(0) }}";
  
  JsonObject dev = doc.createNestedObject("dev");
  dev["ids"] = name;
  dev["name"] = name;
  dev["mf"] = "Pidgeon Systems";
  dev["sw"] = "1.0";

  size_t n = serializeJson(doc, buffer);

  client.publish(discoveryTopic.c_str(), buffer, n);
}

void sendMQTTMainTankCapacityDiscoveryMsg() {
  String discoveryTopic = "homeassistant/sensor/" + name + "/main_tank_capacity/config";

  DynamicJsonDocument doc(2048);
  char buffer[2048];

  doc["uniq_id"] = name + "_main_tank_capacity";
  doc["name"] = name + " Main Tank Capacity";
  doc["stat_t"]   = stateTopic;
  doc["unit_of_meas"] = "L";
  doc["frc_upd"] = true;
  doc["val_tpl"] = "{{ value_json.main_tank_capacity|default(0) }}";
  
  JsonObject dev = doc.createNestedObject("dev");
  dev["ids"] = name;
  dev["name"] = name;
  dev["mf"] = "Pidgeon Systems";
  dev["sw"] = "1.0";

  size_t n = serializeJson(doc, buffer);

  client.publish(discoveryTopic.c_str(), buffer, n);
}

void sendMQTTUnitTankPercentDiscoveryMsg() {
  String discoveryTopic = "homeassistant/sensor/" + name + "/unit_tank_percent/config";

  DynamicJsonDocument doc(2048);
  char buffer[2048];

  doc["uniq_id"] = name + "_unit_tank_percent";
  doc["name"] = name + " Unit Tank Percentage";
  doc["stat_t"]   = stateTopic;
  doc["unit_of_meas"] = "%";
  doc["frc_upd"] = true;
  doc["val_tpl"] = "{{ value_json.unit_tank_percent|default(0) }}";
  
  JsonObject dev = doc.createNestedObject("dev");
  dev["ids"] = name;
  dev["name"] = name;
  dev["mf"] = "Pidgeon Systems";
  dev["sw"] = "1.0";

  size_t n = serializeJson(doc, buffer);

  client.publish(discoveryTopic.c_str(), buffer, n);
}

void sendMQTTUnitTankCapacityDiscoveryMsg() {
  String discoveryTopic = "homeassistant/sensor/" + name + "/unit_tank_capacity/config";

  DynamicJsonDocument doc(2048);
  char buffer[2048];

  doc["uniq_id"] = name + "_unit_tank_capacity";
  doc["name"] = name + " Unit Tank Capacity";
  doc["stat_t"]   = stateTopic;
  doc["unit_of_meas"] = "L";
  doc["frc_upd"] = true;
  doc["val_tpl"] = "{{ value_json.unit_tank_capacity|default(0) }}";
  
  JsonObject dev = doc.createNestedObject("dev");
  dev["ids"] = name;
  dev["name"] = name;
  dev["mf"] = "Pidgeon Systems";
  dev["sw"] = "1.0";

  size_t n = serializeJson(doc, buffer);

  client.publish(discoveryTopic.c_str(), buffer, n);
}

void setup() {
  pinMode(powerPin, OUTPUT);
  digitalWrite(powerPin, HIGH);
  Serial.begin(115200);


//turn on power to ultrasonic modules via bjt


delay(500);

  mainDistance = sonar[0].convert_cm(sonar[0].ping_median(10));
  delay(50);
  unitDistance = sonar[1].convert_cm(sonar[1].ping_median(10));


  //calculate volume
  mainVol =  (((mainDistance-mainAdjust)*10)*mainArea);
  mainCapacity = mainTotal - mainVol;
  mainPercent = ((float)mainCapacity/(float)mainTotal)*100;

  unitVol =  (((unitDistance-unitAdjust)*10)*unitArea);
  unitCapacity = unitTotal - unitVol;
  unitPercent = ((float)unitCapacity/(float)unitTotal)*100;


  // Print the distance on the Serial Monitor (Ctrl+Shift+M):
  Serial.print("Main tank Distance = ");
  Serial.print(mainDistance);
  Serial.println(" cm");

  Serial.print("Main Litres = ");
  Serial.print(mainCapacity);
  Serial.print(" litres, aka ");
  Serial.print(mainPercent);
  Serial.println(" %");


  Serial.println();

  // Print the distance on the Serial Monitor (Ctrl+Shift+M):
  Serial.print("Unit Distance = ");
  Serial.print(unitDistance);
  Serial.println(" cm");

  Serial.print("Unit Litres = ");
  Serial.print(unitCapacity);
    Serial.print(" litres, aka ");
  Serial.print(unitPercent);
  Serial.println(" %");
  
    setup_wifi();
    client.setServer(mqtt_server, 1883);
    //client.setCallback(callback);
    client.setBufferSize(512);

    while (!client.connected())
    {
      Serial.print("Attempting MQTT connection...");
      // Attempt to connect
      if (client.connect(mqttName.c_str(), mqttUser, mqttPassword))
      {
        Serial.println("connected");
        sendMQTTMainTankPercentDiscoveryMsg();
        sendMQTTMainTankCapacityDiscoveryMsg();
        sendMQTTUnitTankPercentDiscoveryMsg();
        sendMQTTUnitTankCapacityDiscoveryMsg();
      }
      else
      {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
        // Wait 5 seconds before retrying
        delay(5000);
      }
    }

    DynamicJsonDocument doc(2048);
    char buffer[2048];

    doc["main_distance"] = mainDistance;
    doc["main_tank_percent"] = mainPercent;
    doc["main_tank_capacity"] = mainCapacity;
    doc["unit_tank_percent"] = unitPercent;
    doc["unit_tank_capacity"] = unitCapacity;
    doc["unit_distance"] = unitDistance;

    size_t n = serializeJson(doc, buffer);

    bool published = client.publish(stateTopic.c_str(), buffer, n);
//allow 5 seconds for it to send
delay(5000);

digitalWrite(powerPin, LOW);
    Serial.println("Deep sleep mode");
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    //Serial.flush();
    esp_deep_sleep_start();
  /*}
  else {
    Serial.println("Level change insigificant, enter deep sleep mode");
    digitalWrite(powerPin, LOW);
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    //Serial.flush();
    esp_deep_sleep_start();
  }*/



}

void loop() {
}