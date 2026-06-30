#include <Arduino.h>
#include <EEPROM.h>
#include <FlexCAN_T4.h>
#include "gauge_stepper.h"
#include "serial_console_commands.h"
#include "volt_meter.h"
#include "lights.h"
#include "screens.h"
#include "LMT87.h"
#include "fault.h"


#define LDIR 2
#define LSTEP 3 
#define RDIR 4
#define RSTEP 5

FlexCAN_T4<CAN1, RX_SIZE_1024, TX_SIZE_1024> can1;
CAN_message_t msg;

const uint32_t KEYPAD_CAN_ID = 0x120;
const uint32_t INPUTMOD_MES1_CAN_ID = 0x220;
const uint32_t KEYPAD_STATUS_CAN_ID = 0x121;
const uint32_t INPUTMOD_STATUS_CAN_ID = 0x221;
const uint32_t ECU_STATUS_CAN_ID = 0x321;
const uint32_t REARSTEER_STATUS_CAN_ID = 0x421;
const uint32_t STATUS_CAN_ID = 0x521;
const unsigned long DISPLAY_REFRESH_INTERVAL_MS = 250;
const unsigned long STATUS_BROADCAST_INTERVAL_MS = 1000;
const unsigned long HEARTBEAT_TIMEOUT_MS = 1500;
const unsigned int SPEEDOMETER_MAX_MPH = 90;
const unsigned int TACHOMETER_MAX_RPM = 9000;
const unsigned int GAUGE_MOTION_STEP_DELAY_US = 1200;
const int GAUGE_TRAVEL_STEPS = 1200;
const int MAX_MANUAL_MOVE_STEPS = 500;
const int DEFAULT_ZERO_OFFSET_STEPS = 50;
const int DEFAULT_MAX_OFFSET_STEPS = 50;
const uint32_t DEFAULT_SPEED_CAN_ID = 0;
const uint32_t DEFAULT_RPM_CAN_ID = 0;
const uint8_t DEFAULT_SPEED_CAN_BYTE = 0;
const uint8_t DEFAULT_RPM_CAN_BYTE = 0;
const uint8_t DEFAULT_SPEED_CAN_BYTES = 1;
const uint8_t DEFAULT_RPM_CAN_BYTES = 2;

uint8_t heartbeatCounter = 0;
unsigned long lastStatusBroadcast = 0;

X27168 leftGauge(LSTEP,LDIR);   // Speedometer
X27168 rightGauge(RSTEP,RDIR);  // Tachometer
volt_meter batt;
LMT87 ambientTempSensor;
cluster_lights dashLights;
mainScreen temp;

uint8_t currentKeypadPage = 0;
uint8_t currentRearSteerMode = 1;
uint8_t currentRearSteerAngle = 1;
bool currentLaunchControl = false;
bool currentAWD = false;
bool currentDiff = false;
bool currentPod = false;
bool currentLightBar = false;
bool currentBrake = false;
uint8_t currentGearValue = 0;
bool ringGaugeLightEnabled = false;
char ecu1BatteryV[16] = "--";
char ecu1OilPressure[16] = "--";
char ecu1OilTemp[16] = "--";
char ecu1Map[16] = "--";
char ecu1O2[16] = "--";
char ecu2Iat[16] = "--";
char ecu2ChtAvg[16] = "--";
char ecu2TargetBoost[16] = "--";
char ecu2Ethanol[16] = "--";
char ecu2Baro[16] = "--";
char ecu3Egt1[16] = "--";
char ecu3Egt2[16] = "--";
char ecu3Tps[16] = "--";
char ecu3FuelP[16] = "--";
bool fullDisplayUpdatePending = false;
bool displayUpdatePending = false;
unsigned long lastKeypadHeartbeatMs = 0;
unsigned long lastInputHeartbeatMs = 0;
unsigned long lastEcuHeartbeatMs = 0;
unsigned long lastRearSteerHeartbeatMs = 0;
bool serialTelemetryEnabled = false;
bool serialConsoleBannerShown = false;
bool gaugeSetupModeEnabled = false;

// EEPROM layout for gauge calibration
struct GaugeCalibrationEEPROM {
  uint16_t magic;          // 0xCAFE when valid
  int leftZero;
  int leftMax;
  int rightZero;
  int rightMax;
};
constexpr uint16_t CALIB_MAGIC = 0xCAFE;
constexpr int CALIB_EEPROM_ADDR = 0;

