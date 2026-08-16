#include "Algorithms/power_analysis/power_metrics/power_metrics.h"

#include <limits.h>

static void PowerMetrics_clearResult(PowerMetrics_Result *result)
{
    result->activePower = 0;
    result->dcPower = 0;
    result->acActivePower = 0;
    result->apparentPower = 0U;
    result->reactivePowerMagnitude = 0U;
    result->meanVoltage = 0;
    result->meanCurrent = 0;
    result->voltageRms = 0U;
    result->currentRms = 0U;
    result->voltageAcRms = 0U;
    result->currentAcRms = 0U;
    result->voltagePeakAbs = 0U;
    result->currentPeakAbs = 0U;
    result->powerFactorQ15 = 0;
    result->voltageCrestFactorQ15 = 0U;
    result->currentCrestFactorQ15 = 0U;
    result->sampleCount = 0U;
    result->calculationOverflow = false;
    result->valid = false;
}

static uint32_t PowerMetrics_absI32(int32_t value)
{
    return (value >= 0) ? (uint32_t) value :
        (uint32_t) (-(int64_t) value);
}

static uint64_t PowerMetrics_absI64(int64_t value)
{
    return (value >= 0) ? (uint64_t) value :
        (uint64_t) (-(value + 1)) + 1U;
}

