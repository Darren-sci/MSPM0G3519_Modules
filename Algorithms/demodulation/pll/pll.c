#include "Algorithms/demodulation/pll/pll.h"

#include <limits.h>

/* 0～90度、步进1/256圈的 Q15 正弦表；其余象限由对称性得到。 */
static const int16_t gPLLQuarterSineQ15[65] = {
    0, 804, 1608, 2410, 3212, 4011, 4808, 5602,
    6393, 7179, 7962, 8739, 9512, 10278, 11039, 11793,
    12539, 13279, 14010, 14732, 15446, 16151, 16846, 17530,
    18204, 18868, 19519, 20159, 20787, 21403, 22005, 22594,
    23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790,
    27245, 27683, 28105, 28510, 28898, 29268, 29621, 29956,
    30273, 30571, 30852, 31113, 31356, 31580, 31785, 31971,
    32137, 32285, 32412, 32521, 32609, 32678, 32728, 32757,
    32767
};

static bool PLL_isStateValid(const PLL_State *state)
{
    return (state != 0) && state->initialized &&
           (state->sampleRateHz != 0U) &&
           (state->minimumFrequencyWord <= state->frequencyWord) &&
           (state->frequencyWord <= state->maximumFrequencyWord) &&
           (state->detectorFilterQ15 != 0U) &&
           (state->detectorFilterQ15 <= 32768U) &&
           (state->lockSampleCount != 0U);
}

static int16_t PLL_tableValue(uint8_t index)
{
    uint8_t quadrant = (uint8_t) (index >> 6);
    uint8_t offset = (uint8_t) (index & 0x3FU);

    switch (quadrant) {
        case 0U:
            return gPLLQuarterSineQ15[offset];
        case 1U:
            return gPLLQuarterSineQ15[64U - offset];
        case 2U:
            return (int16_t) -gPLLQuarterSineQ15[offset];
        default:
            return (int16_t) -gPLLQuarterSineQ15[64U - offset];
    }
}

static uint32_t PLL_ratioToPhaseWord(
    uint64_t numerator, uint64_t denominator)
{
    uint64_t remainder = numerator;
    uint32_t result = 0U;
    uint8_t bit;

    if (numerator >= denominator) {
        return UINT32_MAX;
    }

    for (bit = 0U; bit < 32U; bit++) {
        result <<= 1;
        if (remainder >= denominator - remainder) {
            remainder -= denominator - remainder;
            result |= 1U;
        } else {
            remainder <<= 1;
        }
    }

    if ((remainder >= denominator / 2U + denominator % 2U) &&
        (result != UINT32_MAX)) {
        result++;
    }
    return result;
}

static uint32_t PLL_frequencyMilliHzToWord(
    uint32_t sampleRateHz, uint64_t frequencyMilliHz)
{
    uint64_t denominator = (uint64_t) sampleRateHz * 1000U;

    return PLL_ratioToPhaseWord(frequencyMilliHz, denominator);
}

static int32_t PLL_multiplyQ15Rounded(int64_t value)
{
    if (value >= 0) {
        return (int32_t) ((value + 16384) / 32768);
    }
    return (int32_t) -((-value + 16384) / 32768);
}

static int32_t PLL_filterSignedQ15(
    int32_t previous, int32_t input, uint16_t coefficientQ15)
{
    int32_t difference = input - previous;

    return previous + PLL_multiplyQ15Rounded(
        (int64_t) difference * coefficientQ15);
}

static uint32_t PLL_filterUnsignedQ15(
    uint32_t previous, uint32_t input, uint16_t coefficientQ15)
{
    int32_t difference = (int32_t) input - (int32_t) previous;
    int32_t updated = (int32_t) previous + PLL_multiplyQ15Rounded(
        (int64_t) difference * coefficientQ15);

    return (updated < 0) ? 0U : (uint32_t) updated;
}

static uint32_t PLL_absQ15(int16_t value)
{
    return (value >= 0) ? (uint32_t) value :
        (uint32_t) (-(int32_t) value);
}

