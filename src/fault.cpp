#include "fault.h"

inline constexpr float BATTERY_FAULT_THRESHOLD = 11.0f;      // volts
inline constexpr int TEMPERATURE_FAULT_THRESHOLD_F = 185;    // Fahrenheit

FaultId evaluateFaults(float batteryVolts, int ambientF) {
    if (batteryVolts < BATTERY_FAULT_THRESHOLD) {
        return FAULT_BATTERY_LOW;
    }
    if (ambientF > TEMPERATURE_FAULT_THRESHOLD_F) {
        return FAULT_TEMPERATURE_HIGH;
    }
    return FAULT_NONE;
}
