#include <Arduino.h>
#include "pinout.h"
#include "calibration.h"

// Timing Variables
unsigned long lightLastTime = 0;
unsigned long voltageLastTime = 0;
unsigned long requestTimer = 0;
unsigned long reqestLastTime = 0;
unsigned long flashTime = 0;
unsigned long reqLastChangeTime = 0;
unsigned long overrideTime = 0;



//Global References
uint16_t lightLevel = 0;
uint8_t lightEnabled = 1; // True if light sensor connected.
float voltage = 0;
uint8_t lightRequest = 0;
uint8_t reqStatePrev = 0;
uint8_t state = SWITCHED_OFF;
uint8_t lightOverride = 0;

//Filter Varialbes
float voltageAlpha = 0;
float voltageBeta = 0;
float lightAlpha = 0;
float lightBeta = 0;

//Prototypes
void lightSensorInit();
void lightSensorRead();

void voltageReadInit();
void voltageRead();
void readLightReq();

void stateMachine();
void stateTo(uint8_t nextState);
void printState(uint8_t st);



void setup() {

  //Calculate Alpha Filter Multipliers
  voltageAlpha = (float)1 / (float)VOLTAGE_FILTER_LENGTH;
  voltageBeta = (float)1 - (float)voltageAlpha;
  lightAlpha = (float)1 / (float)LIGHT_FILTER_LENGTH;
  lightBeta = (float)1 - (float)lightAlpha;

  if(DEBUG){
    Serial.begin(115200);
    delay(5);
    Serial.println(F("Initialising, DEBUG enabled"));
    Serial.print(F("Voltage Averaged over (ms): "));
    Serial.println(VOLTAGE_CHECK_INTERVAL * VOLTAGE_FILTER_LENGTH);
    Serial.print(F("Light Averaged over (ms): "));
    Serial.println(LIGHT_CHECK_INTERVAL * LIGHT_FILTER_LENGTH);
  }

  pinMode(V_BATT_PIN, INPUT);
  pinMode(LIGHT_REQ_PIN, INPUT);
  pinMode(LIGHT_SENSE_PIN, INPUT);

  pinMode(LIGHT_OUT_PIN, OUTPUT);

  // Initialise Light Sensor
  lightSensorInit();
  voltageReadInit();

}

void loop() {
  readLightReq(); // Sample the switch as fast as possible
  stateMachine(); // Run state machine as fast as possible - to respond to switch

  if(millis() - voltageLastTime > VOLTAGE_CHECK_INTERVAL){
    voltageRead();
    voltageLastTime = millis();
  }

  if(lightEnabled && (millis() - lightLastTime > LIGHT_CHECK_INTERVAL)){
    lightSensorRead();
    lightLastTime = millis();
  }
}

//Functions
void lightSensorInit(){
  lightLevel = analogRead(LIGHT_SENSE_PIN);
  if(lightLevel>1000){
    lightEnabled = 0;
    lightLevel = 0; // Full brightness for further calculations.
  }
  if(DEBUG) Serial.print(F("Light Sensor initialised to: "));
  if(DEBUG) Serial.println(lightLevel);
}

void lightSensorRead(){
  lightLevel = lightLevel*lightBeta + analogRead(LIGHT_SENSE_PIN)*lightAlpha;
  // if (DEBUG)
  //   Serial.print(F("Light Sensor Value: "));
  // if (DEBUG)
  //   Serial.println(lightLevel);
}

void voltageReadInit(){
  voltage = analogRead(V_BATT_PIN) * VOLTAGE_COUNT_CAL;
  if (DEBUG)
    Serial.print(F("Voltage initialised to: "));
  if (DEBUG)
    Serial.println(voltage);
}

void voltageRead(){
  // voltage = analogRead(V_BATT_PIN) * VOLTAGE_COUNT_CAL;
  voltage = (voltage*voltageBeta + (analogRead(V_BATT_PIN)*VOLTAGE_COUNT_CAL)*voltageAlpha);
  // if (DEBUG)
  //   Serial.print(F("Voltage Value: "));
  // if (DEBUG)
  //   Serial.println(voltage);
}

