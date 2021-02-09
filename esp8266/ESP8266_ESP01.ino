#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>

 //this make your device vulnerable when exposed to public, used only in dev
#include <ESP8266HTTPUpdateServer.h>


ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

#define SOCKET_ONE 2
#define SOCKET_TWO 3
#define SOCKET_THREE 4
#define SOCKET_FOUR 5
#define SOCKET_FIVE 6
#define SOCKET_SIX 7
#define SOCKET_SEVEN 8
#define SOCKET_EIGHT 9

#define FEEDER_R1 11
#define LEVEL 10

int myStaticIp [4] = {192, 168, 0, 51};
int myGateway [4] = {192, 168, 0, 1};
int mySubnet [4] = {255, 255, 255, 0};
int myDns [4] = {192, 168, 0, 1};
int firstDot;
int secondDot;
int thirdDot;

const char* deviceName = "Aquarium v3.5";
String ssid = "config";
String password = "12345678";
String userName = "admin";
String userPass = "1234";
boolean useUserPass = false;
const char* host = "esp8266-webupdate";

String localToken = String(ESP.getChipId()) + String(ESP.getFreeHeap() * ESP.random() + millis());
long lastTokenUpdate = 0;

int softwareVersion = 10;

typedef struct {
  boolean defined;
  boolean used;
  int timeOn;
  int timeOff;
} ElementSchedule;

typedef struct {
  String name;
  int pin;
  ElementSchedule schedules[3];
} Socket;

typedef struct {
  boolean defined;
  boolean used;
  int time;
  int duration;
} FeederSchedule;

typedef struct {
  String name;
  String action;
  FeederSchedule schedules[3];
} Feeder;

typedef struct {
  boolean defined;
  boolean used;
  int action;//1-> start, 0-> stop
  int socket;
  int sensor;//Temperature" 0 "Level" 1 "Ph" 2
  String operand;
  float value;
  int mode;//1-> ignore, 0-> dont ignore
} Rule;

typedef struct {
  String sockets;
  int stopMinutes;
  boolean isStop;
} FeedingRule;

Rule rules[19];
FeedingRule feedingRule = {"9", 5, false};

int isSummerTime = 1; //1 -> true, 0 ->false
float timeZone = 2;

float zeroLevel = 0;
float ph_c = 0;
float ph_m = 0;

boolean temperatureRuleIsPresent =  false;
boolean levelRuleIsPresent = false;
boolean phRuleIsPresent = false;
boolean rulesInitialized = false;

//default elements
Socket socketOne = {"Socket 1", SOCKET_ONE, {{true, false, 0, 0}, {false, false, 0, 0}, {false, false, 0, 0}}};
Socket socketTwo = {"Socket 2", SOCKET_TWO,  {{true, false, 0, 0}, {false, false, 0, 0}, {false, false, 0, 0}}};
Socket socketThree = {"Socket 3", SOCKET_THREE,  {{true, false, 0, 0}, {false, false, 0, 0}, {false, false, 0, 0}}};
Socket socketFour = {"Socket 4", SOCKET_FOUR,  {{true, false, 0, 0}, {false, false, 0, 0}, {false, false, 0, 0}}};
Socket socketFive = {"Socket 5", SOCKET_FIVE,  {{true, false, 0, 0}, {false, false, 0, 0}, {false, false, 0, 0}}};
Socket socketSix = {"Socket 6", SOCKET_SIX,  {{true, false, 0, 0}, {false, false, 0, 0}, {false, false, 0, 0}}};
Socket socketSeven = {"Socket 7", SOCKET_SEVEN,  {{true, false, 0, 0}, {false, false, 0, 0}, {false, false, 0, 0}}};
Socket socketEight = {"Socket 8", SOCKET_EIGHT,  {{true, false, 0, 0}, {false, false, 0, 0}, {false, false, 0, 0}}};

Socket sockets[] = {socketOne, socketTwo, socketThree, socketFour, socketFive, socketSix, socketSeven, socketEight};

Feeder feeder = {"Feeder", "feeding", {{true, false, 190000, 3}, {false, false, 0, 0}, {false, false, 0, 0}}};
String controllerName = "Controller";

//runtime variables
const byte SIZE_OF_ARRAYS = 15; //it has to be at least max pin number + 1
boolean isOn[SIZE_OF_ARRAYS] = {};
boolean unhandledUpdates = false;

//feeding runtine variables
boolean feedingDone[] = {false, false, false};
boolean stopedWhileFeeding[SIZE_OF_ARRAYS] = {};
int feedingRuleSockets[8] = {};
int timeOffWhileFeeding;

long lastCheck = 0;

float lastLevel = 0;
long ruleCheckInterval = 60000;
float lastRuleCheck = 0;

const int maxEvents = 60;
String events[maxEvents];
String formattedTime = "0";

