/*
Config file for board including user changable variables
*/


#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  5        /* Time ESP32 will go to sleep (in seconds) */

#define SONAR_NUM 1      // Number of sensors.
#define MAX_DISTANCE 350 // Maximum distance (in cm) to ping.


//function to generate chipId
uint32_t chipId = 0;
uint32_t getChipId(){
  for(int i=0; i<17; i=i+8) {
  chipId = ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  } 
  return chipId;
}
//name, id and mqtt state topic
const String name = "tank_monitor";
const String id = name + "_" + String(getChipId(), HEX);
String stateTopic = "home/" + id + "/state";
const char* mqtt_server = "10.0.0.23";


//physical measurements ADJUST FOR INDIVIDUAL TANK DIMENSIONS
const int tankTotal = 27250;
const float tankArea = 10.46347;

//provide adjustment distance to full water level - MEASURE AFTER INSTALLATION
const int tankAdjust = 46;