static void saveCalibrationToEEPROM(){
  GaugeCalibrationEEPROM data;
  data.magic      = CALIB_MAGIC;
  data.leftZero   = leftGauge.getRelMin();
  data.leftMax    = leftGauge.getRelMax();
  data.rightZero  = rightGauge.getRelMin();
  data.rightMax   = rightGauge.getRelMax();
  EEPROM.put(CALIB_EEPROM_ADDR, data);
  Serial.println("Calibration saved to EEPROM.");
}

static void applyDefaultGaugeCalibration(){
  int leftMax = GAUGE_TRAVEL_STEPS - DEFAULT_MAX_OFFSET_STEPS;
  int rightMax = GAUGE_TRAVEL_STEPS - DEFAULT_MAX_OFFSET_STEPS;

  leftGauge.setRelMin(DEFAULT_ZERO_OFFSET_STEPS);
  leftGauge.setRelMax(leftMax);
  rightGauge.setRelMin(DEFAULT_ZERO_OFFSET_STEPS);
  rightGauge.setRelMax(rightMax);
}

// Returns true if valid data was loaded (sweep can be skipped).
static bool loadCalibrationFromEEPROM(){
  GaugeCalibrationEEPROM data;
  EEPROM.get(CALIB_EEPROM_ADDR, data);
  if(data.magic != CALIB_MAGIC){
    return false;
  }
  leftGauge.setRelMin(data.leftZero);
  leftGauge.setRelMax(data.leftMax);
  rightGauge.setRelMin(data.rightZero);
  rightGauge.setRelMax(data.rightMax);

  return true;
}

struct GaugeCanSource {
  bool enabled;
  uint32_t canId;
  uint8_t startByte;
  uint8_t byteCount;
  float scale;
  float offset;
};

GaugeCanSource speedSource = {false, DEFAULT_SPEED_CAN_ID, DEFAULT_SPEED_CAN_BYTE, DEFAULT_SPEED_CAN_BYTES, 1.0f, 0.0f};
GaugeCanSource rpmSource = {false, DEFAULT_RPM_CAN_ID, DEFAULT_RPM_CAN_BYTE, DEFAULT_RPM_CAN_BYTES, 1.0f, 0.0f};

char serialLine[96];
size_t serialLineLength = 0;

static void homeGaugesTogether();

static inline uint16_t combineBytes(uint8_t low, uint8_t high){
  return (uint16_t)low | ((uint16_t)high << 8);
}

static uint32_t readCanRawValue(const CAN_message_t &frame, const GaugeCanSource &source){
  if(!source.enabled || frame.id != source.canId){
    return 0;
  }
  if(source.startByte >= frame.len){
    return 0;
  }

  if(source.byteCount <= 1){
    return frame.buf[source.startByte];
  }

  if((uint16_t)source.startByte + 1 >= frame.len){
    return 0;
  }

  return (uint32_t)frame.buf[source.startByte] | ((uint32_t)frame.buf[source.startByte + 1] << 8);
}

static unsigned int applyGaugeScale(uint32_t rawValue, const GaugeCanSource &source, unsigned int maxValue){
  float scaledValue = (static_cast<float>(rawValue) * source.scale) + source.offset;
  if(scaledValue < 0.0f){
    scaledValue = 0.0f;
  }
  if(scaledValue > static_cast<float>(maxValue)){
    scaledValue = static_cast<float>(maxValue);
  }
  return static_cast<unsigned int>(scaledValue + 0.5f);
}

static int mapGaugeValueToStep(unsigned int value, unsigned int maxValue, X27168 &gauge){
  if(maxValue == 0){
    maxValue = 1;
  }
  if(value > maxValue){
    value = maxValue;
  }

  int relMin = gauge.getRelMin();
  int relMax = gauge.getRelMax();
  long span = static_cast<long>(relMax) - static_cast<long>(relMin);
  long mapped = static_cast<long>(relMin) +
                (span * static_cast<long>(value)) / static_cast<long>(maxValue);
  return static_cast<int>(mapped);
}

static void printGaugeCalibration(const char *label, X27168 &gauge){
  Serial.print(label);
  Serial.print(" zero=");
  Serial.print(gauge.getRelMin());
  Serial.print(" max=");
  Serial.print(gauge.getRelMax());
  Serial.print(" travelLimit=");
  Serial.println(GAUGE_TRAVEL_STEPS);
}