void handleActionRequest() {
  String response = "{\"response\":\"ko\"}";
  int responseCode = 200;
  String responseType = "application/json";
  if (server.hasArg("action")) {
    String action = server.arg("action");

    if (server.hasHeader("Authorization")) {
      String receivedToken = server.header("Authorization");
      receivedToken.trim();
      if (receivedToken.equals("Bearer " + localToken)) {
        if (server.hasArg("pin")) {
          int pin = server.arg("pin").toInt();
          String action = server.arg("action");
          if (action.equals("on")) {
            turnOn(pin);
            response = "{\"status\":\"ON\"}";
            addEvent("Manual: ON " + String(getName(pin)));
          }
          if (action.equals("off")) {
            turnOff(pin);
            response = "{\"status\":\"OFF\"}";
            addEvent("Manual: OFF " + String(getName(pin)));
          }

          if (action.equals("status")) {
            response = "{\"status\":\"" + getStatus(pin) + "\"}";
          }
        }

        if (action.equals("feeding")) {
          doFeeding(3);
          response = "{\"response\":\"ok\"}";
          addEvent("Manual: Feeding done!");
        }

        if (action.equals("getSockets")) {
          response = getSockets(true);
        }


        if (action.equals("getFeeder")) {
          response = getFeeder();
        }

        if (action.equals("getRules")) {
          response = getRules();
        }

        if (action.equals("getFeedingRule")) {
          response = getFeedingRule();
        }

        if (action.equals("getGeneralSettings")) {
          response = getGeneralSettings();
        }

        if (action.equals("getSensorsReadings")) {
          response = getSensorsReadings();
        }

        if (action.equals("updateGeneral")) {
          response = updateGeneral();
        }

        if (action.equals("updateSockets")) {
          response = updateSockets();
        }

        if (action.equals("updateFeeding")) {
          response = updateFeeding();
        }

        if (action.equals("updateRules")) {
          response = updateRules();
        }

        if (action.equals("restart")) {
          ESP.restart();
        }

        if (action.equals("events")) {
          int offset = (server.hasArg("offset") ? (server.arg("offset").toInt() == 0 ? 0 : server.arg("offset").toInt()) : 0);
          int numberOfEvents = (server.hasArg("numberOfEvents") ? server.arg("numberOfEvents").toInt() : 10);
          response = getEvents(offset, numberOfEvents);
        }

      } else {
        response = "";
        responseCode = 401;
      }
    } else {
      responseCode = 401;
    }

    if (action.equals("validate")) {
      response = "false";
      responseCode = 401;
      responseType = "plain/text";
      if (server.hasHeader("Authorization")) {
        String receivedToken = server.header("Authorization");
        receivedToken.trim();
        if (receivedToken.equals("Bearer " + localToken)) {
          response = "true";
          responseCode = 200;
        }
      }
    }

    if (action.equals("login")) {
      response = "";
      responseCode = 401;
      responseType = "plain/text";
      if (server.hasArg("user") && server.hasArg("pass")) {
        String receivedUser = server.arg("user");
        String receivedPass = server.arg("pass");
        if (receivedUser.equals(userName) && receivedPass.equals(userPass)) {
          response = localToken;
          responseCode = 200;
          //addEvent("Succes login, user " + receivedUser + " !");
        } else {
          addEvent("Fail login attempt, user " + receivedUser + " !");
        }
      }
    }
    //this need to be protected somehow
    if (action.equals("updateNetwork") && ssid.equals("config") && password.equals("12345678")) {
      response = updateNetwork();
    }
  }

  readAllFromSerial();
  server.send(response.equals("{\"response\":\"ko\"}") ? 404 : responseCode, responseType , response);
}


String getSockets(boolean includeStatus) {
  String response = "[";
  for (byte i = 0; i < 8; i = i + 1) {
    if (i != 0) {
      response = response + ",";
    }
    String line = "{\"name\":\"" + sockets[i].name + "\",\"pin\":" + sockets[i].pin;
    line = line + ",\"schedules\":[";
    boolean isFirst = true;
    for (byte j = 0; j < 3; j = j + 1) {
      if (sockets[i].schedules[j].defined)
      {
        if (!isFirst) {
          line = line + ",";
        }
        line = line + "{\"timeOn\":\"" + jsonTime(sockets[i].schedules[j].timeOn) +
               "\",\"timeOff\":\"" + jsonTime(sockets[i].schedules[j].timeOff) +
               "\",\"used\":" + (sockets[i].schedules[j].used == true ?  "true" : "false") + "}";
        isFirst = false;
      }
    }
    line = line + "]";

    if (includeStatus) {
      line = line + (",\"status\":\"" + getStatus(sockets[i].pin) + "\"");
    }
    line = line + "}";
    response = response + line;
  }
  response = response + "]";
  return response;
}

String getFeeder() {
  String response = "{\"name\":\"" + feeder.name +
                    "\",\"action\":\"" + feeder.action +
                    "\",\"schedules\":[";
  boolean firstElement = true;
  for (byte j = 0; j < 3; j = j + 1) {

    if (feeder.schedules[j].defined)
    {
      if (!firstElement) {
        response = response + ",";
      }
      response = response + "{\"time\":\"" + jsonTime(feeder.schedules[j].time) + "\",\"used\":" + (feeder.schedules[j].used == true ?  "true" : "false") +
                 + ",\"duration\":" + String(feeder.schedules[j].duration) + "}";
      firstElement = false;
    }
  }
  response = response + "]}";
  return response;
}