int16_t PLL_phaseToSineQ15(uint32_t phaseWord)
{
    uint8_t index = (uint8_t) (phaseWord >> 24);
    uint8_t nextIndex = (uint8_t) (index + 1U);
    uint16_t fraction = (uint16_t) (phaseWord >> 8);
    int32_t first = PLL_tableValue(index);
    int32_t second = PLL_tableValue(nextIndex);
    int32_t difference = second - first;
    int32_t interpolated = first + PLL_multiplyQ15Rounded(
        (int64_t) difference * (fraction >> 1));

    return (int16_t) interpolated;
}

int16_t PLL_phaseToCosineQ15(uint32_t phaseWord)
{
    return PLL_phaseToSineQ15(phaseWord + UINT32_C(0x40000000));
}

bool PLL_init(PLL_State *state, const PLL_Config *config)
{
    uint64_t nyquistMilliHz;

    if (state == 0) {
        return false;
    }

    state->initialized = false;
    if ((config == 0) || (config->sampleRateHz == 0U)) {
        return false;
    }

    nyquistMilliHz = (uint64_t) config->sampleRateHz * 500U;
    if ((config->minimumFrequencyMilliHz >
            config->initialFrequencyMilliHz) ||
        (config->initialFrequencyMilliHz >
            config->maximumFrequencyMilliHz) ||
        (config->maximumFrequencyMilliHz >= nyquistMilliHz) ||
        (config->proportionalGainMilliHz >= nyquistMilliHz) ||
        (config->integralGainMilliHzPerSample >= nyquistMilliHz) ||
        (config->detectorFilterQ15 == 0U) ||
        (config->detectorFilterQ15 > 32768U) ||
        (config->minimumInputAmplitudeQ15 > 32768U) ||
        (config->lockErrorThresholdQ15 > 32768U) ||
        (config->lockSampleCount == 0U)) {
        return false;
    }

    state->sampleRateHz = config->sampleRateHz;
    state->initialFrequencyWord = PLL_frequencyMilliHzToWord(
        config->sampleRateHz, config->initialFrequencyMilliHz);
    state->minimumFrequencyWord = PLL_frequencyMilliHzToWord(
        config->sampleRateHz, config->minimumFrequencyMilliHz);
    state->maximumFrequencyWord = PLL_frequencyMilliHzToWord(
        config->sampleRateHz, config->maximumFrequencyMilliHz);
    state->proportionalGainWord = PLL_frequencyMilliHzToWord(
        config->sampleRateHz, config->proportionalGainMilliHz);
    state->integralGainWord = PLL_frequencyMilliHzToWord(
        config->sampleRateHz, config->integralGainMilliHzPerSample);
    state->detectorFilterQ15 = config->detectorFilterQ15;
    state->minimumInputAmplitudeQ15 =
        config->minimumInputAmplitudeQ15;
    state->lockErrorThresholdQ15 = config->lockErrorThresholdQ15;
    state->lockSampleCount = config->lockSampleCount;
    state->initialized = true;
    return PLL_reset(state, 0U);
}

bool PLL_reset(PLL_State *state, uint32_t initialPhaseWord)
{
    if ((state == 0) || !state->initialized) {
        return false;
    }

    state->phaseWord = initialPhaseWord;
    state->frequencyWord = state->initialFrequencyWord;
    state->filteredPhaseErrorQ15 = 0;
    state->filteredAmplitudeQ15 = 0U;
    state->lockCounter = 0U;
    state->locked = false;
    return true;
}

uint64_t PLL_getFrequencyMilliHz(const PLL_State *state)
{
    uint64_t product;
    uint64_t wholeHz;
    uint64_t fractionalWord;

    if (!PLL_isStateValid(state)) {
        return 0U;
    }

    product = (uint64_t) state->frequencyWord * state->sampleRateHz;
    wholeHz = product >> 32;
    fractionalWord = product & UINT32_MAX;
    return wholeHz * 1000U +
        (fractionalWord * 1000U + (UINT64_C(1) << 31)) /
        (UINT64_C(1) << 32);
}

uint32_t PLL_getPhaseWord(const PLL_State *state)
{
    return PLL_isStateValid(state) ? state->phaseWord : 0U;
}

bool PLL_isLocked(const PLL_State *state)
{
    return PLL_isStateValid(state) && state->locked;
}