static void showSerialHelp(){
  Serial.println("--- Commands ---");
  Serial.println("  home                 run gauge homing sweep (up 1200, down 1200)");
  Serial.println("  setup on             enter manual gauge setup mode");
  Serial.println("  setup off            return to normal CAN-driven mode");
  Serial.println("  setup                print current setup mode state");
  Serial.println("  write eeprom         store current calibration values to EEPROM");
  Serial.println("  set min <left|right> capture current needle as min (zero)");
  Serial.println("  set max <left|right> capture current needle as max");
  Serial.println("  ml<steps>            move left gauge, max 500 steps per command");
  Serial.println("  mr<steps>            move right gauge, max 500 steps per command");
  Serial.println("  lzero / rzero        move left or right gauge to zero value");
  Serial.println("  lmax  / rmax         move left or right gauge to max value");
  Serial.println("  zero <left|right>    same as lzero/rzero");
  Serial.println("  max  <left|right>    same as lmax/rmax");
  Serial.println("  show data on|off     enable or disable serial CAN data output");
  Serial.println("  Example: ml-50  (negative = down, positive = up)");
}

static void announceSerialConsoleIfReady(){
  if(serialConsoleBannerShown || !Serial){
    return;
  }
  showSerialHelp();
  serialConsoleBannerShown = true;
}

