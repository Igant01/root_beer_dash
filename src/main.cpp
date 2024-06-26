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
  Serial.begin(115200);
  can1.begin();
  can1.setBaudRate(500000);
  delay(100);

  dashLights.gaugelight_off();
  temp.init();
  temp.splashScreen(1000);
  temp.splashScreen2(5000);
  temp.updateAll();
  delay(1000);

}
  

void loop() {

  temp.rear_calibration_screen(5000);

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

