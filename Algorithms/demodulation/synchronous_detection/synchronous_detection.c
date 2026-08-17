#include "Algorithms/demodulation/synchronous_detection/synchronous_detection.h"

#include <limits.h>

static const uint32_t gSynchronousDetectionCordicAngles[31] = {
    0x20000000U, 0x12E4051EU, 0x09FB385BU, 0x051111D4U,
    0x028B0D43U, 0x0145D7E1U, 0x00A2F61EU, 0x00517C55U,
    0x0028BE53U, 0x00145F2FU, 0x000A2F98U, 0x000517CCU,
    0x00028BE6U, 0x000145F3U, 0x0000A2FAU, 0x0000517DU,
    0x000028BEU, 0x0000145FU, 0x00000A30U, 0x00000518U,
    0x0000028CU, 0x00000146U, 0x000000A3U, 0x00000051U,
    0x00000029U, 0x00000014U, 0x0000000AU, 0x00000005U,
    0x00000003U, 0x00000001U, 0x00000001U
};

static void SynchronousDetection_clearResult(
    SynchronousDetection_Result *result)
{
    result->inPhase = 0;
    result->quadrature = 0;
    result->amplitude = 0U;
    result->phaseMilliDegrees = 0;
    result->processedSampleCount = 0U;
    result->saturated = false;
    result->stable = false;
    result->valid = false;
}

static uint64_t SynchronousDetection_absI64(int64_t value)
{
    return (value >= 0) ? (uint64_t) value :
        (uint64_t) (-(value + 1)) + 1U;
}

static int64_t SynchronousDetection_shiftRight(
    int64_t value, uint8_t shift)
{
    if (value >= 0) {
        return value >> shift;
    }
    return -((-value) >> shift);
}

static bool SynchronousDetection_addI64(
    int64_t first, int64_t second, int64_t *result)
{
    if ((second > 0) && (first > INT64_MAX - second)) {
        return false;
    }
    if ((second < 0) && (first < INT64_MIN - second)) {
        return false;
    }
    *result = first + second;
    return true;
}

static int64_t SynchronousDetection_divideI64Rounded(
    int64_t numerator, uint64_t denominator)
{
    uint64_t magnitude = SynchronousDetection_absI64(numerator);
    uint64_t quotient = magnitude / denominator;
    uint64_t remainder = magnitude % denominator;

    if (remainder >= denominator / 2U + denominator % 2U) {
        quotient++;
    }
    if (numerator >= 0) {
        return (int64_t) quotient;
    }
    if (quotient == (UINT64_C(1) << 63)) {
        return INT64_MIN;
    }
    return -(int64_t) quotient;
}

static int32_t SynchronousDetection_saturateI32(
    int64_t value, bool *saturated)
{
    if (value > INT32_MAX) {
        *saturated = true;
        return INT32_MAX;
    }
    if (value < INT32_MIN) {
        *saturated = true;
        return INT32_MIN;
    }
    return (int32_t) value;
}

static uint32_t SynchronousDetection_integerSquareRoot(uint64_t value)
{
    uint64_t result = 0U;
    uint64_t bit = UINT64_C(1) << 62;

    while (bit > value) {
        bit >>= 2;
    }
    while (bit != 0U) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    if (value > result) {
        result++;
    }
    return (result > UINT32_MAX) ? UINT32_MAX : (uint32_t) result;
}

