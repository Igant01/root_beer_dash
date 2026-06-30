#include <Arduino.h>
#include <gauge_stepper.h>

X27168::X27168(int step,int dir)
    : stepper(AccelStepper::DRIVER, step, dir){
    absPosition = 0; //absolute stored position
    microStep = 2; //half stepping
    stepPin = step;
    dirPin = dir;
    absMax = 1200; //physical max at half-step resolution
    absMin = 0; //physical min
    relMax = 0; //relative max
    relMin = 0; //relative min
    maxSpeed = 2000;
    maxAcceleration = 250;

    stepper.setMaxSpeed(maxSpeed);
    stepper.setAcceleration(maxAcceleration);
    stepper.setPinsInverted(true, false, false);
    stepper.setCurrentPosition(absPosition);
}

int X27168::getPosition(){
    absPosition = static_cast<int>(stepper.currentPosition());
    return absPosition;
}

void X27168::setPosition(int set){
    if(set > absMax){
        set = absMax;
    }
    if(set < absMin){
        set = absMin;
    }

    stepper.moveTo(set);
    stepper.runToPosition();
    absPosition = static_cast<int>(stepper.currentPosition());
}

void X27168::home(){
    const int overdriveSteps = 1;
    int start = getPosition();

    // Overdrive upward to guarantee stop contact and intentional step loss,
    // then come back exactly one full travel to absolute zero.
    stepper.moveTo(start + (absMax - absMin) + overdriveSteps);
    stepper.runToPosition();

    stepper.setCurrentPosition(absMax);
    stepper.moveTo(absMin);
    stepper.runToPosition();
    stepper.setCurrentPosition(absMin);
    absPosition = absMin;
}

void X27168::setRelMax(int max){
    if(max > absMax){
        max = absMax;
    }
    if(max < absMin){
        max = absMin;
    }
    relMax = max;

    if(relMax < relMin){
        int swap = relMin;
        relMin = relMax;
        relMax = swap;
    }
}

void X27168::setRelMin(int min){
    if(min > absMax){
        min = absMax;
    }
    if(min < absMin){
        min = absMin;
    }
    relMin = min;

    if(relMax < relMin){
        int swap = relMin;
        relMin = relMax;
        relMax = swap;
    }
}

int X27168::getRelMax(){
    return relMax;
}

int X27168::getRelMin(){
    return relMin;
}
