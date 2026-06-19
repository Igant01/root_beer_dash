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
    MANUAL,
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
        // Maximum number of header pages that can be stored.
        enum { MAX_TOP_FIELD_PAGES = 256 };
        // Maximum number of fields displayed on each page.
        static const uint8_t MAX_TOP_FIELDS = 5;
        // Maximum length for labels, values, and titles.
        static const uint8_t MAX_FIELD_TEXT = 16;

        struct TopFieldPage {
            uint8_t fieldCount;                             // Number of fields on this page.
            char title[MAX_FIELD_TEXT];                     // Page title displayed in the top header.
            char labels[MAX_TOP_FIELDS][MAX_FIELD_TEXT];    // Static field labels.
            char values[MAX_TOP_FIELDS][MAX_FIELD_TEXT];    // Dynamic field values updated from CAN.
        };

        bool diff;                                         // Differential lock state.
        bool AWD;                                          // All-wheel drive state.
        rearSteer mode;                                    // Rear steer mode.
        float angle;                                       // Steering angle placeholder.
        GEAR currentGear;                                  // Current gear state.
        TopFieldPage topPages[MAX_TOP_FIELD_PAGES];        // Header page storage.
        uint16_t topPageCount;                             // Number of added pages.
        uint8_t currentTopPage;                            // Currently visible header page.

        static void skeleton();                            // Draws the wireframe vehicle.
        static void rearSteerOffInvert();                  // Clears previous front wheel steering state.
        static void rearSteerCrabInvert();                 // Clears previous crab steering state.
        static void rearSteerNormalInvert();               // Clears previous normal steering state.
        static void rearSteerManualInvert();               // Clears previous manual steering state.
        void renderTopFieldPage();                         // Draws the top header page.
        void drawTopFieldRow(uint8_t row, const char* label, const char* value); // Draws one label/value row.
        void drawTopFieldValue(uint8_t row, const char* label, const char* value); // Draws only the value portion.
        bool validateTopFieldIndexes(uint16_t pageIndex, uint8_t fieldIndex) const;
        void copyTopFieldText(char* dest, const char* source);

    public:
        mainScreen();
        void init();
        void updateAll();
        void updateTopHeader();
        void splashScreen(int time);
        void splashScreen2(int time);
        void diffLock();
        void diffUnlock();
        void AWD_engage();
        void AWD_disengage();
        void rearSteerOff();
        void rearSteerCrab();
        void rearSteerNormal();
        void rearSteerManual();
        void shift(GEAR inputGear);
        void unlockDiffScreen(int time);
        void lockDiffScreen(int time);
        void AWD_engageScreen(int time);
        void AWD_disengageScreen(int time);
        void rear_calibration_screen(int time);

        bool addTopFieldPage(const char title[MAX_FIELD_TEXT], const char labels[][MAX_FIELD_TEXT], uint8_t fieldCount);
        bool removeTopFieldPage(uint8_t pageIndex);
        bool setTopFieldPage(uint8_t pageIndex);
        bool nextTopFieldPage();
        bool prevTopFieldPage();
        uint16_t getTopPageCount() const;
        bool updateTopField(uint8_t pageIndex, uint8_t fieldIndex, const char* label, const char* value);
};

#endif