static void SynchronousDetection_finishResult(
    int64_t inPhase, int64_t quadrature,
    uint32_t sampleCount, bool stable,
    SynchronousDetection_Result *result)
{
    uint64_t magnitudeSquared;

    result->inPhase = SynchronousDetection_saturateI32(
        inPhase, &result->saturated);
    result->quadrature = SynchronousDetection_saturateI32(
        quadrature, &result->saturated);
    magnitudeSquared =
        (uint64_t) SynchronousDetection_absI64(result->inPhase) *
            SynchronousDetection_absI64(result->inPhase) +
        (uint64_t) SynchronousDetection_absI64(result->quadrature) *
            SynchronousDetection_absI64(result->quadrature);
    result->amplitude =
        SynchronousDetection_integerSquareRoot(magnitudeSquared);
    result->phaseMilliDegrees =
        SynchronousDetection_atan2MilliDegrees(
            result->quadrature, result->inPhase);
    result->processedSampleCount = sampleCount;
    result->stable = stable && !result->saturated;
    result->valid = true;
}

int32_t SynchronousDetection_atan2MilliDegrees(
    int64_t y, int64_t x)
{
    uint64_t maximum;
    int64_t angle = 0;
    uint8_t iteration;

    if ((x == 0) && (y == 0)) {
        return 0;
    }
    if (x == 0) {
        return (y > 0) ? 90000 : -90000;
    }

    maximum = SynchronousDetection_absI64(x);
    if (SynchronousDetection_absI64(y) > maximum) {
        maximum = SynchronousDetection_absI64(y);
    }
    while (maximum > (UINT64_C(1) << 30)) {
        x = SynchronousDetection_shiftRight(x, 1U);
        y = SynchronousDetection_shiftRight(y, 1U);
        maximum >>= 1;
    }
    while ((maximum != 0U) && (maximum < (UINT64_C(1) << 28))) {
        x *= 2;
        y *= 2;
        maximum <<= 1;
    }

    if (x < 0) {
        angle = (y >= 0) ?
            (INT64_C(1) << 31) : -(INT64_C(1) << 31);
        x = -x;
        y = -y;
    }

    for (iteration = 0U; iteration < 31U; iteration++) {
        int64_t xShift =
            SynchronousDetection_shiftRight(x, iteration);
        int64_t yShift =
            SynchronousDetection_shiftRight(y, iteration);
        int64_t oldX = x;

        if (y > 0) {
            x = oldX + yShift;
            y -= xShift;
            angle += gSynchronousDetectionCordicAngles[iteration];
        } else if (y < 0) {
            x = oldX - yShift;
            y += xShift;
            angle -= gSynchronousDetectionCordicAngles[iteration];
        } else {
            break;
        }
    }

    if (angle >= 0) {
        return (int32_t) ((angle * 360000 +
            (INT64_C(1) << 31)) / (INT64_C(1) << 32));
    }
    return (int32_t) -(((-angle) * 360000 +
        (INT64_C(1) << 31)) / (INT64_C(1) << 32));
}

bool SynchronousDetection_calculateBlock(
    const int32_t *input,
    const int16_t *referenceSineQ15,
    const int16_t *referenceCosineQ15,
    uint32_t sampleCount,
    SynchronousDetection_Result *result)
{
    int64_t inPhaseSum = 0;
    int64_t quadratureSum = 0;
    uint64_t denominator;
    uint32_t index;

    if (result == 0) {
        return false;
    }
    SynchronousDetection_clearResult(result);
    if ((input == 0) || (referenceSineQ15 == 0) ||
        (referenceCosineQ15 == 0) || (sampleCount == 0U)) {
        return false;
    }

    for (index = 0U; index < sampleCount; index++) {
        int64_t inPhaseProduct =
            (int64_t) input[index] * referenceSineQ15[index];
        int64_t quadratureProduct =
            (int64_t) input[index] * referenceCosineQ15[index];

        if (!SynchronousDetection_addI64(
                inPhaseSum, inPhaseProduct, &inPhaseSum) ||
            !SynchronousDetection_addI64(
                quadratureSum, quadratureProduct,
                &quadratureSum)) {
            result->saturated = true;
            return false;
        }
    }

    /* 2/(N*32768) 等价于 1/(N*16384)，恢复正弦峰值。 */
    denominator = (uint64_t) sampleCount * 16384U;
    SynchronousDetection_finishResult(
        SynchronousDetection_divideI64Rounded(
            inPhaseSum, denominator),
        SynchronousDetection_divideI64Rounded(
            quadratureSum, denominator),
        sampleCount, true, result);
    return true;
}