static void processSerialCommand(char *line){
  if(line == nullptr || line[0] == '\0') return;

  char lineCopy[96];
  strncpy(lineCopy, line, sizeof(lineCopy) - 1);
  lineCopy[sizeof(lineCopy) - 1] = '\0';

  // ml<steps> — move left gauge
  if(line[0] == 'm' && line[1] == 'l'){
    int steps = atoi(line + 2);
    if(steps > MAX_MANUAL_MOVE_STEPS) steps = MAX_MANUAL_MOVE_STEPS;
    if(steps < -MAX_MANUAL_MOVE_STEPS) steps = -MAX_MANUAL_MOVE_STEPS;
    int newPos = leftGauge.getPosition() - steps;
    if(newPos < 0) newPos = 0;
    if(newPos > GAUGE_TRAVEL_STEPS) newPos = GAUGE_TRAVEL_STEPS;
    leftGauge.setPosition(newPos);
    Serial.print("left -> "); Serial.println(newPos);
    return;
  }

  // mr<steps> — move right gauge
  if(line[0] == 'm' && line[1] == 'r'){
    int steps = atoi(line + 2);
    if(steps > MAX_MANUAL_MOVE_STEPS) steps = MAX_MANUAL_MOVE_STEPS;
    if(steps < -MAX_MANUAL_MOVE_STEPS) steps = -MAX_MANUAL_MOVE_STEPS;
    int newPos = rightGauge.getPosition() - steps;
    if(newPos < 0) newPos = 0;
    if(newPos > GAUGE_TRAVEL_STEPS) newPos = GAUGE_TRAVEL_STEPS;
    rightGauge.setPosition(newPos);
    Serial.print("right -> "); Serial.println(newPos);
    return;
  }

  char *command = strtok(line, " \t");
  if(command == nullptr) return;

  if(strcmp(command, SerialConsoleCommands::SETUP) == 0){
    char *state = strtok(nullptr, " \t");
    if(!state){
      Serial.println(gaugeSetupModeEnabled ? "setup mode is on" : "setup mode is off");
      return;
    }
    if(strcmp(state, SerialConsoleCommands::ON) == 0){
      gaugeSetupModeEnabled = true;
      Serial.println("setup mode enabled");
      return;
    }
    if(strcmp(state, SerialConsoleCommands::OFF) == 0){
      gaugeSetupModeEnabled = false;
      Serial.println("setup mode disabled; gauges returned to CAN control");
      return;
    }
    Serial.println("Usage: setup on|off");
    return;
  }

  if(strcmp(command, SerialConsoleCommands::HOME) == 0){
    homeGaugesTogether();
    Serial.println("homing complete");
    return;
  }

  if(strcmp(command, SerialConsoleCommands::WRITE) == 0){
    char *target = strtok(nullptr, " \t");
    if(target && strcmp(target, SerialConsoleCommands::EEPROM) == 0){
      saveCalibrationToEEPROM();
      return;
    }
    Serial.println("Usage: write eeprom");
    return;
  }

  if(strcmp(command, SerialConsoleCommands::SET) == 0){
    char *which = strtok(nullptr, " \t");
    char *side = strtok(nullptr, " \t");
    if(!which || !side){
      Serial.println("Usage: set min|max <left|right>");
      return;
    }

    bool isLeft = strcmp(side, SerialConsoleCommands::LEFT) == 0;
    bool isRight = strcmp(side, SerialConsoleCommands::RIGHT) == 0;
    if(!isLeft && !isRight){
      Serial.println("Usage: set min|max <left|right>");
      return;
    }

    X27168 &gauge = isLeft ? leftGauge : rightGauge;
    const char *label = isLeft ? "left" : "right";
    if(strcmp(which, SerialConsoleCommands::MIN) == 0){
      gauge.setRelMin(gauge.getPosition());
      Serial.print(label);
      Serial.print(" min captured at step ");
      Serial.println(gauge.getRelMin());
      return;
    }
    if(strcmp(which, SerialConsoleCommands::MAX) == 0){
      gauge.setRelMax(gauge.getPosition());
      Serial.print(label);
      Serial.print(" max captured at step ");
      Serial.println(gauge.getRelMax());
      return;
    }

    Serial.println("Usage: set min|max <left|right>");
    return;
  }

  

  if(strcmp(command, SerialConsoleCommands::LZERO) == 0){
    leftGauge.setPosition(leftGauge.getRelMin());
    Serial.println("left gauge -> zero");
    return;
  }

  if(strcmp(command, SerialConsoleCommands::RZERO) == 0){
    rightGauge.setPosition(rightGauge.getRelMin());
    Serial.println("right gauge -> zero");
    return;
  }

  if(strcmp(command, SerialConsoleCommands::LMAX) == 0){
    leftGauge.setPosition(leftGauge.getRelMax());
    Serial.println("left gauge -> max (90mph)");
    return;
  }

  if(strcmp(command, SerialConsoleCommands::RMAX) == 0){
    rightGauge.setPosition(rightGauge.getRelMax());
    Serial.println("right gauge -> max (9000rpm)");
    return;
  }

  // zero <left|right>
  if(strcmp(command, SerialConsoleCommands::ZERO) == 0){
    char *sideT = strtok(nullptr, " \t");
    if(!sideT){ Serial.println("Usage: zero <left|right>"); return; }
    if(strcmp(sideT, SerialConsoleCommands::LEFT) == 0){
      leftGauge.setPosition(leftGauge.getRelMin());
      Serial.println("left gauge -> zero");
    } else if(strcmp(sideT, SerialConsoleCommands::RIGHT) == 0){
      rightGauge.setPosition(rightGauge.getRelMin());
      Serial.println("right gauge -> zero");
    } else { Serial.println("Usage: zero <left|right>"); }
    return;
  }

  // max <left|right>
  if(strcmp(command, SerialConsoleCommands::MAX) == 0){
    char *sideT = strtok(nullptr, " \t");
    if(!sideT){ Serial.println("Usage: max <left|right>"); return; }
    if(strcmp(sideT, SerialConsoleCommands::LEFT) == 0){
      leftGauge.setPosition(leftGauge.getRelMax());
      Serial.println("left gauge -> max (90mph)");
    } else if(strcmp(sideT, SerialConsoleCommands::RIGHT) == 0){
      rightGauge.setPosition(rightGauge.getRelMax());
      Serial.println("right gauge -> max (9000rpm)");
    } else { Serial.println("Usage: max <left|right>"); }
    return;
  }

  // show data on|off
  if(strcmp(command, SerialConsoleCommands::SHOW) == 0){
    char *t1 = strtok(nullptr, " \t");
    char *t2 = strtok(nullptr, " \t");
    if(!t1 || strcmp(t1, SerialConsoleCommands::DATA) != 0 || !t2){
      Serial.println("Usage: show data on|off"); return;
    }
    if(strcmp(t2, SerialConsoleCommands::ON) == 0){
      serialTelemetryEnabled = true;
      Serial.println("data stream on");
    } else if(strcmp(t2, SerialConsoleCommands::OFF) == 0){
      serialTelemetryEnabled = false;
      Serial.println("data stream off");
    } else {
      Serial.println("Usage: show data on|off");
    }
    return;
  }

  showSerialHelp();
}

