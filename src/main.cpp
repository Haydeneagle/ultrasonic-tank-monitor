
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
#define periphPower 3 //pin must be set to high to switch 3v3 to peripherials
#define trigPin 5
#define echoPin 4
#define ledPin 10

//declare object of sonar
NewPing sonar[SONAR_NUM] = {   // Sensor object array.
  NewPing(trigPin, echoPin, MAX_DISTANCE), // Each sensor's trigger pin, echo pin, and max distance to ping. 
};

AsyncWebServer server(80); //for ota updates

//create dynamic variables
int distance;
int tankVol;
int capacity;
float percent;

bool deepSleepDisable = 0;

void startOTA(){
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Hi! This is ElegantOTA AsyncDemo. test");
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
    Serial.println("entering Deep sleep mode");
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR); //variables defined in config.h

    //Serial.flush(); // unused as was blocking code when no serial terminal connected...

    digitalWrite(ledPin, HIGH); //just ensure led is definitely off
    digitalWrite(periphPower, LOW); //ensure peripherials are off

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

void sendMQTTPercentDiscoveryMsg() {
  String discoveryTopic = discTopic + "/percent/config";

  DynamicJsonDocument doc(2048);
  char buffer[2048];

  doc["uniq_id"] = id + "_percent";
  doc["name"] = "Percentage";
  doc["stat_t"]   = stateTopic;
  doc["unit_of_meas"] = "%";
  doc["frc_upd"] = true;
  doc["val_tpl"] = "{{ value_json.percent|default(0) }}";
  
  JsonObject dev = doc.createNestedObject("dev");
  dev["ids"] = id;
  dev["name"] = name;
  dev["mf"] = "Pidgeon Systems";
  dev["sw"] = "1.0";

  size_t n = serializeJson(doc, buffer);

  client.publish(discoveryTopic.c_str(), buffer, n);
}

void sendMQTTCapacityDiscoveryMsg() {
  String discoveryTopic = discTopic + "/capacity/config";

  DynamicJsonDocument doc(2048);
  char buffer[2048];

  doc["uniq_id"] = id + "_capacity";
  doc["name"] = "Capacity";
  doc["stat_t"]   = stateTopic;
  doc["unit_of_meas"] = "L";
  doc["frc_upd"] = true;
  doc["val_tpl"] = "{{ value_json.capacity|default(0) }}";
  
  JsonObject dev = doc.createNestedObject("dev");
  dev["ids"] = id;
  dev["name"] = name;
  dev["mf"] = "Pidgeon Systems";
  dev["sw"] = "1.0";

  size_t n = serializeJson(doc, buffer);

  client.publish(discoveryTopic.c_str(), buffer, n);
}

void sendMQTTDistanceDiscoveryMsg() {
  String discoveryTopic = discTopic + "/distance/config";

  DynamicJsonDocument doc(2048);
  char buffer[2048];

  doc["uniq_id"] = id + "_distance";
  doc["name"] = "Distance";
  doc["stat_t"]   = stateTopic;
  doc["unit_of_meas"] = "cm";
  doc["frc_upd"] = true;
  doc["val_tpl"] = "{{ value_json.distance|default(0) }}";
  
  JsonObject dev = doc.createNestedObject("dev");
  dev["ids"] = id;
  dev["name"] = name;
  dev["mf"] = "Pidgeon Systems";
  dev["sw"] = "1.0";

  size_t n = serializeJson(doc, buffer);

  client.publish(discoveryTopic.c_str(), buffer, n);
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // Feel free to add more if statements to control more GPIOs with MQTT

  // If a message is received on the topic, you check if the message is either "on" or "off". 
  // Changes the output state according to the message
  if (String(topic) == otaTopic.c_str()) {
    Serial.print("Changing output to ");

    if (messageTemp == "on"){
      Serial.println("OTA mode on");
      deepSleepDisable = 1;
    }
    else { //(messageTemp == "off")
      Serial.println("OTA mode off");
      deepSleepDisable = 0;
    }
  }
}

void setupMQTT() {
  Serial.println("Configuring MQTT....");
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
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
        client.subscribe(otaTopic.c_str());
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

  //poll distance
  pinMode(periphPower, OUTPUT);
  digitalWrite(periphPower, HIGH);  //turn on for data then low after sensing
  delay(40);  //allow time for power up 10ms too low, 25 enough but allow 40 just to be safe
  distance = sonar[0].convert_cm(sonar[0].ping_median(10)); //find median of 10 pings in cm
  delay(40);  //allow time for measurements before shutting power off
  digitalWrite(periphPower, LOW);

  //calculate volume
  tankVol =  (((distance-tankAdjust)*10)*tankArea);
  capacity = tankTotal - tankVol;
  percent = ((float)capacity/(float)tankTotal)*100;

  // Print the distance on the Serial Monitor (Ctrl+Shift+M):
  Serial.print("tank Distance = ");
  Serial.print(distance);
  Serial.println(" cm");

  Serial.print("tank Litres = ");
  Serial.print(capacity);
  Serial.print(" litres, aka ");
  Serial.print(percent);
  Serial.println(" %");

  //set json values
  doc["distance"] = distance;
  doc["percent"] = percent;
  doc["capacity"] = capacity;
  size_t n = serializeJson(doc, buffer);

  //send to mqtt
  bool published = client.publish(stateTopic.c_str(), buffer, n);
}


void flashLED(int onTime) {   //onTime representing time LED will remain on for
  digitalWrite(ledPin, LOW);  //low for en
  delay(onTime);
  digitalWrite(ledPin, HIGH);
}

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  
  digitalWrite(ledPin, HIGH); //default led off

  setupWifi();

  setupMQTT();

  for (int i = 0; i < 10; i++) {
    client.loop();
    flashLED(100);
    delay(100);
  }
  if (deepSleepDisable == 1) {
    Serial.println("deepsleepdisable == 1");
    Serial.println("Starting OTA");
    startOTA();
    digitalWrite(ledPin, LOW); //led on for debugging, remove once working
  }
  else {
    Serial.println("deepsleepdisable == 0");
    sendMQTTPercentDiscoveryMsg();
    sendMQTTCapacityDiscoveryMsg();
    sendMQTTDistanceDiscoveryMsg();

    sendData();

    for (int i = 0; i < 5; i++) {//allow some time for data to transmit before jumping into deep sleep
    flashLED(300);
    delay(100);
  }
    deepSleep();
  }
}

void loop() {
  ElegantOTA.loop(); //allows for esp.reboot
  client.loop();
  //loop subcribe to ota topic to go straight to sleep when topic set to off
  if (deepSleepDisable == 0) {
    sendData();
    deepSleep();
  }
}