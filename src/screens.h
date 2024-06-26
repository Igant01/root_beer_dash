#ifndef SCREENS_H
#define SCREENS_H

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <gear.h>
#include <locks.h>
#include <4x4.h>
#include <frame.h>
#include <rear_calibration.h>

#define DC 15
#define CS 10

enum rearSteer{
    NORMAL,
    CRAB,
    OFF,
};

enum GEAR{
    PARK,
    REVERSE,
    NEUTRAL,
    LOWGEAR,
    DRIVE,
};

class mainScreen{

    private:        
        bool diff;
        bool AWD;
        rearSteer mode;
        float angle;
        GEAR currentGear;
        static void skeleton();
        static void rearSteerOffInvert();
        static void rearSteerCrabInvert();
        static void rearSteerNormalInvert();

    public:
        mainScreen();
        void init();
        void updateAll();
        void splashScreen(int time);
        void splashScreen2(int time);
        void diffLock();
        void diffUnlock();
        void AWD_engage();
        void AWD_disengage();
        void rearSteerOff();
        void rearSteerCrab();
        void rearSteerNormal();
        void shift(GEAR inputGear);
        void unlockDiffScreen(int time);
        void lockDiffScreen(int time);
        void AWD_engageScreen(int time);
        void AWD_disengageScreen(int time);
        void rear_calibration_screen(int time);

};

#endif