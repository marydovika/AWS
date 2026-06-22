#ifndef SENSOR_VALIDATOR_H
#define SENSOR_VALIDATOR_H

#include <Arduino.h>
#include "SensorData.h"

class SensorValidator {
public:
    // Runs range, stuck-at, and abrupt-change checks on the
    // currently wired sensors, logging any violations via ErrorLogger.
    static void validate(const SensorData &data);

private:
    static void checkRange(const SensorData &data);
    static void checkStuckAndAbrupt(const SensorData &data);
};

#endif
