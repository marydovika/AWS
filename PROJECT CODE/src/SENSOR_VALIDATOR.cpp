#include "SENSOR_VALIDATOR.h"
#include "ERROR_LOGGER.h"
#include <math.h>

// ── Persistent state across deep sleep ──────────────────────────────────────
RTC_DATA_ATTR float   lastTemperature = NAN;
RTC_DATA_ATTR float   lastHumidity    = NAN;
RTC_DATA_ATTR float   lastPressure    = NAN;
RTC_DATA_ATTR float   lastVoltBatt    = NAN;
RTC_DATA_ATTR float   lastVoltSolar   = NAN;

RTC_DATA_ATTR uint8_t stuckCountTemp  = 0;
RTC_DATA_ATTR uint8_t stuckCountHum   = 0;
RTC_DATA_ATTR uint8_t stuckCountPress = 0;
RTC_DATA_ATTR uint8_t stuckCountBatt  = 0;
RTC_DATA_ATTR uint8_t stuckCountSolar = 0;

#define STUCK_THRESHOLD_CYCLES 3

static void checkOutOfRange(const char* field, float value, float minV, float maxV) {
    if (isnan(value)) return; // sensor failure already logged at its own source
    if (value < minV || value > maxV) {
        String detail = String(field) + "=" + String(value, 2) +
                         " (valid: " + String(minV, 1) + " to " + String(maxV, 1) + ")";
        ErrorLogger::log(COMP_VALIDATOR, ERR_VALIDATOR_OUT_OF_RANGE, detail.c_str());
    }
}

static void checkFieldDrift(const char* field, float current, float &lastValue,
                            uint8_t &stuckCount, float maxDelta) {
    if (isnan(current)) return;

    if (!isnan(lastValue)) {
        float delta = current - lastValue;

        if (current == lastValue) {
            stuckCount++;
            if (stuckCount >= STUCK_THRESHOLD_CYCLES) {
                String detail = String(field) + " unchanged for " + String(stuckCount) + " cycles: " + String(current, 2);
                ErrorLogger::log(COMP_VALIDATOR, ERR_VALIDATOR_STUCK_AT, detail.c_str());
            }
        } else {
            stuckCount = 0;
        }

        if (fabs(delta) > maxDelta) {
            String detail = String(field) + " jumped " + String(delta, 2) +
                             " in one cycle (max expected: " + String(maxDelta, 1) + ")";
            ErrorLogger::log(COMP_VALIDATOR, ERR_VALIDATOR_ABRUPT_CHANGE, detail.c_str());
        }
    }

    lastValue = current;
}

void SensorValidator::validate(const SensorData &data) {
    checkRange(data);
    checkStuckAndAbrupt(data);
}

void SensorValidator::checkRange(const SensorData &data) {
    checkOutOfRange("temperature_c",  data.temperature,        -40.0f,   85.0f);
    checkOutOfRange("humidity_pct",   data.humidity,              0.0f,  100.0f);
    checkOutOfRange("pressure_hpa",   data.airPressure,         300.0f, 1100.0f);
    checkOutOfRange("altitude_m",     data.altitude,           -500.0f, 9000.0f);
    checkOutOfRange("light_v",        data.lightLevel,            0.0f,    3.3f);
    checkOutOfRange("soil_moist_v",   data.soilMoisture,          0.0f,    3.3f);
    checkOutOfRange("rain_count",     (float)data.rainCount,      0.0f, 1000.0f);
    checkOutOfRange("wind_speed_kph", data.windSpeed,             0.0f,  150.0f);
    checkOutOfRange("wind_dir_deg",   (float)data.windDirection,  0.0f,  359.0f);
    checkOutOfRange("volt_3v3",       data.volt_3v3,              0.0f,    3.6f);
    checkOutOfRange("volt_5v",        data.volt_5v,               0.0f,    5.5f);
    checkOutOfRange("volt_batt",      data.volt_batt,             0.0f,   15.0f);
    checkOutOfRange("volt_solar",     data.volt_solar,            0.0f,   25.0f);
    checkOutOfRange("volt_dc",        data.volt_dc,               0.0f,   25.0f);
    checkOutOfRange("curr_batt",      data.curr_batt,            -5.0f,    5.0f);
    checkOutOfRange("curr_solar",     data.curr_solar,           -0.5f,    5.0f);
}

void SensorValidator::checkStuckAndAbrupt(const SensorData &data) {
    checkFieldDrift("temperature_c", data.temperature, lastTemperature, stuckCountTemp,  5.0f);
    checkFieldDrift("humidity_pct",  data.humidity,    lastHumidity,    stuckCountHum,   20.0f);
    checkFieldDrift("pressure_hpa",  data.airPressure, lastPressure,    stuckCountPress, 10.0f);
    checkFieldDrift("volt_batt",     data.volt_batt,   lastVoltBatt,    stuckCountBatt,   2.0f);
    checkFieldDrift("volt_solar",    data.volt_solar,  lastVoltSolar,   stuckCountSolar, 9999.0f);
}
