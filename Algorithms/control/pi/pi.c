#include "pi.h"
#include <limits.h>

static int64_t clamp64(int64_t x, int64_t lo, int64_t hi) {
    return (x < lo) ? lo : ((x > hi) ? hi : x);
}
static int32_t q16_to_i32(int64_t x) {
    int64_t y = (x >= 0) ? (x + 32768) / 65536 : (x - 32768) / 65536;
    if (y > INT32_MAX) return INT32_MAX;
    if (y < INT32_MIN) return INT32_MIN;
    return (int32_t)y;
}
static bool valid_cfg(const PI_Config *c) {
    return c != NULL && c->samplePeriodUs != 0U && c->samplePeriodUs <= 10000000U &&
           c->outputMin < c->outputMax && c->integratorMin <= c->integratorMax &&
           c->deadband >= 0 && c->action <= PI_ACTION_REVERSE &&
           c->antiWindup <= PI_ANTI_WINDUP_BACK_CALCULATION;
}
static bool recompute_ki(PI_Controller *p) {
    int64_t v = ((int64_t)p->config.kiQ16PerSecond * p->config.samplePeriodUs +
                 ((p->config.kiQ16PerSecond >= 0) ? 500000 : -500000)) / 1000000;
    if (v > INT32_MAX || v < INT32_MIN) return false;
    p->kiStepQ16 = (int32_t)v;
    return true;
}
bool PI_init(PI_Controller *p, const PI_Config *c) {
    if (p == NULL || !valid_cfg(c)) return false;
    p->config = *c;
    if (!recompute_ki(p)) return false;
    p->integratorQ16 = 0; p->previousOutput = c->outputMin;
    p->automatic = true; p->initialized = true;
    return true;
}
bool PI_reset(PI_Controller *p, int32_t output) {
    if (p == NULL || !p->initialized) return false;
    p->integratorQ16 = 0; p->previousOutput = (int32_t)clamp64(output, p->config.outputMin, p->config.outputMax);
    return true;
}
bool PI_setManualMode(PI_Controller *p, int32_t output) {
    if (p == NULL || !p->initialized) return false;
    p->automatic = false; p->previousOutput = (int32_t)clamp64(output, p->config.outputMin, p->config.outputMax); return true;
}
bool PI_setAutomaticMode(PI_Controller *p, int32_t sp, int32_t meas) {
    if (p == NULL || !p->initialized) return false;
    int64_t e = (int64_t)sp - meas; if (p->config.action == PI_ACTION_REVERSE) e = -e;
    if (e < p->config.deadband && e > -p->config.deadband) e = 0;
    int64_t prop = (int64_t)p->config.kpQ16 * e;
    p->integratorQ16 = ((int64_t)p->previousOutput - p->config.feedForward) * 65536 - prop;
    p->integratorQ16 = clamp64(p->integratorQ16, (int64_t)p->config.integratorMin * 65536, (int64_t)p->config.integratorMax * 65536);
    p->automatic = true; return true;
}
bool PI_setTunings(PI_Controller *p, int32_t kp, int32_t ki) { if (!p || !p->initialized) return false; p->config.kpQ16=kp; p->config.kiQ16PerSecond=ki; return recompute_ki(p); }
bool PI_setSamplePeriod(PI_Controller *p, uint32_t us) { if (!p || !p->initialized || us==0U || us>10000000U) return false; p->config.samplePeriodUs=us; return recompute_ki(p); }
bool PI_setOutputLimits(PI_Controller *p, int32_t lo, int32_t hi) { if (!p || !p->initialized || lo>=hi) return false; p->config.outputMin=lo; p->config.outputMax=hi; p->previousOutput=(int32_t)clamp64(p->previousOutput,lo,hi); return true; }
bool PI_setIntegratorLimits(PI_Controller *p, int32_t lo, int32_t hi) { if (!p || !p->initialized || lo>hi) return false; p->config.integratorMin=lo; p->config.integratorMax=hi; p->integratorQ16=clamp64(p->integratorQ16,(int64_t)lo*65536,(int64_t)hi*65536); return true; }
bool PI_setFeedForward(PI_Controller *p, int32_t ff) { if (!p || !p->initialized) return false; p->config.feedForward=ff; return true; }
bool PI_setDeadband(PI_Controller *p, int32_t db) { if (!p || !p->initialized || db<0) return false; p->config.deadband=db; return true; }
bool PI_setSlewLimits(PI_Controller *p, uint32_t rise, uint32_t fall) { if (!p || !p->initialized || rise > INT32_MAX || fall > INT32_MAX) return false; p->config.outputRiseLimit=rise; p->config.outputFallLimit=fall; return true; }
bool PI_update(PI_Controller *p, int32_t sp, int32_t meas, PI_Result *r) {
    if (p == NULL || !p->initialized) return false;
    PI_Result local = {0}; if (r == NULL) r = &local;
    r->setpoint=sp; r->measurement=meas; r->automatic=p->automatic; r->valid=true;
    if (!p->automatic) { r->output=p->previousOutput; r->integral=q16_to_i32(p->integratorQ16); return true; }
    int64_t e=(int64_t)sp-meas; if (p->config.action==PI_ACTION_REVERSE) e=-e;
    r->error=(int32_t)clamp64(e,INT32_MIN,INT32_MAX); r->effectiveError=(e<p->config.deadband && e>-p->config.deadband)?0:r->error;
    int64_t prop=(int64_t)p->config.kpQ16*r->effectiveError;
    int64_t oldI=p->integratorQ16, delta=(int64_t)p->kiStepQ16*r->effectiveError;
    int64_t candidate=clamp64(oldI+delta,(int64_t)p->config.integratorMin*65536,(int64_t)p->config.integratorMax*65536);
    int64_t unclampedQ=prop+candidate+(int64_t)p->config.feedForward*65536;
    int32_t unclamped=q16_to_i32(unclampedQ); int32_t limited=(int32_t)clamp64(unclamped,p->config.outputMin,p->config.outputMax);
    if (p->config.antiWindup==PI_ANTI_WINDUP_CONDITIONAL && limited!=unclamped) {
        bool pushing=(unclamped>p->config.outputMax && delta>0)||(unclamped<p->config.outputMin && delta<0); if (pushing) { candidate=oldI; unclampedQ=prop+candidate+(int64_t)p->config.feedForward*65536; unclamped=q16_to_i32(unclampedQ); limited=(int32_t)clamp64(unclamped,p->config.outputMin,p->config.outputMax); }
    }
    int32_t out=limited; bool slew=false;
    if (p->config.outputRiseLimit && (int64_t)out > (int64_t)p->previousOutput + p->config.outputRiseLimit) { out=(int32_t)((int64_t)p->previousOutput+p->config.outputRiseLimit); slew=true; }
    if (p->config.outputFallLimit && (int64_t)out < (int64_t)p->previousOutput - p->config.outputFallLimit) { out=(int32_t)((int64_t)p->previousOutput-p->config.outputFallLimit); slew=true; }
    if (p->config.antiWindup==PI_ANTI_WINDUP_BACK_CALCULATION) candidate += ((int64_t)(out-unclamped)*p->config.backCalculationGainQ16);
    p->integratorQ16=clamp64(candidate,(int64_t)p->config.integratorMin*65536,(int64_t)p->config.integratorMax*65536); p->previousOutput=out;
    r->proportional=q16_to_i32(prop); r->integral=q16_to_i32(p->integratorQ16); r->feedForward=p->config.feedForward; r->unclampedOutput=unclamped; r->output=out; r->outputLimited=(limited!=unclamped); r->slewLimited=slew; r->integratorLimited=(p->integratorQ16!=oldI+delta); return true;
}
int32_t PI_getOutput(const PI_Controller *p) { return (p && p->initialized)?p->previousOutput:0; }
int32_t PI_getIntegrator(const PI_Controller *p) { return (p && p->initialized)?q16_to_i32(p->integratorQ16):0; }
bool PI_isAutomatic(const PI_Controller *p) { return p && p->initialized && p->automatic; }
