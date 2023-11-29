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
const int tankTotal = 27250;    //total water volume in L
const float tankArea = 10.46347; //tank area m2
const int tankAdjust = 0; //distance from the bottom of the ultrasonic tranducer to the full water level mark
