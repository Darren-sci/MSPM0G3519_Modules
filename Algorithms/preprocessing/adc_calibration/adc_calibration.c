#include "Algorithms/preprocessing/adc_calibration/adc_calibration.h"

#include <limits.h>

static bool ADCCalibration_isValid(
    const ADCCalibration_Config *config)
{
    return (config != 0) &&
           (config->lowCode <= config->zeroCode) &&
           (config->zeroCode < config->highCode);
}

static int64_t ADCCalibration_divideRounded(
    int64_t numerator, uint32_t denominator)
{
    int64_t halfDenominator = (int64_t) denominator / 2;

    if (numerator >= 0) {
        return (numerator + halfDenominator) / denominator;
    }
    return -((-numerator + halfDenominator) / denominator);
}

static int32_t ADCCalibration_interpolate(
    uint16_t code, uint16_t code0, uint16_t code1,
    int32_t value0, int32_t value1)
{
    uint32_t codeOffset = (uint32_t) code - code0;
    uint32_t codeSpan = (uint32_t) code1 - code0;
    int64_t valueSpan = (int64_t) value1 - value0;
    int64_t adjustment = ADCCalibration_divideRounded(
        valueSpan * codeOffset, codeSpan);

    return (int32_t) ((int64_t) value0 + adjustment);
}

bool ADCCalibration_init(ADCCalibration_Config *config,
    uint16_t lowCode, uint16_t zeroCode, uint16_t highCode,
    int32_t lowValue, int32_t highValue)
{
    if ((config == 0) || (lowCode > zeroCode) ||
        (zeroCode >= highCode)) {
        return false;
    }

    config->lowCode = lowCode;
    config->zeroCode = zeroCode;
    config->highCode = highCode;
    config->lowValue = lowValue;
    config->highValue = highValue;
    return true;
}

bool ADCCalibration_initUnipolarQ15(ADCCalibration_Config *config,
    uint16_t zeroCode, uint16_t fullScaleCode)
{
    return ADCCalibration_init(config,
        zeroCode, zeroCode, fullScaleCode, 0, INT16_MAX);
}

bool ADCCalibration_initBipolarQ15(ADCCalibration_Config *config,
    uint16_t negativeFullScaleCode, uint16_t zeroCode,
    uint16_t positiveFullScaleCode)
{
    return ADCCalibration_init(config,
        negativeFullScaleCode, zeroCode, positiveFullScaleCode,
        INT16_MIN, INT16_MAX);
}

int32_t ADCCalibration_apply(
    const ADCCalibration_Config *config, uint16_t rawCode)
{
    if (!ADCCalibration_isValid(config)) {
        return 0;
    }

    if (rawCode <= config->lowCode) {
        return config->lowValue;
    }
    if (rawCode >= config->highCode) {
        return config->highValue;
    }
    if (rawCode == config->zeroCode) {
        return 0;
    }

    if (rawCode < config->zeroCode) {
        if (config->lowCode == config->zeroCode) {
            return 0;
        }
        return ADCCalibration_interpolate(rawCode,
            config->lowCode, config->zeroCode,
            config->lowValue, 0);
    }

    return ADCCalibration_interpolate(rawCode,
        config->zeroCode, config->highCode,
        0, config->highValue);
}

int16_t ADCCalibration_applyToInt16(
    const ADCCalibration_Config *config, uint16_t rawCode)
{
    int32_t value = ADCCalibration_apply(config, rawCode);

    if (value > INT16_MAX) {
        return INT16_MAX;
    }
    if (value < INT16_MIN) {
        return INT16_MIN;
    }
    return (int16_t) value;
}

bool ADCCalibration_applyBlock(const ADCCalibration_Config *config,
    const uint16_t *input, int32_t *output, uint32_t sampleCount)
{
    uint32_t index;

    if (!ADCCalibration_isValid(config)) {
        return false;
    }
    if (sampleCount == 0U) {
        return true;
    }
    if ((input == 0) || (output == 0)) {
        return false;
    }

    for (index = 0U; index < sampleCount; index++) {
        output[index] = ADCCalibration_apply(config, input[index]);
    }
    return true;
}

bool ADCCalibration_applyBlockToInt16(
    const ADCCalibration_Config *config,
    const uint16_t *input, int16_t *output, uint32_t sampleCount)
{
    uint32_t index;

    if (!ADCCalibration_isValid(config)) {
        return false;
    }
    if (sampleCount == 0U) {
        return true;
    }
    if ((input == 0) || (output == 0)) {
        return false;
    }

    for (index = 0U; index < sampleCount; index++) {
        output[index] =
            ADCCalibration_applyToInt16(config, input[index]);
    }
    return true;
}
