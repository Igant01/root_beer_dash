#include <Arduino.h>
#include <FlexCAN_T4.h>
#include <gauge_stepper.h>
#include <volt_meter.h>
#include <lights.h>
#include <screens.h>


#define LDIR 2
#define LSTEP 3 
#define RDIR 4
#define RSTEP 5

FlexCAN_T4<CAN1, RX_SIZE_1024, TX_SIZE_1024> can1;
CAN_message_t msg;

X27168 leftGauge(LSTEP,LDIR);
X27168 rightGauge(RSTEP,RDIR);
volt_meter batt;
cluster_lights dashLights;
mainScreen temp;

void setup() {
  Serial.begin(115200);                   // Start serial for debugging and CAN message logging
  can1.begin();                           // Initialize CAN bus hardware
  can1.setBaudRate(500000);               // Set CAN speed to 500 kbps
  delay(100);                             // Brief delay for hardware stabilization

  dashLights.gaugelight_off();            // Ensure dashboard lights are off at startup
  temp.init();                            // Initialize display and clear the screen

  // Add 4 starter top-field pages with labels only. Values are updated later from CAN.
  const char labels0[][16] = {
    "Speed",
    "Battery",
    "Temp",
    "Mode",
    "Diff"
  };
  temp.addTopFieldPage("Vehicle", labels0, 5);

  const char labels1[][16] = {
    "CAN RX",
    "RPM",
    "Pressure",
    "Fuel",
    "Status"
  };
  temp.addTopFieldPage("Diagnostics", labels1, 5);

  const char labels2[][16] = {
    "Voltage",
    "Current",
    "Range",
    "Charge",
    "Temp"
  };
  temp.addTopFieldPage("Power", labels2, 5);

  const char labels3[][16] = {
    "Steering",
    "AWD",
    "RearSteer",
    "Oil",
    "Brakes"
  };
  temp.addTopFieldPage("Controls", labels3, 5);

  temp.splashScreen(1000);                // Show startup splash
  temp.splashScreen2(5000);              // Show secondary splash
  temp.updateAll();                      // Draw initial dashboard state
  delay(1000);                           // Pause so user can see initial page
}
  

void loop() {
  // Update sample field values from CAN-like inputs.
  // The first argument is the page index, the second argument is the field index.
  temp.updateTopField(0, 0, "Speed", "12 mph");   // page 0 = Vehicle, field 0 = Speed
  temp.updateTopField(0, 1, "Battery", "12.1 V"); // page 0 = Vehicle, field 1 = Battery
  temp.updateTopField(1, 0, "CAN RX", "Active");  // page 1 = Diagnostics, field 0 = CAN RX
  temp.updateTopField(1, 1, "RPM", "1200");       // page 1 = Diagnostics, field 1 = RPM

  // Cycle through the starter pages to demonstrate the header scrolling.
  for(uint8_t page = 0; page < 4; ++page){
    temp.setTopFieldPage(page);
    temp.updateAll();
    delay(2000);
  }

  temp.rear_calibration_screen(5000);   // Show calibration screen during startup demo

  temp.lockDiffScreen(3000);
  temp.diffLock();
  delay(1000);
  temp.unlockDiffScreen(3000);
  temp.diffUnlock();
  delay(1000);
  
  temp.AWD_disengageScreen(3000);
  temp.AWD_disengage();
  delay(1000);
  temp.AWD_engageScreen(3000);
  temp.AWD_engage();
  delay(1000);

  temp.rearSteerCrab();
  delay(1000);
  temp.rearSteerNormal();
  delay(1000);
  temp.rearSteerOff();
  delay(1000);

  temp.shift(REVERSE);
  delay(1000);
  temp.shift(NEUTRAL);
  delay(1000);
  temp.shift(LOWGEAR);
  delay(1000);
  temp.shift(DRIVE);
  delay(1000);
  temp.shift(PARK);
  delay(1000);

  dashLights.backlight_off();
  delay(1000);
  dashLights.backlight_on();
  delay(1000);

  dashLights.gaugelight_on();
  delay(1000);
  dashLights.gaugelight_off();
  delay(1000);
}

