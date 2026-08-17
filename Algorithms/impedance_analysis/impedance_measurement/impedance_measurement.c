#include "Algorithms/impedance_analysis/impedance_measurement/impedance_measurement.h"

#include "Algorithms/demodulation/pll/pll.h"

#include <limits.h>

#define IMPEDANCE_INV_TWO_PI_SCALED       UINT64_C(159154943)
#define IMPEDANCE_CAPACITANCE_CONSTANT    UINT64_C(159154943091895335)

static uint64_t ImpedanceMeasurement_absI64(int64_t value)
{
    return (value >= 0) ? (uint64_t) value :
        (uint64_t) (-(value + 1)) + 1U;
}

static int32_t ImpedanceMeasurement_shiftRightI32(
    int32_t value, uint8_t shift)
{
    if (value >= 0) {
        return value >> shift;
    }
    return -(int32_t) ((uint32_t) (-(int64_t) value) >> shift);
}

static bool ImpedanceMeasurement_signedRatioScaled(
    int64_t numerator, uint64_t denominator,
    uint32_t scale, int64_t *result)
{
    uint64_t magnitude = ImpedanceMeasurement_absI64(numerator);
    uint64_t whole = magnitude / denominator;
    uint64_t remainder = magnitude % denominator;
    uint64_t fractional;
    uint64_t scaled;

    if (whole > (uint64_t) INT64_MAX / scale) {
        return false;
    }
    while (remainder >
           (UINT64_MAX - denominator / 2U) / scale) {
        remainder = (remainder + 1U) >> 1;
        denominator = (denominator + 1U) >> 1;
    }
    fractional =
        (remainder * scale + denominator / 2U) / denominator;
    scaled = whole * scale;
    if (scaled > (uint64_t) INT64_MAX - fractional) {
        return false;
    }
    scaled += fractional;
    *result = (numerator < 0) ?
        -(int64_t) scaled : (int64_t) scaled;
    return true;
}

static bool ImpedanceMeasurement_unsignedRatioScaled(
    uint64_t numerator, uint64_t denominator,
    uint32_t scale, uint64_t *result)
{
    uint64_t whole = numerator / denominator;
    uint64_t remainder = numerator % denominator;
    uint64_t fractional;

    if (whole > UINT64_MAX / scale) {
        return false;
    }
    while (remainder >
           (UINT64_MAX - denominator / 2U) / scale) {
        remainder = (remainder + 1U) >> 1;
        denominator = (denominator + 1U) >> 1;
    }
    fractional =
        (remainder * scale + denominator / 2U) / denominator;
    *result = whole * scale;
    if (*result > UINT64_MAX - fractional) {
        return false;
    }
    *result += fractional;
    return true;
}

