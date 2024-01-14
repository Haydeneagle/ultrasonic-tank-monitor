
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

//construct objects, credentials stores in secret.h
WiFiClient wifiClient;
PubSubClient client(wifiClient);

//pin definitions
#define periphPower 3 //pin must be set to high to switch 3v3 to peripherials
#define trigPin 5
#define echoPin 4
#define ledPin 10 //active low

//declare object of sonar
NewPing sonar[SONAR_NUM] = {   // Sensor object array.
NewPing(trigPin, echoPin, MAX_DISTANCE), // Each sensor's trigger pin, echo pin, and max distance to ping. 
};

AsyncWebServer server(80); //for ota updates

//create dynamic variables
int distance; //ultrasonic measurement distance
int tankVol; //tank maximum volume (L)
int capacity; //calculated water volume (L)
float percent; //percent remaining water

uint64_t lastPublish = esp_timer_get_time(); // used by publish
bool deepSleepDisable = 0;

void startOTA(){
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", index_html);
  });

  ElegantOTA.setAuth(otaUser, otaPass);
  ElegantOTA.begin(&server);
  server.begin();
  Serial.println("HTTP server started");
}

void deepSleep() {
    Serial.println("entering Deep sleep mode");
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR); //constants defined in config.h

    //Serial.flush(); // unused as was blocking code when no serial terminal connected...

    digitalWrite(ledPin, HIGH); //just ensure led is definitely off
    digitalWrite(periphPower, LOW); //ensure peripherials are off

    client.disconnect(); //disconnect from broker cleanly
    wifiClient.flush(); //flush wifi before sleep

    while( client.state() != -1){  
    Serial.println(client.state());
    delay(10);
    }
    esp_wifi_stop(); //force close wifi to be sure its off, drops from 1.6ma to 0.1ma
    esp_deep_sleep_start();
}

// Wifi Manager
void setupWifi() {
  Serial.println("Connecting to WiFi");
  WiFiManager wifiManager;

  String HOTSPOT = id;
  wifiManager.setSaveConnectTimeout(10); //set for redundancy due to constant connect errors. Might have been USB power supply rated, as no problemms on battery
  wifiManager.setConnectTimeout(10); 
  wifiManager.setConnectRetries(5); // we have plenty of battery life and this will rule out any issues with long range connections
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

  // checks message on /ota topic to see whether to enter OTA update mode, or return to deep sleep
  if (String(topic) == otaTopic.c_str()) {
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
  client.setBufferSize(512);
  client.setKeepAlive(60);

    int counter = 0;
    while (!client.connected())
    {
      Serial.print("Attempting MQTT connection...");

      // Attempt to connect to mqtt
      if (client.connect(id.c_str(), mqttUser, mqttPassword))
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
  StaticJsonDocument<128> doc;
  char buffer[128];

  //poll distance
  pinMode(periphPower, OUTPUT);
  digitalWrite(periphPower, HIGH);  //turn on for data then low after sensing
  delay(40);  //allow time for power up 10ms too low, 25 enough but allow 40 just to be safe
  distance = sonar[0].convert_cm(sonar[0].ping_median(10)); //find median of 10 pings in cm
  delay(40);  //allow time for measurements before shutting power off
  digitalWrite(periphPower, LOW);

  //calculate volume
  tankVol =  (((distance-tankAdjust)*10)*tankArea);
  capacity = tankTotalVol - tankVol;
  percent = ((float)capacity/(float)tankTotalVol)*100;

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
    Serial.println("deepSleepDisable == 1");
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

//this code will only run when /ota topic is set to on, otherwise it will not enter loop()
void loop() {
  ElegantOTA.loop(); //has to be in loop to allow for reboot

  if (!client.connected()) { //if lost mqtt connection, reconnect
    Serial.println("Connection lost.. reconnecting");
    setupMQTT();
  }
  client.loop();//loop to subscribe to /ota topic for disable

  int TimeToPublish = 5000000; //5000000uS (5s)
  if ( (esp_timer_get_time() - lastPublish) >= TimeToPublish ) //transmit every 5s during active ota mode
    {
      sendData();// data for mqtt publish
      lastPublish = esp_timer_get_time(); // get next publish time
      Serial.println(lastPublish/1000000); //debugging to check timings

      Serial.println(client.state());
    }

  //enter deep sleep as soon as /ota topic changed
  if (deepSleepDisable == 0) {
    deepSleep();
  }
}