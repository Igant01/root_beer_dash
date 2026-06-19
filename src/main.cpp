#include <Arduino.h>
#include <FlexCAN_T4.h>
#include <gauge_stepper.h>
#include <volt_meter.h>
#include <lights.h>
#include <screens.h>
#include <LMT87.h>
#include "fault.h"


#define LDIR 2
#define LSTEP 3 
#define RDIR 4
#define RSTEP 5

FlexCAN_T4<CAN1, RX_SIZE_1024, TX_SIZE_1024> can1;
CAN_message_t msg;

const uint32_t KEYPAD_CAN_ID = 0x120;
const uint32_t INPUTMOD_MES1_CAN_ID = 0x220;
const uint32_t STATUS_CAN_ID = 0x521;
const unsigned long DISPLAY_REFRESH_INTERVAL_MS = 250;
const unsigned long STATUS_BROADCAST_INTERVAL_MS = 1000;
const float BATTERY_FAULT_THRESHOLD = 11.0f;      // volts
const int TEMPERATURE_FAULT_THRESHOLD_F = 185;    // Fahrenheit

FaultContainer moduleFaults;
uint8_t heartbeatCounter = 0;
unsigned long lastStatusBroadcast = 0;

X27168 leftGauge(LSTEP,LDIR);
X27168 rightGauge(RSTEP,RDIR);
volt_meter batt;
LMT87 ambientTempSensor;
cluster_lights dashLights;
mainScreen temp;

static inline uint16_t combineBytes(uint8_t low, uint8_t high){
  return (uint16_t)low | ((uint16_t)high << 8);
}

static void evaluateFaults(){
  float batteryVolts = batt.read();
  if(batteryVolts < BATTERY_FAULT_THRESHOLD){
    moduleFaults.set(FAULT_BATTERY_LOW);
  } else {
    moduleFaults.clear(FAULT_BATTERY_LOW);
  }

  int ambientF = ambientTempSensor.read();
  if(ambientF > TEMPERATURE_FAULT_THRESHOLD_F){
    moduleFaults.set(FAULT_TEMPERATURE_HIGH);
  } else {
    moduleFaults.clear(FAULT_TEMPERATURE_HIGH);
  }
}

static void sendStatusMessage(){
  CAN_message_t status;
  status.id = STATUS_CAN_ID;
  status.flags.extended = 0;
  status.flags.remote = 0;
  status.len = 8;

  status.buf[0] = heartbeatCounter++;
  status.buf[1] = moduleFaults.firstActiveFault();
  status.buf[2] = static_cast<uint8_t>(temp.getTopPageCount());
  uint16_t mask = moduleFaults.activeMask16();
  status.buf[3] = static_cast<uint8_t>(mask & 0xFF);
  status.buf[4] = static_cast<uint8_t>((mask >> 8) & 0xFF);
  status.buf[5] = 0;
  status.buf[6] = 0;
  status.buf[7] = 0;

  can1.write(status);
}

