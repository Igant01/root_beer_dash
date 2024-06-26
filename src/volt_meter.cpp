#include <Arduino.h>
#include <volt_meter.h>
#define battPin 21 

float volt_meter::read(){
    int volt = analogRead(battPin);
    float adcVolt = volt*0.003223; //10-bit adc 3.223mV per step 
    float finalVolt = adcVolt*13; //Vin = (13*Vout)/3  10k & 3k divider
    finalVolt = finalVolt/3;
    finalVolt = finalVolt+0.3; //0.3V diode drop
    return finalVolt;
}