String getRules() {
  String response = "[";
  boolean firstElement = true;
  for (byte i = 0; i < 19; i = i + 1) {
    if (rules[i].defined) {
      if (!firstElement) {
        response += ",";
      }
      String line = "{\"used\":";
      line += (rules[i].used == true ?  "true" : "false");
      line += ",\"action\":";
      line += rules[i].action;
      line += ",\"socket\":";
      line += rules[i].socket;
      line += ",\"sensor\":";
      line += rules[i].sensor;
      line += ",\"operand\":\"";
      line += rules[i].operand;
      line += "\",\"value\":";
      line += rules[i].value;
      line += ",\"mode\":";
      line += rules[i].mode;
      line += "}";
      response += line;
      firstElement = false;
    }
  }
  response += "]";
  return response;
}

String getFeedingRule() {
  String response = "{\"sockets\":[" + feedingRule.sockets + "],\"stopMinutes\":" + feedingRule.stopMinutes + ",\"isStop\":" + (feedingRule.isStop == true ?  "true" : "false") + "}";
  return response;
}

String getGeneralSettings() {
  String response = "{\"controllerName\":\"" + controllerName;
  response += "\",\"isSummerTime\":";
  response += (isSummerTime == 1 ?  "true" : "false");
  response += (",\"selectedTimeZone\":" + String(timeZone));
  response += (",\"ph_c\":" + String(ph_c));
  response += (",\"ph_m\":" + String(ph_m));
  response += (",\"serialNo\":" + String(ESP.getChipId()));
  response += (",\"version\":" + String(softwareVersion));
  response += "}";
  return response;
}

String getSensorsReadings() {
  String response = "{\"temperature\":" + String(getSensor("Temperature"));
  response = response + ",\"level\":" + String(getLevel());
  response = response + ",\"ph\":" + String(getPh());
  response = response + "}";
  return response;
}

//this is useful when using relays NC (Nominal Close) for things that will be on most of the time
boolean isNO(int pin) {
  if (pin == SOCKET_EIGHT) {
    return false;
  }
  return true;
}

float getPh() {
  float mvPh = getSensor("Ph");
  return (ph_m * mvPh) + ph_c;
}

float getLevel() {
  float response = getSensor("Level");
  response = (float)((response - zeroLevel) * -1);
  return response;
}

float getSensor(String name) {
  float response = -999;
  readAllFromSerial();
  Serial.println("get" + name + "(0)");
  long beginTime = millis();
  while ((!Serial.available() > 0) && (millis() - beginTime < 3000))
  {
    delay(50);
  }
  if (Serial.available() > 0) {
    response = (Serial.readString().toFloat());
  }
  return response;
}

String getStatus(int pin) {
  String response = (isOn[pin] == true ? "ON" : "OFF");
  return response;
}

String getRealStatus(int pin) {
  String response = "UNKNOWN";
  readAllFromSerial();
  Serial.println("digitalRead(" + String(pin) + ")");
  long beginTime = millis();
  while ((!Serial.available() > 0) && (millis() - beginTime < 3000))
  {
    delay(50);
  }
  if (Serial.available() > 0) {
    if (Serial.readString() == "LOW") {
      if (isNO(pin)) {
        response = "ON";
      } else {
        response = "OFF";
      }
    } else {
      if (isNO(pin)) {
        response = "OFF";
      } else {
        response = "ON";
      }
    }
  }
  response = (isOn[pin] == true ? "ON" : "OFF");
  return response;
}

String getEvents(int offset, int numberOfEvents) {
  String response = "[";
  boolean isFirst = true;
  if (offset < maxEvents)
  {
    for (byte i = offset; i < maxEvents; i ++) {
      if (numberOfEvents == 0 ) {
        break;
      }
      if (events[i].length() > 0) {
        if (!isFirst) {
          response += ",";
        }
        numberOfEvents --;
        response += (events[i]);
        isFirst = false;
      }
    }
  }
  response += "]";
  return response;
}

String jsonTime(int time) {
  if (time == 0)
  {
    return "00:00";
  }
  float timeWhitoutSeconds = time / 100;
  float fTime = timeWhitoutSeconds / 100;
  String strTime = String(fTime);
  strTime.replace(".", ":");
  return strTime;
}

