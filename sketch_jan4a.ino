#include <SoftwareSerial.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <BH1750.h>
#include "Adafruit_PM25AQI.h"

// ==========================================
// PIN DEFINITIONS
// ==========================================
#define PM25_RX_PIN 2
#define PM25_TX_PIN 3
#define TEMP_PIN 4
#define FAN_PWM_PIN 5
#define BTN_ON 6         
#define BTN_OFF 7        
#define BTN_AUTO 8
#define GREEN_LED_PIN 10
#define GAS_LED_PIN 9   
#define BUZZER_PIN 11
#define GAS_SENSOR_PIN 12 // Changed from A0 to Digital Pin 12

// ==========================================
// CALIBRATION THRESHOLDS
// ==========================================
// Note: GAS_DANGER_LEVEL is removed since the physical potentiometer handles it now
const float TEMP_CRITICAL = 45.0;      
const int PM25_START_SMOKE = 500;      
const int PM25_MAX_SMOKE = 700;        
const int DARKNESS_THRESHOLD = 15;     

// The Stopwatches (in milliseconds)
const unsigned long SMOKE_DELAY_MS = 8000;       
const unsigned long SMOKE_CLEAR_DELAY_MS = 15000; 

// ==========================================
// SENSOR SETUP & VARIABLES
// ==========================================
SoftwareSerial pmSerial(PM25_RX_PIN, PM25_TX_PIN);
Adafruit_PM25AQI aqi = Adafruit_PM25AQI();
OneWire oneWire(TEMP_PIN);
DallasTemperature tempSensor(&oneWire);
BH1750 lightMeter;

enum SystemMode { MODE_AUTO, MODE_MANUAL_ON, MODE_MANUAL_OFF };
SystemMode currentMode = MODE_AUTO;

// Variables for Timers and Speed Memory
unsigned long smokeDetectTime = 0;
unsigned long smokeClearTime = 0;
bool isCookingConfirmed = false;
int lastFanSpeed = 0; 

