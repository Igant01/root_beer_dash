#ifndef GAUGESTEPPER_H
#define GAUGESTEPPER_H

class X27168 {

    private:
        int absPosition;
        int microStep;
        bool positionKnown;
        int stepPin;
        int dirPin;

    public:
        X27168(int step,int dir);
        int getPosition();
        void setPosition(int set);
        void home();
        void setMicroSteps(int microSteps);
        bool homed();

};

#endif