static void serviceSerialConsole(){
  while(Serial.available() > 0){
    char incoming = static_cast<char>(Serial.read());
    if(incoming == '\r'){
      continue;
    }
    if(incoming == '\n'){
      serialLine[serialLineLength] = '\0';
      if(serialLineLength > 0){
        processSerialCommand(serialLine);
      } else {
        showSerialHelp();
      }
      serialLineLength = 0;
      continue;
    }

    if(serialLineLength < sizeof(serialLine) - 1){
      serialLine[serialLineLength++] = incoming;
    }
  }
}

static void applyGaugeTargetsFromCan(const CAN_message_t &frame){
  if(gaugeSetupModeEnabled){
    return;
  }

  if(speedSource.enabled && frame.id == speedSource.canId){
    unsigned int speedValue = applyGaugeScale(readCanRawValue(frame, speedSource), speedSource, SPEEDOMETER_MAX_MPH);
    leftGauge.setPosition(mapGaugeValueToStep(speedValue, SPEEDOMETER_MAX_MPH, leftGauge));
    if(serialTelemetryEnabled){
      Serial.print("speed=");
      Serial.println(speedValue);
    }
  }

  if(rpmSource.enabled && frame.id == rpmSource.canId){
    unsigned int rpmValue = applyGaugeScale(readCanRawValue(frame, rpmSource), rpmSource, TACHOMETER_MAX_RPM);
    rightGauge.setPosition(mapGaugeValueToStep(rpmValue, TACHOMETER_MAX_RPM, rightGauge));
    if(serialTelemetryEnabled){
      Serial.print("rpm=");
      Serial.println(rpmValue);
    }
  }
}

static void serviceBothGaugeMotors(){
  // Motion is handled by blocking setPosition/home calls in the current API.
}

static void homeGaugesTogether(){
  leftGauge.home();
  rightGauge.home();
}



static void syncRingBacklightFromInputs(unsigned long nowMs){
  bool inputConnected = (lastInputHeartbeatMs != 0 && (nowMs - lastInputHeartbeatMs) <= HEARTBEAT_TIMEOUT_MS);
  bool shouldEnable = inputConnected && (currentPod || currentLightBar);
  if(shouldEnable == ringGaugeLightEnabled){
    return;
  }

  ringGaugeLightEnabled = shouldEnable;
  if(ringGaugeLightEnabled){
    dashLights.gaugelight_on();
  }
  else {
    dashLights.gaugelight_off();
  }
}

static void updateCanRxStatusFields(unsigned long nowMs){
  temp.updateTopField(4, 0, "Keypad", (lastKeypadHeartbeatMs != 0 && (nowMs - lastKeypadHeartbeatMs) <= HEARTBEAT_TIMEOUT_MS) ? "CONNECTED" : "--");
  temp.updateTopField(4, 1, "Input", (lastInputHeartbeatMs != 0 && (nowMs - lastInputHeartbeatMs) <= HEARTBEAT_TIMEOUT_MS) ? "CONNECTED" : "--");
  temp.updateTopField(4, 2, "ECU", (lastEcuHeartbeatMs != 0 && (nowMs - lastEcuHeartbeatMs) <= HEARTBEAT_TIMEOUT_MS) ? "CONNECTED" : "--");
  temp.updateTopField(4, 3, "RS", (lastRearSteerHeartbeatMs != 0 && (nowMs - lastRearSteerHeartbeatMs) <= HEARTBEAT_TIMEOUT_MS) ? "CONNECTED" : "--");
}

static void updateInputPageFields(unsigned long nowMs){
  bool inputConnected = (lastInputHeartbeatMs != 0 && (nowMs - lastInputHeartbeatMs) <= HEARTBEAT_TIMEOUT_MS);

  if(inputConnected){
    temp.updateTopField(5, 0, "AWD", currentAWD ? "ON" : "OFF");
    temp.updateTopField(5, 1, "Diff", currentDiff ? "ON" : "OFF");
    temp.updateTopField(5, 2, "Pod", currentPod ? "ON" : "OFF");
    temp.updateTopField(5, 3, "LightBar", currentLightBar ? "ON" : "OFF");
    temp.updateTopField(5, 4, "Brake", currentBrake ? "ON" : "OFF");
  }
  else {
    temp.updateTopField(5, 0, "AWD", "--");
    temp.updateTopField(5, 1, "Diff", "--");
    temp.updateTopField(5, 2, "Pod", "--");
    temp.updateTopField(5, 3, "LightBar", "--");
    temp.updateTopField(5, 4, "Brake", "--");
  }
}

