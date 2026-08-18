#ifndef ALGORITHMS_CONTROL_PID_PID_H_
#define ALGORITHMS_CONTROL_PID_PID_H_
#include <stdbool.h>
#include <stdint.h>
#include "../pi/pi.h"

typedef struct {
    int32_t kpQ16, kiQ16PerSecond, kdQ16Second;
    uint32_t samplePeriodUs;
    int32_t outputMin, outputMax, integratorMin, integratorMax;
    int32_t deadband, feedForward;
    uint32_t outputRiseLimit, outputFallLimit;
    int32_t backCalculationGainQ16;
    uint16_t derivativeFilterQ15;
    bool derivativeOnMeasurement;
    PI_Action action;
    PI_AntiWindupMode antiWindup;
} PID_Config;

typedef struct {
    int32_t setpoint, measurement, error, effectiveError;
    int32_t proportional, integral, derivative, feedForward;
    int32_t unclampedOutput, output;
    bool outputLimited, slewLimited, integratorLimited;
    bool automatic, valid;
} PID_Result;

typedef struct {
    PID_Config config;
    int32_t kiStepQ16, kdStepQ16;
    int64_t integratorQ16, derivativeQ16;
    int32_t previousOutput, previousMeasurement, previousError;
    bool automatic, initialized, hasPrevious;
} PID_Controller;

bool PID_init(PID_Controller *, const PID_Config *);
bool PID_reset(PID_Controller *, int32_t initialOutput);
bool PID_update(PID_Controller *, int32_t setpoint, int32_t measurement, PID_Result *);
bool PID_setManualMode(PID_Controller *, int32_t output);
bool PID_setAutomaticMode(PID_Controller *, int32_t setpoint, int32_t measurement);
bool PID_setTunings(PID_Controller *, int32_t kpQ16, int32_t kiQ16PerSecond, int32_t kdQ16Second);
bool PID_setSamplePeriod(PID_Controller *, uint32_t samplePeriodUs);
bool PID_setOutputLimits(PID_Controller *, int32_t minimum, int32_t maximum);
bool PID_setIntegratorLimits(PID_Controller *, int32_t minimum, int32_t maximum);
bool PID_setDerivativeMode(PID_Controller *, bool derivativeOnMeasurement);
bool PID_setDerivativeFilter(PID_Controller *, uint16_t filterQ15);
bool PID_setFeedForward(PID_Controller *, int32_t feedForward);
bool PID_setDeadband(PID_Controller *, int32_t deadband);
bool PID_setSlewLimits(PID_Controller *, uint32_t rise, uint32_t fall);
int32_t PID_getOutput(const PID_Controller *);
int32_t PID_getIntegrator(const PID_Controller *);
bool PID_isAutomatic(const PID_Controller *);
#endif