bool ImpedanceMeasurement_calculate(
    const SynchronousDetection_Result *voltage,
    const SynchronousDetection_Result *current,
    uint64_t frequencyMilliHz,
    int32_t phaseCorrectionMilliDegrees,
    uint32_t phaseThresholdMilliDegrees,
    ImpedanceMeasurement_Result *result)
{
    int32_t voltageI;
    int32_t voltageQ;
    int32_t currentI;
    int32_t currentQ;
    uint32_t maximum;
    uint8_t shift = 0U;
    int64_t dot;
    int64_t cross;
    uint64_t denominator;

    if (result == 0) {
        return false;
    }
    result->frequencyMilliHz = 0U;
    result->magnitudeMilliOhms = 0U;
    result->resistanceMilliOhms = 0;
    result->reactanceMilliOhms = 0;
    result->phaseMilliDegrees = 0;
    result->nature = IMPEDANCE_NATURE_UNKNOWN;
    result->valid = false;

    if ((voltage == 0) || (current == 0) ||
        !voltage->valid || !current->valid ||
        voltage->saturated || current->saturated ||
        (current->amplitude == 0U) || (frequencyMilliHz == 0U) ||
        (phaseThresholdMilliDegrees > 180000U)) {
        return false;
    }

    voltageI = voltage->inPhase;
    voltageQ = voltage->quadrature;
    currentI = current->inPhase;
    currentQ = current->quadrature;
    maximum = (uint32_t) ImpedanceMeasurement_absI64(voltageI);
    if (ImpedanceMeasurement_absI64(voltageQ) > maximum) {
        maximum = (uint32_t) ImpedanceMeasurement_absI64(voltageQ);
    }
    if (ImpedanceMeasurement_absI64(currentI) > maximum) {
        maximum = (uint32_t) ImpedanceMeasurement_absI64(currentI);
    }
    if (ImpedanceMeasurement_absI64(currentQ) > maximum) {
        maximum = (uint32_t) ImpedanceMeasurement_absI64(currentQ);
    }
    while (maximum > (UINT32_C(1) << 30)) {
        maximum >>= 1;
        shift++;
    }
    voltageI = ImpedanceMeasurement_shiftRightI32(voltageI, shift);
    voltageQ = ImpedanceMeasurement_shiftRightI32(voltageQ, shift);
    currentI = ImpedanceMeasurement_shiftRightI32(currentI, shift);
    currentQ = ImpedanceMeasurement_shiftRightI32(currentQ, shift);

    dot = (int64_t) voltageI * currentI +
          (int64_t) voltageQ * currentQ;
    cross = (int64_t) voltageQ * currentI -
            (int64_t) voltageI * currentQ;
    denominator = (uint64_t) ImpedanceMeasurement_absI64(currentI) *
            ImpedanceMeasurement_absI64(currentI) +
        (uint64_t) ImpedanceMeasurement_absI64(currentQ) *
            ImpedanceMeasurement_absI64(currentQ);
    if (denominator == 0U) {
        return false;
    }

    if (!ImpedanceMeasurement_unsignedRatioScaled(
            voltage->amplitude, current->amplitude,
            1000U, &result->magnitudeMilliOhms) ||
        !ImpedanceMeasurement_signedRatioScaled(
            dot, denominator, 1000U,
            &result->resistanceMilliOhms) ||
        !ImpedanceMeasurement_signedRatioScaled(
            cross, denominator, 1000U,
            &result->reactanceMilliOhms)) {
        return false;
    }

    if (phaseCorrectionMilliDegrees != 0) {
        int64_t phaseWordSigned =
            ((int64_t) phaseCorrectionMilliDegrees *
             (INT64_C(1) << 32)) / 360000;
        int32_t correctionCosine =
            PLL_phaseToCosineQ15((uint32_t) phaseWordSigned);
        int32_t correctionSine =
            PLL_phaseToSineQ15((uint32_t) phaseWordSigned);
        int64_t resistance = result->resistanceMilliOhms;
        int64_t reactance = result->reactanceMilliOhms;
        int64_t rotatedResistance;
        int64_t rotatedReactance;

        if ((ImpedanceMeasurement_absI64(resistance) >
                (uint64_t) INT64_MAX / 65536U) ||
            (ImpedanceMeasurement_absI64(reactance) >
                (uint64_t) INT64_MAX / 65536U)) {
            return false;
        }
        rotatedResistance =
            resistance * correctionCosine -
            reactance * correctionSine;
        rotatedReactance =
            resistance * correctionSine +
            reactance * correctionCosine;
        result->resistanceMilliOhms =
            (rotatedResistance >= 0) ?
            (rotatedResistance + 16384) / 32768 :
            -((-rotatedResistance + 16384) / 32768);
        result->reactanceMilliOhms =
            (rotatedReactance >= 0) ?
            (rotatedReactance + 16384) / 32768 :
            -((-rotatedReactance + 16384) / 32768);
        dot = result->resistanceMilliOhms;
        cross = result->reactanceMilliOhms;
    }

    result->phaseMilliDegrees =
        SynchronousDetection_atan2MilliDegrees(cross, dot);
    result->frequencyMilliHz = frequencyMilliHz;
    if ((uint32_t) ((result->phaseMilliDegrees >= 0) ?
            result->phaseMilliDegrees : -result->phaseMilliDegrees) <=
        phaseThresholdMilliDegrees) {
        result->nature = IMPEDANCE_NATURE_RESISTIVE;
    } else if (result->phaseMilliDegrees > 0) {
        result->nature = IMPEDANCE_NATURE_INDUCTIVE;
    } else {
        result->nature = IMPEDANCE_NATURE_CAPACITIVE;
    }
    result->valid = true;
    return true;
}

bool ImpedanceMeasurement_estimateInductanceMicroHenry(
    const ImpedanceMeasurement_Result *result,
    uint64_t *inductanceMicroHenry)
{
    uint64_t scaled;

    if ((result == 0) || (inductanceMicroHenry == 0) ||
        !result->valid || (result->frequencyMilliHz == 0U) ||
        (result->reactanceMilliOhms <= 0)) {
        return false;
    }
    if (!ImpedanceMeasurement_unsignedRatioScaled(
            (uint64_t) result->reactanceMilliOhms,
            result->frequencyMilliHz,
            (uint32_t) IMPEDANCE_INV_TWO_PI_SCALED,
            &scaled)) {
        return false;
    }
    *inductanceMicroHenry = (scaled + 500U) / 1000U;
    return true;
}

bool ImpedanceMeasurement_estimateCapacitancePicoFarad(
    const ImpedanceMeasurement_Result *result,
    uint64_t *capacitancePicoFarad)
{
    uint64_t reactance;
    uint64_t denominator;

    if ((result == 0) || (capacitancePicoFarad == 0) ||
        !result->valid || (result->frequencyMilliHz == 0U) ||
        (result->reactanceMilliOhms >= 0)) {
        return false;
    }
    reactance = ImpedanceMeasurement_absI64(
        result->reactanceMilliOhms);
    if ((reactance == 0U) ||
        (result->frequencyMilliHz > UINT64_MAX / reactance)) {
        return false;
    }
    denominator = result->frequencyMilliHz * reactance;
    *capacitancePicoFarad =
        IMPEDANCE_CAPACITANCE_CONSTANT / denominator;
    if ((IMPEDANCE_CAPACITANCE_CONSTANT % denominator) >=
        denominator / 2U + denominator % 2U) {
        (*capacitancePicoFarad)++;
    }
    return true;
}