bool SynchronousDetection_init(SynchronousDetection_State *state,
    uint16_t filterCoefficientQ15,
    uint32_t minimumAmplitude,
    uint32_t settlingSampleCount)
{
    if ((state == 0) || (filterCoefficientQ15 == 0U) ||
        (filterCoefficientQ15 > 32768U) ||
        (settlingSampleCount == 0U)) {
        return false;
    }

    state->filterCoefficientQ15 = filterCoefficientQ15;
    state->minimumAmplitude = minimumAmplitude;
    state->settlingSampleCount = settlingSampleCount;
    state->initialized = true;
    return SynchronousDetection_reset(state);
}

bool SynchronousDetection_reset(SynchronousDetection_State *state)
{
    if ((state == 0) || !state->initialized) {
        return false;
    }
    state->filteredInPhase = 0;
    state->filteredQuadrature = 0;
    state->processedSampleCount = 0U;
    return true;
}

bool SynchronousDetection_processSample(
    SynchronousDetection_State *state,
    int32_t input,
    int16_t referenceSineQ15,
    int16_t referenceCosineQ15,
    SynchronousDetection_Result *result)
{
    int64_t inPhaseTarget;
    int64_t quadratureTarget;
    int64_t inPhaseDifference;
    int64_t quadratureDifference;
    int64_t inPhaseCorrection;
    int64_t quadratureCorrection;
    bool stable;

    if ((state == 0) || !state->initialized || (result == 0)) {
        return false;
    }
    SynchronousDetection_clearResult(result);

    inPhaseTarget = SynchronousDetection_divideI64Rounded(
        (int64_t) input * referenceSineQ15, 16384U);
    quadratureTarget = SynchronousDetection_divideI64Rounded(
        (int64_t) input * referenceCosineQ15, 16384U);
    inPhaseDifference = inPhaseTarget - state->filteredInPhase;
    quadratureDifference =
        quadratureTarget - state->filteredQuadrature;
    inPhaseCorrection = SynchronousDetection_divideI64Rounded(
        inPhaseDifference * state->filterCoefficientQ15, 32768U);
    quadratureCorrection = SynchronousDetection_divideI64Rounded(
        quadratureDifference * state->filterCoefficientQ15, 32768U);
    state->filteredInPhase += inPhaseCorrection;
    state->filteredQuadrature += quadratureCorrection;
    if (state->processedSampleCount != UINT32_MAX) {
        state->processedSampleCount++;
    }

    stable = state->processedSampleCount >= state->settlingSampleCount;
    SynchronousDetection_finishResult(
        state->filteredInPhase,
        state->filteredQuadrature,
        state->processedSampleCount,
        stable, result);
    if (result->amplitude < state->minimumAmplitude) {
        result->stable = false;
    }
    return true;
}

bool SynchronousDetection_processBlock(
    SynchronousDetection_State *state,
    const int32_t *input,
    const int16_t *referenceSineQ15,
    const int16_t *referenceCosineQ15,
    uint32_t sampleCount,
    SynchronousDetection_Result *lastResult)
{
    uint32_t index;

    if ((state == 0) || !state->initialized ||
        (lastResult == 0)) {
        return false;
    }
    if (sampleCount == 0U) {
        SynchronousDetection_clearResult(lastResult);
        return true;
    }
    if ((input == 0) || (referenceSineQ15 == 0) ||
        (referenceCosineQ15 == 0)) {
        return false;
    }

    for (index = 0U; index < sampleCount; index++) {
        if (!SynchronousDetection_processSample(
                state, input[index],
                referenceSineQ15[index],
                referenceCosineQ15[index], lastResult)) {
            return false;
        }
    }
    return true;
}