void processCanMessage(const CAN_message_t &msg){
  char buf[16];
  bool pageChanged = false;
  bool modeChanged = false;
  bool headerChanged = false;

  Serial.print("CAN RX ID=0x");
  Serial.println(msg.id, HEX);

  if(msg.id == 0x320){
    snprintf(buf, sizeof(buf), "%u", msg.buf[5]);
    temp.updateTopField(0, 0, "Battery V", buf);
    Serial.print(" Battery V="); Serial.println(buf);

    snprintf(buf, sizeof(buf), "%u", msg.buf[4]);
    temp.updateTopField(0, 2, "Oil Temp", buf);
    Serial.print(" Oil Temp="); Serial.println(buf);

    snprintf(buf, sizeof(buf), "%u", combineBytes(msg.buf[1], msg.buf[2]));
    temp.updateTopField(0, 3, "MAP", buf);
    Serial.print(" MAP="); Serial.println(buf);

    snprintf(buf, sizeof(buf), "%u", msg.buf[6]);
    temp.updateTopField(0, 4, "O2", buf);
    Serial.print(" O2="); Serial.println(buf);
  }
  else if(msg.id == 0x325){
    snprintf(buf, sizeof(buf), "%u", msg.buf[3]);
    temp.updateTopField(1, 3, "Ethanol %", buf);
    Serial.print(" Ethanol %="); Serial.println(buf);

    snprintf(buf, sizeof(buf), "%u", msg.buf[4]);
    temp.updateTopField(1, 4, "Baro", buf);
    Serial.print(" Baro="); Serial.println(buf);

    snprintf(buf, sizeof(buf), "%u", msg.buf[1]);
    temp.updateTopField(2, 2, "TPS", buf);
    Serial.print(" TPS="); Serial.println(buf);

    snprintf(buf, sizeof(buf), "%u", msg.buf[7]);
    temp.updateTopField(2, 3, "Fuel P", buf);
    Serial.print(" Fuel P="); Serial.println(buf);
  }
  else if(msg.id == 0x330){
    snprintf(buf, sizeof(buf), "%u", msg.buf[0]);
    temp.updateTopField(0, 1, "Oil Pressure", buf);
    Serial.print(" Oil Pressure="); Serial.println(buf);

    uint16_t cht = ((uint16_t)msg.buf[1] + (uint16_t)msg.buf[2]) / 2;
    snprintf(buf, sizeof(buf), "%u", cht);
    temp.updateTopField(1, 1, "CHT Avg", buf);
    Serial.print(" CHT Avg="); Serial.println(buf);

    snprintf(buf, sizeof(buf), "%u", msg.buf[3]);
    temp.updateTopField(2, 0, "EGT1", buf);
    Serial.print(" EGT1="); Serial.println(buf);

    snprintf(buf, sizeof(buf), "%u", msg.buf[4]);
    temp.updateTopField(2, 1, "EGT2", buf);
    Serial.print(" EGT2="); Serial.println(buf);
  }
  else if(msg.id == KEYPAD_CAN_ID){
    uint8_t desiredPage = msg.buf[2] > 0 ? msg.buf[2] - 1 : 0;
    if(desiredPage < temp.getTopPageCount() && desiredPage != currentKeypadPage){
      currentKeypadPage = desiredPage;
      temp.setTopFieldPage(desiredPage);
      pageChanged = true;
    }

    uint8_t rawMode = msg.buf[0] < 1 ? 1 : msg.buf[0];
    if(rawMode > 4){ rawMode = 4; }
    if(rawMode != currentRearSteerMode){
      currentRearSteerMode = rawMode;
      switch(rawMode){
        case 1: temp.rearSteerOff(); break;
        case 2: temp.rearSteerNormal(); break;
        case 3: temp.rearSteerCrab(); break;
        default: temp.rearSteerManual(); break;
      }
      modeChanged = true;
    }

    uint8_t newAngle = msg.buf[1] < 1 ? 1 : msg.buf[1];
    if(newAngle != currentRearSteerAngle){
      currentRearSteerAngle = newAngle;
      snprintf(buf, sizeof(buf), "%u", currentRearSteerAngle);
      temp.updateTopField(3, 3, "Angle", buf);
      headerChanged = true;
    }

    bool newLaunch = msg.buf[3] != 0;
    if(newLaunch != currentLaunchControl){
      currentLaunchControl = newLaunch;
      temp.updateTopField(3, 2, "Launch", currentLaunchControl ? "ON" : "OFF");
      headerChanged = true;
    }
  }
  else if(msg.id == INPUTMOD_MES1_CAN_ID){
    bool newMode = false;
    snprintf(buf, sizeof(buf), "%s", (msg.buf[0] & 0x01) ? "ON" : "OFF");
    temp.updateTopField(5, 0, "AWD", buf);
    snprintf(buf, sizeof(buf), "%s", (msg.buf[0] & 0x02) ? "ON" : "OFF");
    temp.updateTopField(5, 1, "Diff", buf);
    snprintf(buf, sizeof(buf), "%s", (msg.buf[0] & 0x04) ? "ON" : "OFF");
    temp.updateTopField(5, 2, "Pod", buf);
    snprintf(buf, sizeof(buf), "%s", (msg.buf[0] & 0x08) ? "ON" : "OFF");
    temp.updateTopField(5, 3, "LightBar", buf);
    snprintf(buf, sizeof(buf), "%s", (msg.buf[0] & 0x10) ? "ON" : "OFF");
    temp.updateTopField(5, 4, "Brake", buf);

    uint8_t gearValue = msg.buf[1];
    switch(gearValue){
      case 1: temp.shift(PARK); break;
      case 2: temp.shift(REVERSE); break;
      case 3: temp.shift(NEUTRAL); break;
      case 4: temp.shift(LOWGEAR); break;
      case 5: temp.shift(DRIVE); break;
      default: temp.shift(PARK); break;
    }
    fullDisplayUpdatePending = true;
  }

  snprintf(buf, sizeof(buf), "0x%03lX", (unsigned long)msg.id);
  temp.updateTopField(4, 2, "ECU", buf);
  temp.updateTopField(4, 0, "Keypad", "RX");
  temp.updateTopField(4, 1, "Input", "CAN");
  temp.updateTopField(4, 3, "RS", "GOOD");

  if(modeChanged){
    fullDisplayUpdatePending = true;
  }
  else if(pageChanged || headerChanged){
    displayUpdatePending = true;
  }
}

