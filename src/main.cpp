#include <Arduino.h>
#include "pinout.h"
#include "calibration.h"
#include "states.h"

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
float voltageTriggerLevelUpper = VOLTAGE_12_TRIGGER_LEVEL_UPPER;
float voltageTriggerLevelLower = VOLTAGE_12_TRIGGER_LEVEL_LOWER;
uint8_t debug = DEBUG; //Provide a software settable debug parameter

//Light Control Variables
int lightOutputLevel = 0;
int requestedLightOutputLevel = 0;
unsigned long lastLightLevelChange = 0;

//Filter Varialbes
float voltageAlpha = 0;
float voltageBeta = 0;
float lightAlpha = 0;
float lightBeta = 0;

//Prototypes
void setAnalogueReference(uint8_t source);

void lightSensorInit();
void lightSensorRead();

void voltageReadInit();
void voltageRead();
void readLightReq();

void setLight(uint8_t state);
void lightLevelCheck();

void serialCheck();

void stateMachine();
void stateTo(uint8_t nextState);
void printState(uint8_t st);

void debugState();

void setup()
{
  //protect for error and rework in PCB v0.3 PROTOTYPE
  pinMode(2, INPUT);

  //Calculate Alpha Filter Multipliers
  voltageAlpha = (float)1 / (float)VOLTAGE_FILTER_LENGTH;
  voltageBeta = (float)1 - (float)voltageAlpha;
  lightAlpha = (float)1 / (float)LIGHT_FILTER_LENGTH;
  lightBeta = (float)1 - (float)lightAlpha;

  Serial.begin(9600);
  if (debug)
  {
    delay(5);
    Serial.println(F("Initialising, DEBUG enabled"));
    Serial.print(F("Voltage Averaged over (ms): "));
    Serial.println(VOLTAGE_CHECK_INTERVAL * VOLTAGE_FILTER_LENGTH);
    Serial.print(F("Light Averaged over (ms): "));
    Serial.println(LIGHT_LEVEL_CHECK_INTERVAL * LIGHT_FILTER_LENGTH);
  }

  pinMode(V_BATT_PIN, INPUT);
  pinMode(LIGHT_REQ_PIN, INPUT);
  pinMode(LIGHT_SENSE_PIN, INPUT);

  pinMode(LIGHT_OUT_PIN, OUTPUT);

  // Initialise Light Sensor
  lightSensorInit(); // Also sets ref voltage to internal for V sensing
  voltageReadInit();
}

void loop()
{
  readLightReq(); // Sample the switch as fast as possible
  stateMachine(); // Run state machine as fast as possible - to respond to switch

  if (millis() - voltageLastTime > VOLTAGE_CHECK_INTERVAL)
  {
    voltageRead();
    voltageLastTime = millis();
  }

  if (lightEnabled && (millis() - lightLastTime > LIGHT_LEVEL_CHECK_INTERVAL))
  {
    lightSensorRead();
    lightLastTime = millis();
  }

  lightLevelCheck(); //Check and adjust light level at loop speed
  serialCheck();
}

//Functions
void setAnalogueReference(uint8_t source)
{
  if (source)
  {
    analogReference(INTERNAL); // Internal resolves to 3
  }
  else
  {
    analogReference(EXTERNAL); // External resolves to 0;
  }
  analogRead(V_BATT_PIN); // AnalogRead applies the analogReference setting above
  delayMicroseconds(10);
}

void lightSensorInit()
{
  setAnalogueReference(EXTERNAL);
  lightLevel = analogRead(LIGHT_SENSE_PIN);
  setAnalogueReference(INTERNAL);
  if (lightLevel > 1000)
  {
    lightEnabled = 0;
    lightLevel = 0; // Full brightness for further calculations.
  }
  if (debug)
    Serial.print(F("Light Sensor initialised to: "));
  if (debug)
    Serial.println(lightLevel);
}

void lightSensorRead()
{
  setAnalogueReference(EXTERNAL);
  lightLevel = lightLevel * lightBeta + analogRead(LIGHT_SENSE_PIN) * lightAlpha;
  setAnalogueReference(INTERNAL);
  // if (debug)
  //   Serial.print(F("Light Sensor Value: "));
  // if (debug)
  //   Serial.println(lightLevel);
}

void voltageReadInit()
{
  voltage = analogRead(V_BATT_PIN) * VOLTAGE_COUNT_CAL;
  if (debug)
    Serial.print(F("Voltage initialised to: "));
  if (debug)
    Serial.println(voltage);
}

void voltageRead()
{
  // voltage = analogRead(V_BATT_PIN) * VOLTAGE_COUNT_CAL;
  voltage = (voltage * voltageBeta + (analogRead(V_BATT_PIN) * VOLTAGE_COUNT_CAL) * voltageAlpha);
  // if (debug)
  //   Serial.print(F("Voltage Value: "));
  // if (debug)
  //   Serial.println(voltage);
}

