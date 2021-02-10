#Aquarium Controller
Aquarium controller made with ESP8266 ESP01 and Aduino Nano. The ESP8266 ESP01 act as a http server and as a routines manager and the Aduino Nano as an executor, they comunicate over Serial. The controller can command 8 AC sockets, can read sensors like: temperature, ph and ultrasound for distance (water level). Based on the sensor reads there can be set rulles to command the sockets. The UI is made with AngularJs and Boostrap, the communication between fron-end and back-end is made with RestAPI calls. The configuration is stored in files on ESP8266 ESP01.

##Hardware parts