void handleWifi() {
  //Static IP address configuration
  loadWifiSettings();
  IPAddress gateway(myGateway[0], myGateway[1], myGateway[2], myGateway[3]);
  IPAddress subnet(mySubnet[0], mySubnet[1], mySubnet[2], mySubnet[3]);
  IPAddress dns(myDns[0], myDns[1], myDns[2], myDns[3]);
  IPAddress staticIP(myStaticIp[0], myStaticIp[1], myStaticIp[2], myStaticIp[3]);

  WiFi.disconnect();
  delay(1000);
  WiFi.hostname(deviceName); 
  WiFi.mode(WIFI_STA);
  if (!(ssid.equals("config") && password.equals("12345678"))) {
    WiFi.config(staticIP, dns, gateway, subnet);
  }

  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

int getCurrentTime() {
  while (!timeClient.update()) {
    timeClient.forceUpdate();
  }
  formattedTime = timeClient.getFormattedTime();
  formattedTime.replace(":", "");
  return formattedTime.toInt();
}

void handleElement(int now, Socket item) {
  for (byte i = 0; i < 3; i = i + 1) {
    if (item.schedules[i].used && !(item.schedules[i].timeOn == item.schedules[i].timeOff)) {
      if (shouldDoAction(now, item.schedules[i].timeOn) && !isOn[item.pin]) {
        addEvent("Turning on " + item.name + "!");
        turnOn(item.pin);
      }

      if (shouldDoAction(now, item.schedules[i].timeOff) && isOn[item.pin]) {
        addEvent("Turning off " + item.name + "!");
        turnOff(item.pin);
      }
    }
  }
}

void handleElements(int now) {
  for (byte i = 0; i < 8; i ++) {
    handleElement(now, sockets[i]);
  }
  handleFeeding(now);
  handleStoppedSocketsWhileFeeding();
}

void handleFeeding(int timeNow) {
  for (byte i = 0; i < 3; i ++) {
    if (!feedingDone[i] && feeder.schedules[i].used && shouldDoAction(timeNow, feeder.schedules[i].time)) {
      doFeeding(feeder.schedules[i].duration);
      feedingDone[i] = true;
      addEvent("Automatic feeding done!");
    }
  }

  //reset feeding evidence
  if (shouldDoAction(timeNow, 235900)) {
    feedingDone[0] = false;
    feedingDone[1] = false;
    feedingDone[2] = false;
  }
}

void doFeeding(int duration) {
  stopWhileFeeding();
  turnOn(FEEDER_R1);//power on relay
  delay(duration * 1000);
  readAllFromSerial();
  turnOff(FEEDER_R1);//power off relay
  delay(1000);
  readAllFromSerial();
}

//feeding rule actions
void handleStoppedSocketsWhileFeeding() {
  if (feedingRule.isStop) {
    for (byte i = 0; i < SIZE_OF_ARRAYS; i ++) {
      if (stopedWhileFeeding[i]) {
        int diff = millis() - timeOffWhileFeeding;
        if (diff > feedingRule.stopMinutes * 60 * 1000) {
          stopedWhileFeeding[i] = false;
          timeOffWhileFeeding = 0;
          turnOn(i);
          addEvent("Turning on " + getName(i) + ", after feeding!");
          delay(200);
          readAllFromSerial();
        }
      }
    }
  }
}

void stopWhileFeeding() {
  if (feedingRule.isStop) {
    for (byte i = 0; i < 8; i ++) {
      int pin = feedingRuleSockets[i];
      if ( pin > 0) {
        if (!stopedWhileFeeding[pin]) {
          addEvent("Turning off " + getName(pin) + " for " + feedingRule.stopMinutes + " min while feeding");
        }
        turnOff(pin);
        stopedWhileFeeding[pin] = true;
        timeOffWhileFeeding = millis();
        delay(200);
        readAllFromSerial();
      }
    }
  }
}

int shouldDoAction(int timeNow, int actionTime) {
  int diference = timeNow - actionTime;
  if (diference > 0 && diference < 59) {
    return true;
  }
  return false;
}

void addEvent(String event) {
  //shift events to free the first position
  for (byte i = maxEvents - 1; i > 0; i = i - 1) {
    events[i] = events[i - 1];
  }
  String entry = "{\"time\":\"" + String(timeClient.getFormattedTime()) + "\",\"event\":\"" + event + "\"}";
  events[0] = entry;
}

int startupElemements() {
  int now = getCurrentTime();
  for (byte i = 0; i < 8; i ++) {
    startupElement(now, sockets[i]);
  }
}

void commandOn(int pin) {
  if (isNO(pin)) {
    Serial.println("digitalWrite(" + String(pin) + ",LOW)");
  } else {
    Serial.println("digitalWrite(" + String(pin) + ",HIGH)");
  }
}

void turnOn(int pin) {
  String response = "UNKNOWN";
  for (byte i = 0; i < 3; i ++) {
    long beginTime = millis();
    commandOn(pin);
    delay(1000);
    while ((!Serial.available() > 0) && (millis() - beginTime < 2000))
    {
      delay(50);
    }
    if (Serial.available() > 0) {
      response = Serial.readString();
    }
    if (!response.equals("UNKNOWN")) {
      break;
    }
  }

  if (response.equals("UNKNOWN")) {
    addEvent("Failed turning on " + getName(pin));
  } else {
    isOn[pin] = true;
  }
}

void turnOff(int pin) {
  String response = "UNKNOWN";


  for (byte i = 0; i < 3; i ++) {
    long beginTime = millis();
    commandOff(pin);
    delay(1000);
    while ((!Serial.available() > 0) && (millis() - beginTime < 2000))
    {
      delay(50);
    }
    if (Serial.available() > 0) {
      response = Serial.readString();
    }
    if (!response.equals("UNKNOWN")) {
      break;
    }
  }

  if (response.equals("UNKNOWN")) {
    addEvent("Failed turning off " + getName(pin));
  } else {
    isOn[pin] = false;
  }
}

void commandOff(int pin) {
  if (isNO(pin)) {
    Serial.println("digitalWrite(" + String(pin) + ",HIGH)");
  } else {
    Serial.println("digitalWrite(" + String(pin) + ",LOW)");
  }
}

void handleRules(long now) {
  if (millis() - lastRuleCheck > ruleCheckInterval) {
    lastRuleCheck = millis();
    float phValue = phRuleIsPresent ? getPh() : 0;
    float levelValue = levelRuleIsPresent ? getLevel() : -999;
    float temperatureValue = temperatureRuleIsPresent ? getSensor("Temperature") : 0;

    for (byte i = 0; i < 19; i ++) {
      if (rules[i].defined && rules[i].used && (rules[i].mode == 1 || isInInterval(rules[i].socket, now))) {
        float ruleValue = rules[i].value;
        float compareValue;

        String sensorName;
        if (rules[i].sensor == 0) {
          compareValue = temperatureValue;
          sensorName = "temperature";
          temperatureRuleIsPresent = true;
        } else if (rules[i].sensor == 1) {
          compareValue = levelValue;
          sensorName = "level";
          float thisLevelRead = compareValue;
          float levelDiff = thisLevelRead - lastLevel;
          lastLevel = thisLevelRead;
          if (levelDiff > 0.5 || levelDiff < (-0.5)) {
            ruleCheckInterval = 2500;
          } else if (levelDiff > 0.3 || levelDiff < (-0.3)) {
            ruleCheckInterval = 5000;
          } else {
            ruleCheckInterval = 60000;
          }
          levelRuleIsPresent = true;
        } else {
          compareValue = phValue;
          sensorName = "ph";
          phRuleIsPresent = true;
        }
        if (!rulesInitialized) {
          rulesInitialized = true;
          return;
        }

        if ((compareValue < ruleValue && rules[i].operand.equals("less")) ||
            (compareValue > ruleValue && rules[i].operand.equals("greater"))) {
          if (rules[i].action == 0 && isOn[rules[i].socket]) {
            turnOff(rules[i].socket);
            addEvent("Rule: Turning off " + getName(rules[i].socket) + ", " + sensorName + " is " + String(compareValue) + "!");
          }

          if (rules[i].action == 1 && !isOn[rules[i].socket]) {
            turnOn(rules[i].socket);
            addEvent("Rule: Turning on " + getName(rules[i].socket) + ", " + sensorName + " is " + String(compareValue) + "!");
          }
        }
      }
    }
  }
}

void startupElement(int now, Socket item) {
  if (isInInterval(item.pin, now)) {
    addEvent("Turning on " + item.name + "!");
    turnOn(item.pin);
  } else {
    turnOff(item.pin);
  }
  delay(1000);
  readAllFromSerial();
}

boolean isInInterval(int pin, long now) {
  for (byte i = 0; i < 8; i ++) { //the size of the structure
    if (sockets[i].pin == pin) {
      for (byte j = 0; j < 3; j ++) {
        if (sockets[i].schedules[j].used) {
          if (sockets[i].schedules[j].timeOff >= sockets[i].schedules[j].timeOn) {
            if ((now > sockets[i].schedules[j].timeOn && now < sockets[i].schedules[j].timeOff)
                || sockets[i].schedules[j].timeOn == sockets[i].schedules[j].timeOff) {
              return true;
            }
          } else {
            if (now > sockets[i].schedules[j].timeOn || now < sockets[i].schedules[j].timeOff) {
              return true;
            }
          }
        }
      }
    }
  }
  return false;
}

String getName(int pin) {

  for (byte i = 0; i < 8; i ++) { //the size of the structure
    if (sockets[i].pin == pin) {
      return sockets[i].name;
    }
  }

  if (pin == FEEDER_R1) {
    return feeder.name;
  }
  if (pin == LEVEL) {
    return "Level";
  }

  return "UNKNOWN";
}

void readAllFromSerial() {
  while (Serial.available() > 0) {
    Serial.readString();
  }
}

String updateSockets() {
  String socketsToSave = "";
  for (byte i = 0; i < 8; i ++) { //the size of the structure
    socketsToSave += addParam("name_" + String(sockets[i].pin), server.arg("name_" + String(sockets[i].pin)));
    for (byte j = 0; j < 3; j ++) {
      if (server.hasArg("used_" + String(sockets[i].pin) + '_' + j)) {
        socketsToSave += addParam("defined_" + String(sockets[i].pin) + '_' + j, String(true));
        socketsToSave += addParam("used_" + String(sockets[i].pin) + '_' + j, String(server.arg("used_" + String(sockets[i].pin) + '_' + j).equals("true")));
        String timeOn = server.arg("timeOn_" + String(sockets[i].pin) + '_' + j);
        timeOn.replace(":", "");
        timeOn = timeOn + "00";
        socketsToSave += addParam("timeOn_" + String(sockets[i].pin) + '_' + j, timeOn);
        String timeOff = server.arg("timeOff_" + String(sockets[i].pin) + '_' + j);
        timeOff.replace(":", "");
        timeOff = timeOff + "00";
        socketsToSave += addParam("timeOff_" + String(sockets[i].pin) + '_' + j, timeOff);
      } else {
        socketsToSave += addParam("defined_" + String(sockets[i].pin) + '_' + j, String(false));
        socketsToSave += addParam("used_" + String(sockets[i].pin) + '_' + j, String(false));
        socketsToSave += addParam("timeOn_" + String(sockets[i].pin) + '_' + j, String(0));
        socketsToSave += addParam("timeOff_" + String(sockets[i].pin) + '_' + j, String(0));
      }
    }
  }

  writeToFile("/sockets.config", socketsToSave, false);

  addEvent("Done writing sockets updates!");

  unhandledUpdates = true;
  return "{\"status\":\"ok\"}";
}

String updateRules() {
  String rulesToSave = "";
  writeToFile("/rules.config", rulesToSave, false);
  for (byte i = 0; i < 19; i ++) {
    if (server.hasArg("r" + String(i) + "_used")) {
      rulesToSave += addParam("r" + String(i) + "_defined", String(true));
      rulesToSave += addParam("r" + String(i) + "_used", String(server.arg("r" + String(i) + "_used").equals("true")));
      rulesToSave += addParam("r" + String(i) + "_action", server.arg("r" + String(i) + "_action"));
      rulesToSave += addParam("r" + String(i) + "_socket", server.arg("r" + String(i) + "_socket"));
      rulesToSave += addParam("r" + String(i) + "_sensor", server.arg("r" + String(i) + "_sensor"));
      rulesToSave += addParam("r" + String(i) + "_operand", server.arg("r" + String(i) + "_operand"));
      rulesToSave += addParam("r" + String(i) + "_value", server.arg("r" + String(i) + "_value"));
      rulesToSave += addParam("r" + String(i) + "_mode", server.arg("r" + String(i) + "_mode"));
    } else {
      rulesToSave += addParam("r" + String(i) + "_defined", String(false));
      rulesToSave += addParam("r" + String(i) + "_used", String(false));
      rulesToSave += addParam("r" + String(i) + "_action", "0");
      rulesToSave += addParam("r" + String(i) + "_socket", "2");
      rulesToSave += addParam("r" + String(i) + "_sensor", "0");
      rulesToSave += addParam("r" + String(i) + "_operand", "less");
      rulesToSave += addParam("r" + String(i) + "_value", "0");
      rulesToSave += addParam("r" + String(i) + "_mode", "0");
    }

    if (i == 6 || i == 12) {
      Serial.println(rulesToSave);
      writeToFile("/rules.config", rulesToSave, true);
      rulesToSave = "";
    }
  }

  writeToFile("/rules.config", rulesToSave, true);

  addEvent("Done writing rules updates!");

  return "{\"status\":\"ok\"}";
}

String updateFeeding() {
  //feeder
  String feederToSave = addParam("name_f", server.arg("name_f"));
  for (byte i = 1; i < 4; i ++) {
    if (server.hasArg("used" + String(i) + "_f")) {
      feederToSave += addParam("defined" + String(i) + "_f", String(true));
      feederToSave += addParam("used" + String(i) + "_f", String(server.arg("used" + String(i) + "_f").equals("true")));
      String f_time = server.arg("time" + String(i) + "_f");
      f_time = f_time + "00";
      f_time.replace(":", "");
      feederToSave += addParam("time" + String(i) + "_f", f_time);
      feederToSave += addParam("duration" + String(i) + "_f", server.arg("duration" + String(i) + "_f"));
    } else {
      feederToSave += addParam("defined" + String(i) + "_f", String(false));
      feederToSave += addParam("used" + String(i) + "_f", String(false));
      feederToSave += addParam("time" + String(i) + "_f", String(0));
      feederToSave += addParam("duration" + String(i) + "_f", String(0));
    }
  }

  writeToFile("/feeding.config", feederToSave, false);

  //feeding rules
  String feedingRuleToSave = addParam("fr_stopMinutes", String(server.arg("fr_stopMinutes")));
  feedingRuleToSave += addParam("fr_isStop", String(server.arg("fr_isStop").equals("true")));
  String frSockets = server.arg("fr_sockets");
  frSockets.replace(".", ",");
  feedingRuleToSave += addParam("fr_sockets", frSockets);

  writeToFile("/feeding.config", feedingRuleToSave, true);

  addEvent("Done writing feeder, feeding rule updates!");

  return "{\"status\":\"ok\"}";
}

String updateGeneral() {
  String generalToSave = addParam("controllerName", server.arg("controllerName"));
  generalToSave += addParam("isSummerTime", String(server.arg("isSummerTime").equals("true")));
  generalToSave += addParam("timeZone", server.arg("selectedTimeZone"));
  generalToSave += addParam("zeroLevel", String(server.hasArg("zeroLevel") ? ((getSensor("Level") + getSensor("Level")) / 2) : zeroLevel));
  generalToSave += addParam("ph_c", server.hasArg("ph_c") ? server.arg("ph_c") : String(ph_c));
  generalToSave += addParam("ph_m", server.hasArg("ph_m") ? server.arg("ph_m") : String(ph_m));

  writeToFile("/options.config", generalToSave, false);
  addEvent("Done writing general updates!");
  return "{\"status\":\"ok\"}";
}

String addParam(String key, String value) {
  return key + ":" + value + ";";
}

void loadElements() {
  loadGeneral();
  loadSockets();
  loadRules();
  loadFeeding();
}

void loadFeeding() {
  File file = SPIFFS.open("/feeding.config", "r");
  String conf = "";
  if (file) {
    while (file.available()) {
      conf = file.readString();
    }
    file.close();
  } else {
    return;
  }

  if (conf.length() > 0) {
    //feeder
    feeder.name = getMyString(conf, "name_f");
    for (byte i = 0; i < 3; i ++) {
      feeder.schedules[i].defined = getBoolean(conf, "defined" + String(i + 1) + "_f");
      feeder.schedules[i].used = getBoolean(conf, "used" + String(i + 1) + "_f");
      feeder.schedules[i].time = getInt(conf, "time" + String(i + 1) + "_f");
      feeder.schedules[i].duration = getInt(conf, "duration" + String(i + 1) + "_f");
    }

    //feeding rule
    feedingRule.isStop = getBoolean(conf, "fr_isStop");
    feedingRule.stopMinutes = getInt(conf, "fr_stopMinutes");
    feedingRule.sockets = getMyString(conf, "fr_sockets");
    setFeedingRuleSockets();
  }
}

void loadGeneral() {
  File file = SPIFFS.open("/options.config", "r");
  String conf = "";
  if (file) {
    while (file.available()) {
      conf = file.readString();
    }
    file.close();
  } else {
    return;
  }

  if (conf.length() > 0) {
    isSummerTime = getBoolean(conf, "isSummerTime");
    timeZone = getInt(conf, "timeZone");
    controllerName = getMyString(conf, "controllerName");
    zeroLevel = getFloat(conf, "zeroLevel");
    ph_c = getFloat(conf, "ph_c");
    ph_m = getFloat(conf, "ph_m");
  }
}

void loadRules() {
  File file = SPIFFS.open("/rules.config", "r");
  String conf = "";
  if (file) {
    while (file.available()) {
      conf = file.readString();
    }
    file.close();
  } else {
    return;
  }

  if (conf.length() > 0) {
    //rules
    for (byte i = 0; i < 19; i ++) {
      rules[i].defined = getBoolean(conf, "r" + String(i) + "_defined");
      rules[i].used = getBoolean(conf, "r" + String(i) + "_used");
      rules[i].action = getInt(conf, "r" + String(i) + "_action");
      rules[i].socket = getInt(conf, "r" + String(i) + "_socket");
      rules[i].sensor = getInt(conf, "r" + String(i) + "_sensor");
      rules[i].operand = getMyString(conf, "r" + String(i) + "_operand");
      rules[i].value = getFloat(conf, "r" + String(i) + "_value");
      rules[i].mode = getInt(conf, "r" + String(i) + "_mode");
    }
  }
}

void loadSockets() {
  File file = SPIFFS.open("/sockets.config", "r");
  String conf = "";
  if (file) {
    while (file.available()) {
      conf = file.readString();
    }
    file.close();
  } else {
    return;
  }
  if (conf.length() > 0) {
    //sockets
    for (byte i = 0; i < 8; i ++) { //the size of the structure
      sockets[i].name = getMyString(conf, "name_" + String(sockets[i].pin));
      for (byte j = 0; j < 3; j ++) {
        sockets[i].schedules[j].defined = getBoolean(conf, "defined_" + String(sockets[i].pin) + '_' + j);
        sockets[i].schedules[j].used = getBoolean(conf, "used_" + String(sockets[i].pin) + '_' + j);
        sockets[i].schedules[j].timeOn = getInt(conf, "timeOn_" + String(sockets[i].pin) + '_' + j);
        sockets[i].schedules[j].timeOff = getInt(conf, "timeOff_" + String(sockets[i].pin) + '_' + j);
      }
    }
  }
}

void setFeedingRuleSockets() {
  int startFrom = 0;
  String tmp = feedingRule.sockets.length() > 0 ? feedingRule.sockets + "," : feedingRule.sockets;
  int indexOfComma = tmp.indexOf(",");
  for (byte i = 0; i < 8; i ++) {
    if (indexOfComma > -1) {
      feedingRuleSockets[i] = (tmp.substring(startFrom, indexOfComma)).toInt();
      startFrom = indexOfComma + 1;
      indexOfComma = tmp.indexOf(",", startFrom);
    } else {
      feedingRuleSockets[i] = 0;
    }
  }
}

String getMyString(String config, String key) {
  String temp = config.substring(config.indexOf(key));
  return temp.substring(temp.indexOf(":") + 1, temp.indexOf(";"));
}

float getFloat(String config, String key) {
  return getMyString(config, key).toFloat();
}

int getInt(String config, String key) {
  return getMyString(config, key).toInt();
}

boolean getBoolean(String config, String key) {
  return getInt(config, key) == 1;
}

void loadWifiSettings() {
  File file = SPIFFS.open("/network.config", "r");
  String conf = "";
  if (file) {
    while (file.available()) {
      conf = file.readString();
    }
    file.close();
  } else {
    return;
  }

  if (conf.length() > 0) {
    String confIp = getMyString(conf, "ip");
    firstDot = confIp.indexOf(".") + 1;
    secondDot = confIp.indexOf(".", firstDot) + 1;
    thirdDot = confIp.indexOf(".", secondDot) + 1;
    myStaticIp[0] = confIp.substring(0, firstDot).toInt();
    myStaticIp[1] = confIp.substring(firstDot, secondDot).toInt();
    myStaticIp[2] = confIp.substring(secondDot, thirdDot).toInt();
    myStaticIp[3] = confIp.substring(thirdDot).toInt();

    String confGateway = getMyString(conf, "gateway");
    firstDot = confGateway.indexOf(".") + 1;
    secondDot = confGateway.indexOf(".", firstDot) + 1;
    thirdDot = confGateway.indexOf(".", secondDot) + 1;
    myGateway[0] = confGateway.substring(0, firstDot).toInt();
    myGateway[1] = confGateway.substring(firstDot, secondDot).toInt();
    myGateway[2] = confGateway.substring(secondDot, thirdDot).toInt();
    myGateway[3] = confGateway.substring(thirdDot).toInt();

    String confSubnet = getMyString(conf, "subnet");
    firstDot = confSubnet.indexOf(".") + 1;
    secondDot = confSubnet.indexOf(".", firstDot) + 1;
    thirdDot = confSubnet.indexOf(".", secondDot) + 1;

    String confDns = getMyString(conf, "dns");
    firstDot = confDns.indexOf(".") + 1;
    secondDot = confDns.indexOf(".", firstDot) + 1;
    thirdDot = confDns.indexOf(".", secondDot ) + 1;
    myDns[0] = confDns.substring(0, firstDot).toInt();
    myDns[1] = confDns.substring(firstDot, secondDot).toInt();
    myDns[2] = confDns.substring(secondDot, thirdDot).toInt();
    myDns[3] = confDns.substring(thirdDot).toInt();

    ssid = getMyString(conf, "ssid");
    password = getMyString(conf, "pass");
    userName = getMyString(conf, "userName");
    userPass = getMyString(conf, "userPass");
    useUserPass = getBoolean(conf, "useUserPass");
  }
}

void writeToFile(String fileName, String toSave, boolean append) {
  File file = SPIFFS.open(fileName, (append ? "a" : "w"));
  if (file) {
    file.print(toSave);
    file.close();
  }
}

String updateNetwork() {
  String toSave = "";
  toSave = toSave + addParam("ssid", server.arg("ssid"));
  toSave = toSave + addParam("pass", server.arg("pass"));
  toSave = toSave + addParam("ip", server.arg("ip"));
  toSave = toSave + addParam("dns", server.arg("dns"));
  toSave = toSave + addParam("subnet", server.arg("subnet"));
  toSave = toSave + addParam("gateway", server.arg("gateway"));
  toSave = toSave + addParam("useUserPass", String(server.arg("useUserPass")));
  toSave = toSave + addParam("userPass", server.arg("userPass"));
  toSave = toSave + addParam("userName", server.arg("user"));
  saveNetworkSettings(toSave);
  return "{\"status\":\"ok\"}";
}

void saveNetworkSettings(String toSave) {
  File file = SPIFFS.open("/network.config", "w");
  if (file) {
    file.print(toSave);
    file.close();
    addEvent("Done updating network settings");
  } else {
    addEvent("Error updating network settings");
  }
  ESP.restart();
}

void renewApiToken() {
  if (millis() - lastTokenUpdate > 86400000) {
    lastTokenUpdate = millis();
    localToken = String(ESP.getChipId()) + String(ESP.getFreeHeap() * ESP.random() + millis());
  }
}

void setup(void) {
  SPIFFS.begin();
  Serial.setTimeout(50);
  Serial.begin(115200);

  //initialize first rule
  rules[0] = (Rule) {
    true, false, 0, 2, 0, "greater", 25.50, 0
  };

  handleWifi();

  if (!(ssid.equals("config") && password.equals("12345678"))) {
    server.serveStatic("/", SPIFFS, "/homepage.html");
  } else {
    server.serveStatic("/", SPIFFS, "/settings.html");
  }

  MDNS.begin(host);
  
  //this make your device vulnerable when exposed to public, used only in dev
  httpUpdater.setup(&server);

  server.on("/action", handleActionRequest);
  server.begin();
  MDNS.addService("http", "tcp", 80);

  if (!(ssid.equals("config") && password.equals("12345678"))) {
    timeClient.begin();
    loadElements();
    int offset = (isSummerTime + timeZone) * 3600;
    timeClient.setTimeOffset(offset);
    timeClient.forceUpdate();
    startupElemements();
  }
  addEvent("Done starting!");
}

void loop(void) {
  MDNS.update();
  server.handleClient();
  renewApiToken();

  if (!(ssid.equals("config") && password.equals("12345678"))) {
    if (WiFi.status() != WL_CONNECTED) {
      delay(60000);
      handleWifi();
    }
    int thisMoment  = getCurrentTime();
    handleElements(thisMoment);
    handleRules(thisMoment);
  }
}