void readLightReq()
{
  // Debounced Check of Dipped Beam Request
  // uint8_t req = digitalRead(LIGHT_REQ_PIN);
  uint8_t req = analogRead(LIGHT_REQ_PIN) > 200;

  if (req != reqStatePrev)
  {
    reqLastChangeTime = millis();
    reqStatePrev = req;
  }
  else if (reqLastChangeTime)
  { // stops the extra calculations after a change has been actioned
    if (millis() - reqLastChangeTime > 20)
    {
      //Debounced signal switch:
      lightRequest = req;
      reqLastChangeTime = 0;

      if (debug)
        Serial.print(F("ReadLightReq > Switch: "));
      if (debug)
        Serial.println(lightRequest);

      //Override Check
      if (!lightRequest)
      {
        overrideTime = millis();
        if (debug)
          Serial.println(F("ReadLightReq > Override Millis Set"));
      }
      else
      {
        if (millis() - overrideTime < OVERRIDE_TIMER)
        {
          if (debug)
          {
            Serial.print(F("ReadLightReq > Override Triggered @ "));
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

void setLight(uint8_t state)
{
  if (state)
  {
    requestedLightOutputLevel = 0; // Full Brightness
  }
  else
  {
    requestedLightOutputLevel = 255; // OFF
  }
  // Old non-pwm implementation
  // digitalWrite(LIGHT_OUT_PIN, !state); //Inverted for new active off design.
  // digitalWrite(LIGHT_OUT_LED_PIN, state);
}

void lightLevelCheck()
{
  if (lightOutputLevel != requestedLightOutputLevel)
  { //Light is transitioning
    if (millis() - lastLightLevelChange > LIGHT_LEVEL_CHANGE_TIME)
    {
      lastLightLevelChange = millis();
      if (requestedLightOutputLevel > lightOutputLevel)
      {
        lightOutputLevel += LIGHT_LEVEL_CHANGE_STEP;
        if (lightOutputLevel > requestedLightOutputLevel)
        {
          lightOutputLevel = requestedLightOutputLevel;
        }
      }
      else
      { // requestedLightLevel < lightOutputLevel
        lightOutputLevel -= LIGHT_LEVEL_CHANGE_STEP;
        if (lightOutputLevel < requestedLightOutputLevel)
        {
          lightOutputLevel = requestedLightOutputLevel;
        }
      }
      analogWrite(LIGHT_OUT_PIN, lightOutputLevel);
      analogWrite(LIGHT_OUT_LED_PIN, 255 - lightOutputLevel);
      // if (debug)
      //   Serial.println(lightOutputLevel);
    }
  }
}

void serialCheck()
{
  if (Serial.available())
  {
    char in = Serial.read();
    if (in == 'd' || in == 'D')
    {
      stateTo(DEBUG_STATE);
    }
  }
}

void stateMachine()
{
  if (state == SWITCHED_OFF)
  {
    // Light is off, can only be turned on by the switch.
    if (lightRequest)
    {
      if (voltage > voltageTriggerLevelUpper)
      {
        // Turns on if voltage is high enough
        setLight(1);
        stateTo(SWITCH_ON_LIGHT_ON);
      }
      else
      {
        // Flashes if voltage too low
        setLight(1); 
        flashTime = millis();
        stateTo(FLASH);
      }
    }
    lightOverride = 0;
    // if(lightOverride && (millis() - overrideTime > OVERRIDE_TIMER)) //Only disable override if it's timed out

    return;
  }
  else if (state == SWITCH_ON_LIGHT_ON)
  {
    if (!lightRequest)
    {
      setLight(0);
      stateTo(SWITCHED_OFF);
    }
    else if (voltage < voltageTriggerLevelLower)
    { // If voltage is low
      if (lightLevel < LIGHT_TRIGGER_LEVEL_LOWER)
      { // AND it's light enough
        setLight(0);
        stateTo(SWITCH_ON_LIGHT_OFF);
      }
    }
    return;
  }
  else if (state == SWITCH_ON_LIGHT_OFF)
  {

    if (voltage > voltageTriggerLevelUpper)
    {
      if (debug)
        Serial.println(voltage);
      setLight(1);
      stateTo(SWITCH_ON_LIGHT_ON);
    }
    if (lightLevel > LIGHT_TRIGGER_LEVEL_UPPER)
    { // It's dark
      if (debug)
        Serial.println(lightLevel);
      setLight(1);
      stateTo(SWITCH_ON_LIGHT_ON);
    }
    if (lightOverride)
    {
      setLight(1);
      stateTo(FORCE_ON);
    }
    if (!lightRequest)
    {
      if (debug)
        Serial.println(F("Light Request Low"));
      stateTo(SWITCHED_OFF);
    }
    return;
  }
  else if (state == FORCE_ON)
  {
    if (!lightRequest)
    {
      setLight(0);
      lightOverride = 0;
      stateTo(SWITCHED_OFF);
    }
    return;
  }
  else if (state == FLASH)
  {
    // Expects light to be on and flashtime to be set before entering state
    if (millis() - flashTime > FLASH_DURATION)
    {
      setLight(0);
      stateTo(SWITCH_ON_LIGHT_OFF);
    }
    if (lightOverride)
    {
      setLight(1);
      stateTo(FORCE_ON);
    }
    return;
  }
  else if (state == DEBUG_STATE)
  {
    // Too much code for the state function
    debugState();
    return;
  }
}

void stateTo(uint8_t nextState)
{
  if (debug)
  {

    Serial.print(F("\n---\n\rState change from "));
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

void printState(uint8_t st)
{
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
  case DEBUG_STATE:
    Serial.print(F("DEBUG_STATE"));
    break;
  default:
    Serial.print(F("UNKNOWN"));
    break;
  }
}

void debugState()
{
  // Turn off light, turn on ST light
  uint8_t light = 0;
  uint8_t lightLed = 0;
  uint8_t statusLed = 0;
  digitalWrite(LIGHT_OUT_PIN, !light);
  digitalWrite(LIGHT_OUT_LED_PIN, lightLed);
  digitalWrite(STATUS_LED_PIN, statusLed);

  Serial.println(F("\n\nDEBUG Mode - Normal functionality suspended"));
  Serial.println(F("Press h for command list, q to exit\n\n"));

  Serial.print(F("Voltage: "));
  Serial.println(voltage);

  Serial.print(F("Light Request: "));
  Serial.println(lightRequest ? "True" : "False");

  Serial.print(F("Ambient Light Level: "));
  if (lightEnabled)
  {
    Serial.println(lightLevel);
  }
  else
  {
    Serial.println(F("No Sensor"));
  }

  uint8_t inMenu = 1;
  uint8_t inSubMenu = 0;

  while (inMenu)
  {
    if (Serial.available())
    {
      char tmp = Serial.read();
      if (tmp == 'q' || tmp == 'Q')
      {
        inMenu = 0;
      }
      else if (tmp == 's' || tmp == 'S')
      {
        statusLed = !statusLed;
        digitalWrite(STATUS_LED_PIN, statusLed);
        Serial.print(F("\n\nStatus LED "));
        Serial.println(statusLed ? "ON" : "OFF");
      }
      else if (tmp == 'o' || tmp == 'O')
      {
        lightLed = !lightLed;
        digitalWrite(LIGHT_OUT_LED_PIN, lightLed);
        Serial.print(F("\n\nOutput LED "));
        Serial.println(lightLed ? "ON" : "OFF");
      }
      else if (tmp == 'l' || tmp == 'L')
      {
        readLightReq();
        if (!lightRequest)
        {
          Serial.println(F("\n\nNOTE: Light Req Voltage low - Output not powered"));
        }
        light = !light;
        digitalWrite(LIGHT_OUT_PIN, light);
        Serial.print(F("Dipped Beam Output "));
        Serial.println(light ? "OFF" : "ON");
      }
      else if (tmp == 'v' || tmp == 'V')
      {
        Serial.println(F("\n\nReading Voltage:"));
        inSubMenu = 1;
        while (inSubMenu)
        {
          if (millis() - voltageLastTime > VOLTAGE_CHECK_INTERVAL)
          {
            voltageRead();
            voltageLastTime = millis();
            Serial.print("\r  ");
            Serial.print(voltage);
            Serial.print("v    ");
          }
          if (Serial.available())
          {
            Serial.read(); //clear character
            inSubMenu = 0;
          }
        }
      }
      else if (tmp == 'p' || tmp == 'P')
      {
        Serial.println(F("\n\nControlling Light Level:"));
        Serial.println(F("Use k to increment and m to decrement, q to quit."));
        int level = 255;
        analogWrite(LIGHT_OUT_PIN, level);
        inSubMenu = 1;
        while (inSubMenu)
        {
          if (Serial.available())
          {
            char sTemp = Serial.read(); //clear character
            if(sTemp == 'q'){
              Serial.println(F("\nQuit to main Debug Menu"));
              inSubMenu = 0;
            }
            else if(sTemp == 'k'){
              level --;
              if(level < 0){
                level = 0;
              }
              analogWrite(LIGHT_OUT_PIN, level);
              Serial.print("\r ");
              Serial.print(level);
              Serial.print("    ");
            }
            else if (sTemp == 'm')
            {
              level++;
              if (level > 256)
              {
                level = 256;
              }
              analogWrite(LIGHT_OUT_PIN, level);
              Serial.print("\r ");
              Serial.print(level);
              Serial.print("    ");
            }
          }
        }
      }
    }
  }

  Serial.println(F("Exiting Debug"));
  stateTo(SWITCHED_OFF);
}