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
const int tankAdjust = 57; //distance from the bottom of the ultrasonic tranducer to the full water level mark
const int tankTotal = 22500;    //total water volume in L
const float tankDiameter = 3.56;
const float tankRadius = tankDiameter/2;
const float tankArea = M_PI * (tankRadius*tankRadius); //tank area m2

const char index_html[] PROGMEM = "<!DOCTYPE html>\n<html lang=\"en\">\n  <head>\n    <meta charset=\"UTF-8\">\n    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n    <meta http-equiv=\"X-UA-Compatible\" content=\"ie=edge\">\n    <title>Tank Monitor configuration</title>\n    <link rel=\"stylesheet\" href=\"style.css\">\n  </head>\n  <body>\n\t<script src=\"index.js\"></script>\n    <h1>Tank Monitor Configuration</h1>\n    </br>\n    <h2>Device Settings</h2>\n    <p>\n        Device Name: </br>\n        ID: </br>\n        Topic: </br>\n        otaTopic: </br>\n        StateTopic: </br>\n        discTopic: </br>\n        MQTT Server: </br>\n    </p>\n    </br>\n    <h2>Sleep Settings</h2>\n    <p>Time to sleep (s): </p>\n    </br>\n    <h2>Tank Settings</h2>\n    <p>\n        Tank size (litres): </br>\n        Distance to full water mark: </br>\n        Tank Diameter:\n    </p>\n  </body>\n</html>"; // large char array, tested with 14k