static void updateEcuPageFields(unsigned long nowMs){
  bool ecuConnected = (lastEcuHeartbeatMs != 0 && (nowMs - lastEcuHeartbeatMs) <= HEARTBEAT_TIMEOUT_MS);

  if(ecuConnected){
    temp.updateTopField(0, 0, "Battery V", ecu1BatteryV);
    temp.updateTopField(0, 1, "Oil Pressure", ecu1OilPressure);
    temp.updateTopField(0, 2, "Oil Temp", ecu1OilTemp);
    temp.updateTopField(0, 3, "MAP", ecu1Map);
    temp.updateTopField(0, 4, "O2", ecu1O2);

    temp.updateTopField(1, 0, "IAT", ecu2Iat);
    temp.updateTopField(1, 1, "CHT Avg", ecu2ChtAvg);
    temp.updateTopField(1, 2, "Target Boost", ecu2TargetBoost);
    temp.updateTopField(1, 3, "Ethanol %", ecu2Ethanol);
    temp.updateTopField(1, 4, "Baro", ecu2Baro);

    temp.updateTopField(2, 0, "EGT1", ecu3Egt1);
    temp.updateTopField(2, 1, "EGT2", ecu3Egt2);
    temp.updateTopField(2, 2, "TPS", ecu3Tps);
    temp.updateTopField(2, 3, "Fuel P", ecu3FuelP);
  }
  else {
    temp.updateTopField(0, 0, "Battery V", "--");
    temp.updateTopField(0, 1, "Oil Pressure", "--");
    temp.updateTopField(0, 2, "Oil Temp", "--");
    temp.updateTopField(0, 3, "MAP", "--");
    temp.updateTopField(0, 4, "O2", "--");

    temp.updateTopField(1, 0, "IAT", "--");
    temp.updateTopField(1, 1, "CHT Avg", "--");
    temp.updateTopField(1, 2, "Target Boost", "--");
    temp.updateTopField(1, 3, "Ethanol %", "--");
    temp.updateTopField(1, 4, "Baro", "--");

    temp.updateTopField(2, 0, "EGT1", "--");
    temp.updateTopField(2, 1, "EGT2", "--");
    temp.updateTopField(2, 2, "TPS", "--");
    temp.updateTopField(2, 3, "Fuel P", "--");
  }
}

static void sendStatusMessage(FaultId fault){
  CAN_message_t status;
  status.id = STATUS_CAN_ID;
  status.flags.extended = 0;
  status.flags.remote = 0;
  status.len = 8;

  status.buf[0] = heartbeatCounter++;
  status.buf[1] = static_cast<uint8_t>(fault);
  status.buf[2] = static_cast<uint8_t>(temp.getTopPageCount());
  status.buf[3] = static_cast<uint8_t>(currentKeypadPage + 1);
  status.buf[4] = 0;
  status.buf[5] = 0;
  status.buf[6] = 0;
  status.buf[7] = 0;

  can1.write(status);
}

