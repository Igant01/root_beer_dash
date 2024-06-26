#include <lights.h>
#include <Arduino.h>
#define backLight 6 //active low
#define PWM 7 //active low

cluster_lights::cluster_lights(){
    pinMode(backLight,OUTPUT);
    pinMode(PWM,OUTPUT);
}

void cluster_lights::on(){
    analogWrite(backLight,0);
    analogWrite(PWM,0);
}

void cluster_lights::off(){
    analogWrite(backLight,255);
    analogWrite(PWM,255);
}

void cluster_lights::set(int brightness){
    analogWrite(backLight,brightness);
    analogWrite(PWM,brightness);
}

void cluster_lights::backlight_on(){
    analogWrite(backLight,0);
}

void cluster_lights::backlight_off(){
    analogWrite(backLight,255);
}

void cluster_lights::backlight_set(int brightness){
    analogWrite(backLight,brightness);
}

void cluster_lights::gaugelight_on(){
    analogWrite(PWM,0);
}

void cluster_lights::gaugelight_off(){
    analogWrite(PWM,255);
}

void cluster_lights::gaugelight_set(int brightness){
    analogWrite(PWM,brightness);
}