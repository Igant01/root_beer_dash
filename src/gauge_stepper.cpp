#include <Arduino.h>
#include <gauge_stepper.h>
int pause = 20; //delay for mechanical limitation

X27168::X27168(int step,int dir){
    absPosition = 0;
    microStep = 2; 
    positionKnown = false;
    stepPin = step;
    dirPin = dir;
    pinMode(step,OUTPUT);
    pinMode(dir,OUTPUT);
    digitalWrite(stepPin,LOW);
    digitalWrite(dirPin,LOW);

}

int X27168::getPosition(){
    return absPosition;
}

void X27168::setPosition(int set){ //600 steps * microstep
    if(positionKnown){
        if(set>absPosition){
            digitalWrite(dirPin,HIGH);
            for(int i=0; i<=set-absPosition; i++){
                digitalWrite(stepPin,HIGH);
                delay(pause);
                digitalWrite(stepPin,LOW);
                delay(pause);
            }
        }
        if(set<absPosition){
            digitalWrite(dirPin,LOW);
            for(int i=0; i<absPosition-set; i++){
                digitalWrite(stepPin,HIGH);
                delay(pause);
                digitalWrite(stepPin,LOW);
                delay(pause);                
            }
        }   
    absPosition = set;
    }
}

void X27168::home(){
    digitalWrite(dirPin,HIGH);
    for(int i=0; i<=600*microStep; i++){
        digitalWrite(stepPin,HIGH);
        delay(pause);
        digitalWrite(stepPin,LOW);
        delay(pause);
    }
    digitalWrite(dirPin,LOW);
    for(int i=0; i<=600*microStep; i++){
        digitalWrite(stepPin,HIGH);
        delay(pause);
        digitalWrite(stepPin,LOW);
        delay(pause);
    }
    absPosition = 0;
    positionKnown = true;
}

void X27168::setMicroSteps(int microSteps){
    microStep = microSteps;
}

bool X27168::homed(){
    return positionKnown;
}