/*
Config file for board including user changable variables
*/


#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  600        /* Time ESP32 will go to sleep (in seconds) */

#define SONAR_NUM 1      // Number of sensors.
#define MAX_DISTANCE 350 // Maximum distance (in cm) to ping.

//name, id and mqtt state topic
const String name = "tank_monitor"; //unused to maintain easy identification
const String id = name + "_" + String(ESP.getEfuseMac(), HEX); //primary ID
const String topic = "home/tank_monitor/" + id;
const String otaTopic = topic + "/ota";
const String stateTopic = topic + "/state";
const String discTopic = "homeassistant/sensor/" + id;
const char* mqtt_server = "10.0.0.23";


//physical measurements ADJUST FOR INDIVIDUAL TANK DIMENSIONS
const int tankTotal = 27250;
const float tankArea = 10.46347;

//provide adjustment distance to full water level - MEASURE AFTER INSTALLATION
const int tankAdjust = 0;