void setup() {
  pinMode(FAN_PWM_PIN, OUTPUT);
  pinMode(GAS_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  pinMode(BTN_ON, INPUT_PULLUP);
  pinMode(BTN_OFF, INPUT_PULLUP);
  pinMode(BTN_AUTO, INPUT_PULLUP);
  
  pinMode(GAS_SENSOR_PIN, INPUT); // Tell the Arduino to listen to the D0 pin

  Serial.begin(9600);
  pmSerial.begin(9600); 
  tempSensor.begin();
  Wire.begin();

  if (lightMeter.begin()) {
    Serial.println(F("BH1750 initialized"));
  }

  if (!aqi.begin_UART(&pmSerial)) {
    Serial.println(F("Warning: PM2.5 not found!"));
  }

  analogWrite(FAN_PWM_PIN, 0);
  digitalWrite(GAS_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  
  Serial.println(F("Smart Hood Booted."));
}

void loop() {
  // Read the digital pin (HIGH = Clean Air, LOW = Gas Detected)
  int gasState = digitalRead(GAS_SENSOR_PIN); 
  uint16_t lux = lightMeter.readLightLevel();

  // --------------------------------------------------------
  // STEP 1: GAS LEAK OVERRIDE
  // --------------------------------------------------------
  if (gasState == LOW) { // LOW means the sensor's green light is ON
    digitalWrite(GAS_LED_PIN, HIGH);   
    digitalWrite(GREEN_LED_PIN, LOW);  
    digitalWrite(BUZZER_PIN, HIGH);    
    analogWrite(FAN_PWM_PIN, 255);     
    
    Serial.println(F("EMERGENCY: Gas Leak!"));
    // The "return;" statement is removed so the rest of the code still updates!
  } 
  else {
    digitalWrite(GAS_LED_PIN, LOW);    
    digitalWrite(BUZZER_PIN, LOW);     
    
    if (lux < DARKNESS_THRESHOLD) {
      digitalWrite(GREEN_LED_PIN, HIGH); 
    } else {
      digitalWrite(GREEN_LED_PIN, LOW);  
    }
  }

  // --------------------------------------------------------
  // STEP 2: CHECK MANUAL BUTTONS
  // --------------------------------------------------------
  if (digitalRead(BTN_ON) == LOW) {
    currentMode = MODE_MANUAL_ON;
    Serial.println(F("Mode: MANUAL ON"));
    delay(200);
  }
  else if (digitalRead(BTN_OFF) == LOW) {
    currentMode = MODE_MANUAL_OFF;
    Serial.println(F("Mode: MANUAL OFF"));
    delay(200);
  }
  else if (digitalRead(BTN_AUTO) == LOW) {
    currentMode = MODE_AUTO;
    Serial.println(F("Mode: AUTO DETECT"));
    delay(200);
  }

  // --------------------------------------------------------
  // STEP 3: READ SENSORS & DUAL-TIMER LOGIC
  // --------------------------------------------------------
  tempSensor.requestTemperatures();
  float currentTemp = tempSensor.getTempCByIndex(0);
  
  PM25_AQI_Data data;
  int currentSmoke = 0;
  if (aqi.read(&data)) {
    currentSmoke = data.pm25_env; 
  }

  if (currentSmoke >= PM25_START_SMOKE) {
    smokeClearTime = 0; 
    
    if (smokeDetectTime == 0) {
      smokeDetectTime = millis(); 
    }
    if (millis() - smokeDetectTime >= SMOKE_DELAY_MS) {
      isCookingConfirmed = true; 
    }
  } 
  else {
    smokeDetectTime = 0; 
    
    if (isCookingConfirmed) {
      if (smokeClearTime == 0) {
        smokeClearTime = millis();
      }
      if (millis() - smokeClearTime >= SMOKE_CLEAR_DELAY_MS) {
        isCookingConfirmed = false; 
        smokeClearTime = 0;
      }
    }
  }

  // Updated printout to show text instead of raw analog numbers
  Serial.print(F("Temp: ")); Serial.print(currentTemp);
  Serial.print(F("C | Smoke: ")); Serial.print(currentSmoke);
  Serial.print(F(" | Lux: ")); Serial.print(lux);
  Serial.print(F(" | Gas Status: ")); 
  Serial.print(gasState == LOW ? "LEAK DETECTED" : "CLEAR");
  Serial.print(F(" | Mode: ")); Serial.println(currentMode);

// --------------------------------------------------------
  // STEP 4: EXECUTE MOTOR CONTROL
  // --------------------------------------------------------
  
  if (gasState == LOW) {
    // GAS LEAK OVERRIDE: Fan was set to 255 in Step 1. 
    // We do nothing else here so the normal rules don't turn it off!
  }
  else if (currentTemp >= TEMP_CRITICAL) {
    analogWrite(FAN_PWM_PIN, 255); 
    Serial.println(F("Status: THERMAL PURGE (MAX)"));
  } 
  else {
    switch (currentMode) {
      case MODE_MANUAL_OFF:
        analogWrite(FAN_PWM_PIN, 0); 
        lastFanSpeed = 0;
        break;
        
      case MODE_MANUAL_ON:
        analogWrite(FAN_PWM_PIN, 255); 
        lastFanSpeed = 255;
        break;
        
      case MODE_AUTO:
        if (isCookingConfirmed) {
          
          // THE 0-GLITCH BYPASS
          if (currentSmoke == 0) {
            analogWrite(FAN_PWM_PIN, lastFanSpeed); 
          } 
          else {
            int autoSpeed = map(currentSmoke, PM25_START_SMOKE, PM25_MAX_SMOKE, 106, 255);
            autoSpeed = constrain(autoSpeed, 106, 255); 
            analogWrite(FAN_PWM_PIN, autoSpeed);
            lastFanSpeed = autoSpeed; // Save the speed in case it glitches to 0 next second
          }
          
        } else {
          analogWrite(FAN_PWM_PIN, 0); 
          lastFanSpeed = 0;
        }
        break;
    }
  }

  delay(500); 
}