bool PLL_processSample(PLL_State *state,
    int16_t inputQ15, PLL_Output *output)
{
    uint32_t referencePhase;
    int16_t sine;
    int16_t cosine;
    int32_t rawPhaseError;
    int64_t integralDelta;
    int64_t proportionalCorrection;
    int64_t newFrequency;
    int64_t phaseStep;
    uint32_t absoluteFilteredError;

    if (!PLL_isStateValid(state) || (output == 0)) {
        return false;
    }

    referencePhase = state->phaseWord;
    sine = PLL_phaseToSineQ15(referencePhase);
    cosine = PLL_phaseToCosineQ15(referencePhase);

    /* 输入按 sine(phase) 定义；乘以正交 cosine 后低通得到相位误差。 */
    rawPhaseError = PLL_multiplyQ15Rounded(
        (int64_t) inputQ15 * cosine);
    state->filteredPhaseErrorQ15 = PLL_filterSignedQ15(
        state->filteredPhaseErrorQ15,
        rawPhaseError, state->detectorFilterQ15);
    state->filteredAmplitudeQ15 = PLL_filterUnsignedQ15(
        state->filteredAmplitudeQ15,
        PLL_absQ15(inputQ15), state->detectorFilterQ15);

    integralDelta = PLL_multiplyQ15Rounded(
        (int64_t) state->filteredPhaseErrorQ15 *
        state->integralGainWord);
    newFrequency = (int64_t) state->frequencyWord + integralDelta;
    if (newFrequency < state->minimumFrequencyWord) {
        newFrequency = state->minimumFrequencyWord;
    } else if (newFrequency > state->maximumFrequencyWord) {
        newFrequency = state->maximumFrequencyWord;
    }
    state->frequencyWord = (uint32_t) newFrequency;

    proportionalCorrection = PLL_multiplyQ15Rounded(
        (int64_t) state->filteredPhaseErrorQ15 *
        state->proportionalGainWord);
    phaseStep = newFrequency + proportionalCorrection;
    if (phaseStep < 0) {
        phaseStep = 0;
    } else if (phaseStep > UINT32_MAX) {
        phaseStep = UINT32_MAX;
    }
    state->phaseWord += (uint32_t) phaseStep;

    absoluteFilteredError =
        (state->filteredPhaseErrorQ15 >= 0) ?
        (uint32_t) state->filteredPhaseErrorQ15 :
        (uint32_t) -state->filteredPhaseErrorQ15;
    if ((state->filteredAmplitudeQ15 >=
            state->minimumInputAmplitudeQ15) &&
        (absoluteFilteredError <= state->lockErrorThresholdQ15)) {
        if (state->lockCounter < state->lockSampleCount) {
            state->lockCounter++;
        }
        if (state->lockCounter >= state->lockSampleCount) {
            state->locked = true;
        }
    } else {
        state->lockCounter = 0U;
        state->locked = false;
    }

    output->frequencyMilliHz = PLL_getFrequencyMilliHz(state);
    output->phaseWord = referencePhase;
    output->filteredPhaseErrorQ15 =
        state->filteredPhaseErrorQ15;
    output->filteredAmplitudeQ15 = state->filteredAmplitudeQ15;
    output->sineQ15 = sine;
    output->cosineQ15 = cosine;
    output->locked = state->locked;
    return true;
}

bool PLL_processBlock(PLL_State *state,
    const int16_t *inputQ15,
    int16_t *sineOutput, int16_t *cosineOutput,
    uint32_t sampleCount, PLL_Output *lastOutput)
{
    PLL_Output currentOutput;
    uint32_t index;

    if (!PLL_isStateValid(state) ||
        ((sineOutput == 0) && (cosineOutput == 0) &&
         (lastOutput == 0)) ||
        ((sineOutput != 0) && (sineOutput == cosineOutput))) {
        return false;
    }
    if (sampleCount == 0U) {
        return true;
    }
    if (inputQ15 == 0) {
        return false;
    }

    for (index = 0U; index < sampleCount; index++) {
        if (!PLL_processSample(state, inputQ15[index], &currentOutput)) {
            return false;
        }
        if (sineOutput != 0) {
            sineOutput[index] = currentOutput.sineQ15;
        }
        if (cosineOutput != 0) {
            cosineOutput[index] = currentOutput.cosineQ15;
        }
    }

    if (lastOutput != 0) {
        *lastOutput = currentOutput;
    }
    return true;
}
