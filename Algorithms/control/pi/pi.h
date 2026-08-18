#ifndef ALGORITHMS_CONTROL_PI_PI_H_
#define ALGORITHMS_CONTROL_PI_PI_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum { PI_ACTION_DIRECT = 0, PI_ACTION_REVERSE = 1 } PI_Action;
typedef enum {
    PI_ANTI_WINDUP_NONE = 0,
    PI_ANTI_WINDUP_CONDITIONAL = 1,
    PI_ANTI_WINDUP_BACK_CALCULATION = 2
} PI_AntiWindupMode;

typedef struct {
    int32_t kpQ16;
    int32_t kiQ16PerSecond;
    uint32_t samplePeriodUs;
    int32_t outputMin;
    int32_t outputMax;
    int32_t integratorMin;
    int32_t integratorMax;
    int32_t deadband;
    int32_t feedForward;
    uint32_t outputRiseLimit;
    uint32_t outputFallLimit;
    int32_t backCalculationGainQ16;
    PI_Action action;
    PI_AntiWindupMode antiWindup;
} PI_Config;

typedef struct {
    int32_t setpoint, measurement, error, effectiveError;
    int32_t proportional, integral, feedForward;
    int32_t unclampedOutput, output;
    bool outputLimited, slewLimited, integratorLimited;
    bool automatic, valid;
} PI_Result;

typedef struct {
    PI_Config config;
    int32_t kiStepQ16;
    int64_t integratorQ16;
    int32_t previousOutput;
    bool automatic;
    bool initialized;
} PI_Controller;

bool PI_init(PI_Controller *controller, const PI_Config *config);
bool PI_reset(PI_Controller *controller, int32_t initialOutput);
bool PI_update(PI_Controller *controller, int32_t setpoint,
               int32_t measurement, PI_Result *result);
bool PI_setManualMode(PI_Controller *controller, int32_t output);
bool PI_setAutomaticMode(PI_Controller *controller, int32_t setpoint,
                         int32_t measurement);
bool PI_setTunings(PI_Controller *controller, int32_t kpQ16,
                   int32_t kiQ16PerSecond);
bool PI_setSamplePeriod(PI_Controller *controller, uint32_t samplePeriodUs);
bool PI_setOutputLimits(PI_Controller *controller, int32_t minimum,
                        int32_t maximum);
bool PI_setIntegratorLimits(PI_Controller *controller, int32_t minimum,
                            int32_t maximum);
bool PI_setFeedForward(PI_Controller *controller, int32_t feedForward);
bool PI_setDeadband(PI_Controller *controller, int32_t deadband);
bool PI_setSlewLimits(PI_Controller *controller, uint32_t rise,
                      uint32_t fall);
int32_t PI_getOutput(const PI_Controller *controller);
int32_t PI_getIntegrator(const PI_Controller *controller);
bool PI_isAutomatic(const PI_Controller *controller);

#endif