static bool PowerMetrics_addI64(
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

static bool PowerMetrics_addU64(
    uint64_t first, uint64_t second, uint64_t *result)
{
    if (UINT64_MAX - first < second) {
        return false;
    }
    *result = first + second;
    return true;
}

static uint64_t PowerMetrics_divideU64Rounded(
    uint64_t numerator, uint64_t denominator)
{
    uint64_t result = numerator / denominator;
    uint64_t remainder = numerator % denominator;
    uint64_t halfUp = denominator / 2U + denominator % 2U;

    return (remainder >= halfUp) ? result + 1U : result;
}

static int64_t PowerMetrics_divideI64Rounded(
    int64_t numerator, uint64_t denominator)
{
    uint64_t magnitude = PowerMetrics_absI64(numerator);
    uint64_t rounded = PowerMetrics_divideU64Rounded(
        magnitude, denominator);

    if (numerator >= 0) {
        return (int64_t) rounded;
    }
    if (rounded == (UINT64_C(1) << 63)) {
        return INT64_MIN;
    }
    return -(int64_t) rounded;
}

static uint32_t PowerMetrics_integerSquareRoot(uint64_t value)
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

/* 计算0～1范围内的 Q15 比值，逐位生成小数，避免 numerator*32768 溢出。 */
static uint32_t PowerMetrics_fractionQ15(
    uint64_t numerator, uint64_t denominator)
{
    uint32_t result = 0U;
    uint8_t bit;
    uint64_t remainder;

    if (numerator >= denominator) {
        return 32768U;
    }
    remainder = numerator;

    for (bit = 0U; bit < 15U; bit++) {
        result <<= 1;
        if (remainder >= denominator - remainder) {
            remainder -= denominator - remainder;
            result |= 1U;
        } else {
            remainder <<= 1;
        }
    }

    if (remainder >= denominator / 2U + denominator % 2U) {
        result++;
    }
    return result;
}

static uint64_t PowerMetrics_scaleByQ15(
    uint64_t value, uint32_t factorQ15)
{
    uint64_t whole = value / 32768U;
    uint64_t remainder = value % 32768U;

    return whole * factorQ15 +
        (remainder * factorQ15 + 16384U) / 32768U;
}

static uint32_t PowerMetrics_ratioScaledSaturated(
    uint64_t numerator, uint64_t denominator, uint32_t scale)
{
    uint64_t whole = numerator / denominator;
    uint64_t remainder = numerator % denominator;
    uint64_t fractional;
    uint64_t result;

    if ((whole != 0U) && (whole > UINT32_MAX / scale)) {
        return UINT32_MAX;
    }

    while ((scale != 0U) &&
           (remainder > UINT64_MAX / scale)) {
        remainder = (remainder + 1U) >> 1;
        denominator = (denominator + 1U) >> 1;
    }
    fractional = (remainder * scale + denominator / 2U) /
        denominator;
    result = whole * scale + fractional;
    return (result > UINT32_MAX) ? UINT32_MAX : (uint32_t) result;
}

bool PowerMetrics_calculate(const int32_t *voltage,
    const int32_t *current, uint32_t sampleCount,
    PowerMetrics_Result *result)
{
    int64_t sumVoltage = 0;
    int64_t sumCurrent = 0;
    int64_t sumPower = 0;
    uint64_t sumVoltageSquares = 0U;
    uint64_t sumCurrentSquares = 0U;
    uint64_t sumVoltageAcSquares = 0U;
    uint64_t sumCurrentAcSquares = 0U;
    uint32_t voltagePeak = 0U;
    uint32_t currentPeak = 0U;
    uint32_t index;
    int64_t temporaryI64;
    uint64_t temporaryU64;
    uint32_t powerFactorMagnitudeQ15;
    uint64_t qFactorSquaredQ30;
    uint32_t qFactorQ15;

    if (result == 0) {
        return false;
    }
    PowerMetrics_clearResult(result);
    if ((voltage == 0) || (current == 0) || (sampleCount == 0U)) {
        return false;
    }
    result->sampleCount = sampleCount;

    for (index = 0U; index < sampleCount; index++) {
        uint32_t voltageAbs = PowerMetrics_absI32(voltage[index]);
        uint32_t currentAbs = PowerMetrics_absI32(current[index]);
        uint64_t voltageSquare =
            (uint64_t) voltageAbs * voltageAbs;
        uint64_t currentSquare =
            (uint64_t) currentAbs * currentAbs;
        int64_t instantaneousPower =
            (int64_t) voltage[index] * current[index];

        if (!PowerMetrics_addI64(
                sumVoltage, voltage[index], &sumVoltage) ||
            !PowerMetrics_addI64(
                sumCurrent, current[index], &sumCurrent) ||
            !PowerMetrics_addI64(
                sumPower, instantaneousPower, &sumPower) ||
            !PowerMetrics_addU64(
                sumVoltageSquares, voltageSquare,
                &sumVoltageSquares) ||
            !PowerMetrics_addU64(
                sumCurrentSquares, currentSquare,
                &sumCurrentSquares)) {
            result->calculationOverflow = true;
            return false;
        }

        if (voltageAbs > voltagePeak) {
            voltagePeak = voltageAbs;
        }
        if (currentAbs > currentPeak) {
            currentPeak = currentAbs;
        }
    }

    result->meanVoltage = (int32_t) PowerMetrics_divideI64Rounded(
        sumVoltage, sampleCount);
    result->meanCurrent = (int32_t) PowerMetrics_divideI64Rounded(
        sumCurrent, sampleCount);
    result->activePower = PowerMetrics_divideI64Rounded(
        sumPower, sampleCount);
    result->voltageRms = PowerMetrics_integerSquareRoot(
        PowerMetrics_divideU64Rounded(sumVoltageSquares, sampleCount));
    result->currentRms = PowerMetrics_integerSquareRoot(
        PowerMetrics_divideU64Rounded(sumCurrentSquares, sampleCount));
    result->voltagePeakAbs = voltagePeak;
    result->currentPeakAbs = currentPeak;

    for (index = 0U; index < sampleCount; index++) {
        int64_t voltageDifference =
            (int64_t) voltage[index] - result->meanVoltage;
        int64_t currentDifference =
            (int64_t) current[index] - result->meanCurrent;
        uint64_t voltageMagnitude =
            PowerMetrics_absI64(voltageDifference);
        uint64_t currentMagnitude =
            PowerMetrics_absI64(currentDifference);
        uint64_t voltageSquare = voltageMagnitude * voltageMagnitude;
        uint64_t currentSquare = currentMagnitude * currentMagnitude;

        if (!PowerMetrics_addU64(
                sumVoltageAcSquares, voltageSquare,
                &sumVoltageAcSquares) ||
            !PowerMetrics_addU64(
                sumCurrentAcSquares, currentSquare,
                &sumCurrentAcSquares)) {
            result->calculationOverflow = true;
            return false;
        }
    }

    result->voltageAcRms = PowerMetrics_integerSquareRoot(
        PowerMetrics_divideU64Rounded(
            sumVoltageAcSquares, sampleCount));
    result->currentAcRms = PowerMetrics_integerSquareRoot(
        PowerMetrics_divideU64Rounded(
            sumCurrentAcSquares, sampleCount));
    result->dcPower =
        (int64_t) result->meanVoltage * result->meanCurrent;
    if (!PowerMetrics_addI64(result->activePower,
            -result->dcPower, &temporaryI64)) {
        result->calculationOverflow = true;
        return false;
    }
    result->acActivePower = temporaryI64;
    result->apparentPower =
        (uint64_t) result->voltageRms * result->currentRms;

    if (result->apparentPower != 0U) {
        uint64_t activeMagnitude =
            PowerMetrics_absI64(result->activePower);

        powerFactorMagnitudeQ15 = PowerMetrics_fractionQ15(
            activeMagnitude, result->apparentPower);
        result->powerFactorQ15 = (result->activePower < 0) ?
            -(int32_t) powerFactorMagnitudeQ15 :
            (int32_t) powerFactorMagnitudeQ15;

        qFactorSquaredQ30 = UINT64_C(32768) * 32768U -
            (uint64_t) powerFactorMagnitudeQ15 *
            powerFactorMagnitudeQ15;
        qFactorQ15 = PowerMetrics_integerSquareRoot(
            qFactorSquaredQ30);
        result->reactivePowerMagnitude = PowerMetrics_scaleByQ15(
            result->apparentPower, qFactorQ15);
    }

    if (result->voltageRms != 0U) {
        temporaryU64 = ((uint64_t) voltagePeak * 32768U +
            result->voltageRms / 2U) / result->voltageRms;
        result->voltageCrestFactorQ15 =
            (temporaryU64 > UINT32_MAX) ?
            UINT32_MAX : (uint32_t) temporaryU64;
    }
    if (result->currentRms != 0U) {
        temporaryU64 = ((uint64_t) currentPeak * 32768U +
            result->currentRms / 2U) / result->currentRms;
        result->currentCrestFactorQ15 =
            (temporaryU64 > UINT32_MAX) ?
            UINT32_MAX : (uint32_t) temporaryU64;
    }

    result->valid = true;
    return true;
}

bool PowerMetrics_calculateEfficiency(int64_t inputActivePower,
    int64_t outputActivePower, uint32_t *efficiencyQ15,
    uint32_t *efficiencyMilliPercent)
{
    if ((inputActivePower <= 0) || (outputActivePower < 0) ||
        ((efficiencyQ15 == 0) && (efficiencyMilliPercent == 0))) {
        return false;
    }

    if (efficiencyQ15 != 0) {
        *efficiencyQ15 = PowerMetrics_ratioScaledSaturated(
            (uint64_t) outputActivePower,
            (uint64_t) inputActivePower, 32768U);
    }
    if (efficiencyMilliPercent != 0) {
        *efficiencyMilliPercent = PowerMetrics_ratioScaledSaturated(
            (uint64_t) outputActivePower,
            (uint64_t) inputActivePower, 100000U);
    }
    return true;
}