void processCanMessage(const CAN_message_t &msg){
  char buf[16];
  bool pageChanged = false;
  bool modeChanged = false;
  unsigned long nowMs = millis();

  if(serialTelemetryEnabled){
    Serial.print("CAN RX ID=0x");
    Serial.println(msg.id, HEX);
  }

  if(msg.id == 0x320){
    snprintf(ecu1BatteryV, sizeof(ecu1BatteryV), "%u", msg.buf[5]);
    if(serialTelemetryEnabled){
      Serial.print(" Battery V="); Serial.println(ecu1BatteryV);
    }

    snprintf(ecu1OilTemp, sizeof(ecu1OilTemp), "%u", msg.buf[4]);
    if(serialTelemetryEnabled){
      Serial.print(" Oil Temp="); Serial.println(ecu1OilTemp);
    }

    snprintf(ecu1Map, sizeof(ecu1Map), "%u", combineBytes(msg.buf[1], msg.buf[2]));
    if(serialTelemetryEnabled){
      Serial.print(" MAP="); Serial.println(ecu1Map);
    }

    snprintf(ecu1O2, sizeof(ecu1O2), "%u", msg.buf[6]);
    if(serialTelemetryEnabled){
      Serial.print(" O2="); Serial.println(ecu1O2);
    }
  }
  else if(msg.id == 0x325){
    snprintf(ecu2Iat, sizeof(ecu2Iat), "%u", msg.buf[0]);
    if(serialTelemetryEnabled){
      Serial.print(" IAT="); Serial.println(ecu2Iat);
    }

    snprintf(ecu2TargetBoost, sizeof(ecu2TargetBoost), "%u", msg.buf[2]);
    if(serialTelemetryEnabled){
      Serial.print(" Target Boost="); Serial.println(ecu2TargetBoost);
    }

    snprintf(ecu2Ethanol, sizeof(ecu2Ethanol), "%u", msg.buf[3]);
    if(serialTelemetryEnabled){
      Serial.print(" Ethanol %="); Serial.println(ecu2Ethanol);
    }

    snprintf(ecu2Baro, sizeof(ecu2Baro), "%u", msg.buf[4]);
    if(serialTelemetryEnabled){
      Serial.print(" Baro="); Serial.println(ecu2Baro);
    }

    snprintf(ecu3Tps, sizeof(ecu3Tps), "%u", msg.buf[1]);
    if(serialTelemetryEnabled){
      Serial.print(" TPS="); Serial.println(ecu3Tps);
    }

    snprintf(ecu3FuelP, sizeof(ecu3FuelP), "%u", msg.buf[7]);
    if(serialTelemetryEnabled){
      Serial.print(" Fuel P="); Serial.println(ecu3FuelP);
    }
  }
  else if(msg.id == 0x330){
    snprintf(ecu1OilPressure, sizeof(ecu1OilPressure), "%u", msg.buf[0]);
    if(serialTelemetryEnabled){
      Serial.print(" Oil Pressure="); Serial.println(ecu1OilPressure);
    }

    uint16_t cht = ((uint16_t)msg.buf[1] + (uint16_t)msg.buf[2]) / 2;
    snprintf(ecu2ChtAvg, sizeof(ecu2ChtAvg), "%u", cht);
    if(serialTelemetryEnabled){
      Serial.print(" CHT Avg="); Serial.println(ecu2ChtAvg);
    }

    snprintf(ecu3Egt1, sizeof(ecu3Egt1), "%u", msg.buf[3]);
    if(serialTelemetryEnabled){
      Serial.print(" EGT1="); Serial.println(ecu3Egt1);
    }

    snprintf(ecu3Egt2, sizeof(ecu3Egt2), "%u", msg.buf[4]);
    if(serialTelemetryEnabled){
      Serial.print(" EGT2="); Serial.println(ecu3Egt2);
    }
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
    }

    uint8_t newAngle = msg.buf[1] < 1 ? 1 : msg.buf[1];
    if(newAngle != currentRearSteerAngle){
      currentRearSteerAngle = newAngle;
      snprintf(buf, sizeof(buf), "%u", currentRearSteerAngle);
      temp.updateTopField(3, 2, "Angle", buf);
      if(currentKeypadPage != 3){
        currentKeypadPage = 3;
        temp.setTopFieldPage(3);
        pageChanged = true;
      }
    }

    bool newLaunch = msg.buf[3] != 0;
    if(newLaunch != currentLaunchControl){
      currentLaunchControl = newLaunch;
      temp.updateTopField(2, 4, "Launch", currentLaunchControl ? "ON" : "OFF");
      if(currentKeypadPage != 2){
        currentKeypadPage = 2;
        temp.setTopFieldPage(2);
        pageChanged = true;
      }
    }
  }
  else if(msg.id == INPUTMOD_MES1_CAN_ID){
    bool newAWD = (msg.buf[0] & 0x01) != 0;
    bool newDiff = (msg.buf[0] & 0x02) != 0;
    bool newPod = (msg.buf[0] & 0x04) != 0;
    bool newLightBar = (msg.buf[0] & 0x08) != 0;
    bool newBrake = (msg.buf[0] & 0x10) != 0;
    uint8_t newGearValue = msg.buf[1];
    bool inputChanged = false;

    if(newAWD != currentAWD){
      currentAWD = newAWD;
      if(currentAWD){
        temp.AWD_engage();
      }
      else {
        temp.AWD_disengage();
      }
      inputChanged = true;
    }
    if(newDiff != currentDiff){
      currentDiff = newDiff;
      if(currentDiff){
        temp.diffLock();
      }
      else {
        temp.diffUnlock();
      }
      inputChanged = true;
    }
    if(newPod != currentPod){
      currentPod = newPod;
      inputChanged = true;
    }
    if(newLightBar != currentLightBar){
      currentLightBar = newLightBar;
      inputChanged = true;
    }
    if(newBrake != currentBrake){
      currentBrake = newBrake;
      inputChanged = true;
    }

    if(newGearValue != currentGearValue){
      currentGearValue = newGearValue;
      switch(currentGearValue){
        case 1: temp.shift(PARK); break;
        case 2: temp.shift(REVERSE); break;
        case 3: temp.shift(NEUTRAL); break;
        case 4: temp.shift(LOWGEAR); break;
        case 5: temp.shift(DRIVE); break;
        default: temp.shift(PARK); break;
      }
      inputChanged = true;
    }

    syncRingBacklightFromInputs(nowMs);

    if(inputChanged){
      // input values already update individual rows when their page is active
    }
  }
  else if(msg.id == KEYPAD_STATUS_CAN_ID){
    lastKeypadHeartbeatMs = nowMs;
  }
  else if(msg.id == INPUTMOD_STATUS_CAN_ID){
    lastInputHeartbeatMs = nowMs;
  }
  else if(msg.id == ECU_STATUS_CAN_ID){
    lastEcuHeartbeatMs = nowMs;
  }
  else if(msg.id == REARSTEER_STATUS_CAN_ID){
    lastRearSteerHeartbeatMs = nowMs;
  }

  applyGaugeTargetsFromCan(msg);

  if(modeChanged){
    fullDisplayUpdatePending = true;
  }
  else if(pageChanged){
    displayUpdatePending = true;
  }
}

