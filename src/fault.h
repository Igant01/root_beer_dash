#ifndef FAULT_H
#define FAULT_H

#include <stdint.h>

enum FaultId {
    FAULT_NONE = 0,
    FAULT_BATTERY_LOW = 1,
    FAULT_TEMPERATURE_HIGH = 2,
    FAULT_COUNT
};

struct FaultInfo {
    FaultId id;
    const char *name;
    const char *description;
};

static constexpr FaultInfo FAULT_DEFINITIONS[] = {
    { FAULT_NONE, "None", "No active fault" },
    { FAULT_BATTERY_LOW, "Battery Low", "Battery voltage below threshold" },
    { FAULT_TEMPERATURE_HIGH, "Temperature High", "Ambient temperature above threshold" },
};

static inline const FaultInfo &getFaultInfo(FaultId faultId) {
    if (faultId >= FAULT_NONE && faultId < FAULT_COUNT) {
        return FAULT_DEFINITIONS[faultId];
    }
    return FAULT_DEFINITIONS[FAULT_NONE];
}

static inline const char *getFaultName(FaultId faultId) {
    return getFaultInfo(faultId).name;
}

static inline const char *getFaultDescription(FaultId faultId) {
    return getFaultInfo(faultId).description;
}

struct FaultContainer {
    static const uint8_t MAX_FAULTS = 128;
    static const uint8_t WORDS = MAX_FAULTS / 32;
    uint32_t bits[WORDS];

    FaultContainer() { clearAll(); }

    void clearAll() {
        for (uint8_t i = 0; i < WORDS; ++i) {
            bits[i] = 0;
        }
    }

    void set(uint8_t faultId) {
        if (faultId == FAULT_NONE || faultId > MAX_FAULTS) return;
        uint8_t index = (faultId - 1) / 32;
        uint8_t bit = (faultId - 1) % 32;
        bits[index] |= (1UL << bit);
    }

    void clear(uint8_t faultId) {
        if (faultId == FAULT_NONE || faultId > MAX_FAULTS) return;
        uint8_t index = (faultId - 1) / 32;
        uint8_t bit = (faultId - 1) % 32;
        bits[index] &= ~(1UL << bit);
    }

    bool test(uint8_t faultId) const {
        if (faultId == FAULT_NONE || faultId > MAX_FAULTS) return false;
        uint8_t index = (faultId - 1) / 32;
        uint8_t bit = (faultId - 1) % 32;
        return (bits[index] & (1UL << bit)) != 0;
    }

    bool any() const {
        for (uint8_t i = 0; i < WORDS; ++i) {
            if (bits[i] != 0) return true;
        }
        return false;
    }

    uint8_t firstActiveFault() const {
        for (uint8_t i = 0; i < MAX_FAULTS; ++i) {
            if (test(i + 1)) return i + 1;
        }
        return FAULT_NONE;
    }

    uint16_t activeMask16() const {
        return static_cast<uint16_t>(bits[0] & 0xFFFFu);
    }
};

#endif // FAULT_H
