# Aquarium Controller
Aquarium controller made with ESP8266 ESP01 and Aduino Nano. The ESP8266 ESP01 act as a http server and as a routines manager and the Aduino Nano as an executor, they comunicate over Serial. The controller can command 8 AC sockets, can read sensors like: temperature, ph and ultrasound for distance (water level). Based on the sensor reads there can be set rulles to command the sockets. The UI is made with AngularJs and Boostrap, the communication between fron-end and back-end is made with RestAPI calls. The configuration is stored in files on ESP8266 ESP01.

## Hardware parts
The listed hardware parts should be connected as in CircuitSchematics.png, an working example can be seen in HarwareExample.jpg. Warning, working with high voltage without proper knowlege and protection equipment can kill you.
- 1 x ESP8266 ESP01
- 1 x Aduino Nano
- 1 x Power supply 5V 2Amps
- 1 x AMS1117 3.3V 5V DC-DC Step-Down power supply module
- 1 x 1-Channel Relay Module, 5V
- 2 x 4-Channels Relay Module, 5V
- 1 x PH-4502C
- 1 x BNC PH Probe
- 1 x HC-SR04
- 1 x DS18B20, Waterproof temperature sensor
- 1 x Motor DC 3V-6V with gearbox 1:48
- 8 x AC Modular Sockets
- Wires