void setup() {
  Serial.begin(115200);                   // Start serial for debugging and CAN message logging
  can1.begin();                           // Initialize CAN bus hardware
  can1.setBaudRate(250000);               // Set CAN speed to 250 kbps to match the ECU broadcaster
  can1.setMB(MB0, RX, STD);                 // Enable mailbox 0 for standard CAN receive.
  can1.setMBFilter(MB0, 0x320, 0x325, 0x330, KEYPAD_CAN_ID, INPUTMOD_MES1_CAN_ID); // Filter ECU, keypad, and input module IDs.
  delay(100);                             // Brief delay for hardware stabilization

  dashLights.gaugelight_off();            // Ensure dashboard lights are off at startup
  temp.init();                            // Initialize display and clear the screen

  // Add 5 top-field pages, one per column from the attached grid.
  const char labels0[][16] = {
    "Battery V",   // ECU 1 page field 0
    "Oil Pressure",// ECU 1 page field 1
    "Oil Temp",    // ECU 1 page field 2
    "MAP",         // ECU 1 page field 3
    "O2"           // ECU 1 page field 4
  };
  temp.addTopFieldPage("ECU 1", labels0, 5);

  const char labels1[][16] = {
    "IAT",         // ECU 2 page field 0
    "CHT Avg",     // ECU 2 page field 1
    "Target Boost",// ECU 2 page field 2
    "Ethanol %",   // ECU 2 page field 3
    "Baro"         // ECU 2 page field 4
  };
  temp.addTopFieldPage("ECU 2", labels1, 5);

  const char labels2[][16] = {
    "EGT1",        // ECU 3 page field 0
    "EGT2",        // ECU 3 page field 1
    "TPS",         // ECU 3 page field 2
    "Fuel P",      // ECU 3 page field 3
    ""             // ECU 3 page field 4 (unused)
  };
  temp.addTopFieldPage("ECU 3", labels2, 5);

  const char labels3[][16] = {
    "Avg Power",   // Rear Steer page field 0
    "Peak Power",  // Rear Steer page field 1
    "Launch",      // Rear Steer page field 2
    "Angle",       // Rear Steer page field 3
    ""             // Rear Steer page field 4 (unused)
  };
  temp.addTopFieldPage("Rear Steer", labels3, 5);

  const char labels4[][16] = {
    "Keypad",      // CAN RX/TX page field 0
    "Input",       // CAN RX/TX page field 1
    "ECU",         // CAN RX/TX page field 2
    "RS",          // CAN RX/TX page field 3
    ""             // CAN RX/TX page field 4 (unused)
  };
  temp.addTopFieldPage("CAN RX/TX", labels4, 5);

  const char labels5[][16] = {
    "AWD",         // Input page field 0
    "Diff",        // Input page field 1
    "Pod",         // Input page field 2
    "LightBar",    // Input page field 3
    "Brake"        // Input page field 4
  };
  temp.addTopFieldPage("Inputs", labels5, 5);

  temp.splashScreen(1000);                // Show startup splash
  temp.splashScreen2(5000);              // Show secondary splash
  temp.setTopFieldPage(0);               // Fix display to live CAN page 0.

  // Draw initial placeholders and the full display layout.
  temp.updateTopField(0, 0, "Battery V", "--");
  temp.updateTopField(0, 1, "Oil Pressure", "--");
  temp.updateTopField(0, 2, "Oil Temp", "--");
  temp.updateTopField(0, 3, "MAP", "--");
  temp.updateTopField(0, 4, "O2", "--");
  temp.updateAll();
  delay(1000);                           // Pause so user can see initial page
}
  

void loop() {
  static unsigned long lastRefresh = 0;

  // Read all available CAN messages and mark updates.
  if(can1.read(msg)){
    Serial.println("CAN RX message");
    processCanMessage(msg);
  }

  unsigned long now = millis();
  if(now - lastStatusBroadcast >= STATUS_BROADCAST_INTERVAL_MS){
    lastStatusBroadcast = now;
    evaluateFaults();
    sendStatusMessage();
  }

  if(now - lastRefresh >= DISPLAY_REFRESH_INTERVAL_MS){
    lastRefresh = now;
    if(fullDisplayUpdatePending){
      fullDisplayUpdatePending = false;
      displayUpdatePending = false;
      temp.updateAll();
    }
    else if(displayUpdatePending){
      displayUpdatePending = false;
      temp.updateTopHeader();
    }
  }
}