void readLightReq(){
  // Debounced Check of Dipped Beam Request
  // uint8_t req = digitalRead(LIGHT_REQ_PIN);
  uint8_t req = analogRead(LIGHT_REQ_PIN) >200;

  if(req != reqStatePrev){
    reqLastChangeTime = millis();
    reqStatePrev = req;
  }else if(reqLastChangeTime){ // stops the extra calculations after a change has been actioned
    if(millis() - reqLastChangeTime > 20){
      //Debounced signal switch:
      lightRequest = req;
      reqLastChangeTime = 0;

      if(DEBUG) Serial.print(F("Switch: "));
      if(DEBUG) Serial.println(lightRequest);

      //Override Check
      if(!lightRequest){
        overrideTime = millis();
        if(DEBUG) Serial.println(F("Override Millis Set"));
      }else{
        if(millis() - overrideTime < OVERRIDE_TIMER)
        {
          if(DEBUG)
          {
            Serial.print(F("Override Triggered @ "));
            Serial.print(millis() - overrideTime);
            Serial.print(F("ms\n\r"));
            }
          lightOverride = 1;
          // overrideTime = 0;
        }
      }
    }
  }

  //Override check

}

void setLight(uint8_t state){
  digitalWrite(LIGHT_OUT_PIN, state);
  if(DEBUG) digitalWrite(LED_BUILTIN, state);
}

void stateMachine(){
  if(state == SWITCHED_OFF){
    // Light is off, can only be turned on by the switch.
    if(lightRequest){
      if(voltage > VOLTAGE_TRIGGER_LEVEL_UPPER){
        // Turns on if voltage is high enough
        setLight(1);
        stateTo(SWITCH_ON_LIGHT_ON);
      }else{
        // Flashes if voltage too low
        setLight(1);
        flashTime = millis();
        stateTo(FLASH);
      }
    }
    // if(lightOverride && (millis() - overrideTime > OVERRIDE_TIMER)) //Only disable override if it's timed out
      
    return;

  } else if(state == SWITCH_ON_LIGHT_ON){
    if(!lightRequest){
      setLight(0);
      stateTo(SWITCHED_OFF);
    } else if(voltage < VOLTAGE_TRIGGER_LEVEL_LOWER){ // If voltage is low
      if(lightLevel < LIGHT_TRIGGER_LEVEL_LOWER){ // AND it's light enough
      setLight(0);
      stateTo(SWITCH_ON_LIGHT_OFF);
      }
    }
    return;

  } else if(state == SWITCH_ON_LIGHT_OFF){
    if(DEBUG)Serial.println(voltage);

    if(voltage > VOLTAGE_TRIGGER_LEVEL_UPPER){
      setLight(1);
      stateTo(SWITCH_ON_LIGHT_ON);
    }
    if(lightLevel > LIGHT_TRIGGER_LEVEL_UPPER){ // It's dark
      setLight(1);
      stateTo(SWITCH_ON_LIGHT_ON);
    }
    if(lightOverride){
      setLight(1);
      stateTo(FORCE_ON);
    }
    if(!lightRequest){
      stateTo(SWITCHED_OFF);
    }
    return;

  } else if(state == FORCE_ON){
    if(!lightRequest){
      setLight(0);
      lightOverride = 0;
      stateTo(SWITCHED_OFF);
    }
    return;
  } else if(state == FLASH){
    if(millis() - flashTime > FLASH_DURATION){
      setLight(0);
      stateTo(SWITCH_ON_LIGHT_OFF);
    }
    if(lightOverride){
      setLight(1);
      stateTo(FORCE_ON);
    }
    return;
  }
}

void stateTo(uint8_t nextState){
  if(DEBUG) {

    Serial.print(F("State change from "));
    printState(state);
    Serial.print(F("\n\rto "));
    printState(nextState);
    Serial.print(F("\n\r"));
    Serial.print(F("Light Override: "));
    Serial.print(lightOverride);
    Serial.print(F("\n\r"));
    
  }
  state = nextState;
}

void printState(uint8_t st){
  switch (st)
  {
  case SWITCHED_OFF:
    Serial.print(F("SWITCHED_OFF"));
    break;
  case SWITCH_ON_LIGHT_ON:
    Serial.print(F("SWITCH_ON_LIGHT_ON"));
    break;
  case SWITCH_ON_LIGHT_OFF:
    Serial.print(F("SWITCH_ON_LIGHT_OFF"));
    break;
  case FORCE_ON:
    Serial.print(F("FORCE_ON"));
    break;
  case FLASH:
    Serial.print(F("FLASH"));
    break;
  default:
    Serial.print(F("UNKNOWN"));
    break;
  }
}