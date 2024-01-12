/*
Config file for board including user changable variables
*/


#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  600        /* Time ESP32 will go to sleep (in seconds) */

#define SONAR_NUM 1      // Number of sensors.
#define MAX_DISTANCE 350 // Maximum distance (in cm) to ping.

//name, id and mqtt state topic
const String name = "tank_monitor"; //unused to maintain easy identification
const String id = name + "_" + String(ESP.getEfuseMac(), HEX); //primary ID, should be unique
const String topic = "home/tank_monitor/" + id;
const String otaTopic = topic + "/ota";
const String stateTopic = topic + "/state";
const String discTopic = "homeassistant/sensor/" + id;
const char* mqtt_server = "10.0.0.23";


//physical measurements ADJUST FOR INDIVIDUAL TANK DIMENSIONS
const int tankAdjust = 47; //distance from the bottom of the ultrasonic tranducer to the full water level mark
const int tankTotal = 27250;    //total water volume in L
const float tankDiameter = 3.65;
const float tankRadius = tankDiameter/2;
const float tankArea = M_PI * (tankRadius*tankRadius); //tank area m2

const char index_html[] PROGMEM = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><meta http-equiv=\"X-UA-Compatible\" content=\"ie=edge\"><title>Tank Monitor configuration</title><link rel=\"stylesheet\" href=\"style.css\"></head><body><script src=\"index.js\"></script><h1>Tank Monitor Configuration</h1><br><a href=\"/update\"><button>Update Firmware</button></a><h2>Device Settings</h2><p>Device Name:<br>ID:<br>Topic:<br>otaTopic:<br>StateTopic:<br>discTopic:<br>MQTT Server:<br></p><br><h2>Sleep Settings</h2><p>Time to sleep (s):</p><br><h2>Tank Settings</h2><p>Tank size (litres):<br>Distance to full water mark:<br>Tank Diameter:</p></body></html>"; // large char array, tested with 14k