void setup() {
  Serial.begin(115200);                   // Start serial for debugging and CAN message logging
  unsigned long serialWaitStart = millis();
  while(!Serial && (millis() - serialWaitStart) < 3000){
  }
  can1.begin();                           // Initialize CAN bus hardware
  can1.setBaudRate(250000);               // Set CAN speed to 250 kbps to match the ECU broadcaster
  can1.setMB(MB0, RX, STD);                 // Enable mailbox 0 for standard CAN receive.
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
    "Launch"       // ECU 3 page field 4
  };
  temp.addTopFieldPage("ECU 3", labels2, 5);

  const char labels3[][16] = {
    "Avg Power",   // Rear Steer page field 0
    "Peak Power",  // Rear Steer page field 1
    "Angle",       // Rear Steer page field 2
    "",            // Rear Steer page field 3
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
  //temp.splashScreen2(5000);              // Show secondary splash

  showSerialHelp();
  Serial.println("Default zero is 50 steps above the stop; default max is 50 steps below full travel.");
  Serial.println("Running startup homing sweep (up 1200, down 1200)...");
  homeGaugesTogether();
  if(loadCalibrationFromEEPROM()){
    Serial.println("Gauge calibration loaded from EEPROM.");
  } else {
    Serial.println("No EEPROM calibration found. Applying default calibration.");
    applyDefaultGaugeCalibration();
    saveCalibrationToEEPROM();
  }
  printGaugeCalibration("speed", leftGauge);
  printGaugeCalibration("rpm", rightGauge);

  temp.setTopFieldPage(0);               // Fix display to live CAN page 0.

  // Draw initial placeholders and the full display layout.
  temp.updateTopField(0, 0, "Battery V", "--");
  temp.updateTopField(0, 1, "Oil Pressure", "--");
  temp.updateTopField(0, 2, "Oil Temp", "--");
  temp.updateTopField(0, 3, "MAP", "--");
  temp.updateTopField(0, 4, "O2", "--");
  temp.updateAll();
  if(currentAWD){
    temp.AWD_engage();
  }
  else {
    temp.AWD_disengage();
  }
  if(currentDiff){
    temp.diffLock();
  }
  else {
    temp.diffUnlock();
  }
  delay(1000);                           // Pause so user can see initial page
}
  

void loop() {
  static unsigned long lastRefresh = 0;

  announceSerialConsoleIfReady();
  serviceSerialConsole();

  // Read all available CAN messages and mark updates.
  if(can1.read(msg)){
    if(serialTelemetryEnabled){
      Serial.println("CAN RX message");
    }
    processCanMessage(msg);
  }

  unsigned long now = millis();
  serviceBothGaugeMotors();
  updateCanRxStatusFields(now);
  updateEcuPageFields(now);
  updateInputPageFields(now);
  syncRingBacklightFromInputs(now);

  if(now - lastStatusBroadcast >= STATUS_BROADCAST_INTERVAL_MS){
    lastStatusBroadcast = now;
    FaultId currentFault = evaluateFaults(batt.read(), ambientTempSensor.read());
    sendStatusMessage(currentFault);
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

