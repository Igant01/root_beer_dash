#ifndef FAULT_H
#define FAULT_H

#include <stdint.h>

enum FaultId {
    FAULT_NONE = 0,           // No active fault
    FAULT_BATTERY_LOW = 1,    // Battery voltage below threshold
    FAULT_TEMPERATURE_HIGH = 2 // Ambient temperature above threshold
};

// Evaluate current sensor state and return a single active fault ID.
FaultId evaluateFaults(float batteryVolts, int ambientF);

#endif // FAULT_H
