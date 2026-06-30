#ifndef GAUGESTEPPER_H
#define GAUGESTEPPER_H

#include <AccelStepper.h>

class X27168 {

    private:
        AccelStepper stepper;
        int absPosition; //absolute stored position
        int microStep;
        int stepPin;
        int dirPin;
        int absMax; //physical max
        int absMin; //physical min
        int relMax; //relative max
        int relMin; //relative min
        int maxSpeed;
        int maxAcceleration;

    public:
        X27168(int step,int dir);
        int getPosition();
        void setPosition(int set);
        void home();
        void setRelMax(int max);
        void setRelMin(int min);
        int getRelMax();
        int getRelMin();

};

#endif