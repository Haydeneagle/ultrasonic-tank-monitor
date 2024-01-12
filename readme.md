# Ultrasonic Tank Monitor using ESP32-C3

## Overview

This project is a wireless water tank monitoring solution, based on the ESP32-C3 microcontroller. 

To measure the water level, the AJ-SR04M waterproof ultrasonic sensor is used, measuring the height of the water level. This measured height value combined with the tank diameter and height of the sensor above the full water mark allows for the volume of the tank to be calculated.

Currently, the data is transmitted over Wi-Fi using the MQTT protocol to communicate with a Mosquitto broker linked to Home Assistant, allowing for the data to be easily visualized and used in home automations. In the near future I will likely move to a LoRa based system however currently I do not have a gateway, so provided Wi-Fi signal is adequate, it works well.

A custom PCB has been created to interface the ESP32-C3 with a number of ports, while also optimizing for low power usage in deep sleep with a 70uA quiescent current. The board is powered by a single Li-ion 4.2V 18650 cell and supports charging by both 5V USB, and via the solar input.

![A picture of a unit fully assembled ready to be installed](/images/assembled.jpg)
![A picture of a unit installed into a tank](/images/installed.jpg)

## Features
* WiFiManager for simple first time Wi-Fi setup
* OTA updating
* Simple installation and construction
* Low 70uA quiescent current allowing for 30+ day battery life
* Dual USB/Solar charge input
* +- 1 CM accuracy in measurement
* Simple communications for integration with MQTT for various uses
* Built in battery protection
* Optional support for water temperature sensor



## Hardware
The entire PCB project can be found in the Altium folder in this repo. This includes source files for modification, but also prezipped Gerbers for manufacturing. To manufacture this board you can submit these to a website such as JLCPCB (who I use) or PCBWay, both of which who are very cheap suppliers. The components were purchased from LCSC I opted to hand solder the entire PCB, however in future I would order a stencil to apply paste and use a reflow oven as it was tedious and led to numerous errors.

Revision 2 of the PCB is underway, fixing a number of issues with the original including:
* Correcting USB pair connections (were reversed) 
* Solar input can power circuit directly (was preventing battery charger IC from finishing charge)
* Correct board shape to fit into 76x76 enclosure
* Possibly switching to SMPS to maximize battery usage

### Primary Components
The primary ICs/modules used are
* ESP32-C3-WROOM-02-N4
* AP7361C-33E-13 1A 3.3V LDO
* TP4056 Lithium battery charger IC
* SKCL3130ME Battery protection IC
* TPS2116DRLR Power switch for peripherials

### Shopping List
I have listed below the items needed to recreate this project along with estimated associated costing in $AUD and including shipping. Obviously this can vary a lot so use it as an estimate.

1. Fully assembled ESP32-C3 custom PCB $20 (entire PCBA hand soldered)
2. Li-ion 4.2V 18650 cell (I have used Samsung INR18650-25R harvested out of a powertool battery)
3. [AJ-SR04M waterproof ultrasonic sensor](https://www.aliexpress.com/item/4001116678728.html?spm=a2g0o.order_list.order_list_main.5.41e31802lgvw9L) $6
4. [Adaptable Box 108mm x 108mm x 76mm](https://www.sparkydirect.com.au/p/NLS-30092-Adaptable-Box-108mm-x-108mm-x-76mm) $9
5. [1" BSP Thread Pipe Nipple](https://www.bunnings.com.au/philmac-1-bsp-thread-pipe-nipple_p4813780) $3
6. [5V 250mA Solar Panel 69x110mm](https://www.aliexpress.com/item/32906698984.html?spm=a2g0o.order_list.order_list_main.5.4acf1802ziiZYF) $3
7. [Clear silicone sealant](https://www.bunnings.com.au/parfix-300g-clear-all-purpose-silicone_p1232674)
Optional
8. [DS18B20 stainless Steel temperature sensor](https://www.aliexpress.com/item/4000068914916.html?spm=a2g0o.order_list.order_list_main.11.527418024KgMyr) $3

In addition to these parts, you will need a 5mm drill bit and holesaw for the solar input hole, and a 32mm holesaw for the nipple hole in the bottom of the enclosure. Clear silicone will then need to be used to seal up and holes onces fully assembled.

## Setup
All necessary software configuration takes place in the config.h file. In future this will be via a web based page to facilitate easy modification once installed, however currently it is hard coded via the firmware. 
1. First, you will need to configure the specific tank measurements to suit your tank, this includes the total volume, diameter and the height the ultrasonic sensor is from the full water level (might be easier to update once installation is complete). 
2. The second configuration is the name and MQTT broker information to allow it to communicate successfully.
3. On initial startup of the board, the device will broadcast a Wi-Fi hotspot for you to connect to. Upon connecting, you need to browse to 192.168.4.1 on your connected device. Here you will see the WiFiManager interface. Select your SSID and input the password and press save. The device will reboot and save the credentials for future use. 
4. The device should now connect successfully and publish its first measurement.
5. To enter OTA mode, you will need to publish the message "on" to the topic specified in otaTopic. Ensure this message is set to retained. On next wakeup, the device will begin transmitting rapidly while also hosting the OTA update page (and in future the configuration page)
6. To exit OTA mode, publish the retained message "off" and the device should enter deep sleep.



## TO-DO
* PCB Revision 2
* Web based configuration page
