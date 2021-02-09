#include <NewPing.h>
#include <DallasTemperature.h>
#include <OneWire.h>

#define PH_PIN A0
#define ONE_WIRE_BUS A1
#define TRIGGER_PIN  A2
#define ECHO_PIN     A3
#define MAX_DISTANCE 100
 
NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE);
OneWire oneWire(ONE_WIRE_BUS);  
DallasTemperature sensors(&oneWire);
int buf[500];
int counter = 0;

void setup() {
  sensors.begin();
  sensors.requestTemperatures();
  Serial.setTimeout(50);
  Serial.begin(115200);
  initializePins();
}

void loop() {
  if(Serial.available()>0){
    String recived = Serial.readString();
    if(recived && recived.length() > 0){
      Serial.print(handleCommand(recived));
    }	
  }
  readPh();
}

void readPh(){
	buf[counter]= analogRead(PH_PIN);
	delay(30);
	if(counter == 499){
		//reset
		counter = 0;
	} else {
		counter = counter + 1;
	}
}

String handleCommand(String command){
  String result = "";
  //ex: digitalWirite(12,LOW) | digitalRead(12)
  String action = command.substring(0, command.indexOf('('));
  String pin = command.substring(command.indexOf('(') + 1, command.indexOf(','));
  pin.replace(")","");
  String value = command.substring(command.indexOf(',') + 1, command.lastIndexOf(')'));
  if(supportedAction(action)){
    result = handleAction(action, pin, value);
  }  
  return result;
}

String handleAction(String action, String pinNumber, String value){
   if(action.equals("analogRead")){
      String result = String(analogRead(pinNumber.toInt()));
      return result;
   }
   if(action.equals("digitalWrite")){  
      digitalWrite(pinNumber.toInt(), value.equals("HIGH") ? HIGH : LOW);
      return value;
    }
    if(action.equals("digitalRead")){
      String result = digitalRead(pinNumber.toInt()) == 1 ? "HIGH" : "LOW";
      return result;
    }
    if(action.equals("pinMode")){
      pinMode(pinNumber.toInt(), value.equals("OUTPUT") ? OUTPUT :INPUT);
      return value;
    }
    
    //getTemperature(0);
    if(action.equals("getTemperature")){
      return getTemperature();
    }
     
    //getLevel(0);
    if(action.equals("getLevel")){
      return getLevel();
    }

    //getPh(0);
    if(action.equals("getPh")){
      return getPh();
    }    
  return "FAIL: Action: " + action + " not supported";
}

boolean supportedAction(String action) {
  return action.equals("digitalRead")||
         action.equals("digitalWrite") ||
         action.equals("pinMode") ||
         action.equals("analogRead") || 
         action.equals("getTemperature") ||
         action.equals("getLevel")||
         action.equals("getPh");        
}

String getTemperature() {
  sensors.requestTemperatures();
  return String(sensors.getTempCByIndex(0));
}

String getLevel() {
  return String((float)sonar.ping_median(30)/29.0/2);
}

String getPh() {
unsigned long int avgValue;
int temp;
  avgValue=0;
  for(int i=0;i<500;i++) {                   
    avgValue+=buf[i];
  }
  float phVoltage = (float)(avgValue*(5.0 / 1024.0)/500);
  return String(phVoltage);
}

void initializePins(){
	for (int i = 2; i < 12; i++) {
		pinMode(i, OUTPUT);
		digitalWrite(i, HIGH);
	}
}
