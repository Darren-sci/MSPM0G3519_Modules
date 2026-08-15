#ifndef ALGORITHMS_PREPROCESSING_ADC_CALIBRATION_ADC_CALIBRATION_H_
#define ALGORITHMS_PREPROCESSING_ADC_CALIBRATION_ADC_CALIBRATION_H_

#include <stdbool.h>
#include <stdint.h>

/**
 * ADC 三点分段线性校准参数。
 *
 * lowCode、zeroCode、highCode 是递增的 ADC 标定码。zeroCode 固定映射为
 * 0；lowValue 和 highValue 决定低端、高端输出，可使用 Q15、mV、mA
 * 或其他整数单位。低端和高端分别计算增益，因此允许零点两侧不对称。
 */
typedef struct {
    uint16_t lowCode;
    uint16_t zeroCode;
    uint16_t highCode;
    int32_t lowValue;
    int32_t highValue;
} ADCCalibration_Config;

/**
 * 初始化通用三点校准。
 *
 * 必须满足 lowCode <= zeroCode < highCode。当 lowCode == zeroCode 时，
 * 低端退化为单极性钳位区，所有不高于 zeroCode 的输入都输出 0。
 */
bool ADCCalibration_init(ADCCalibration_Config *config,
    uint16_t lowCode, uint16_t zeroCode, uint16_t highCode,
    int32_t lowValue, int32_t highValue);

/** 初始化单极性 Q15 映射：zeroCode 对应 0，fullScaleCode 对应 32767。 */
bool ADCCalibration_initUnipolarQ15(ADCCalibration_Config *config,
    uint16_t zeroCode, uint16_t fullScaleCode);

/**
 * 初始化双极性 Q15 映射：negativeFullScaleCode 对应 -32768，zeroCode
 * 对应 0，positiveFullScaleCode 对应 32767。
 */
bool ADCCalibration_initBipolarQ15(ADCCalibration_Config *config,
    uint16_t negativeFullScaleCode, uint16_t zeroCode,
    uint16_t positiveFullScaleCode);

/**
 * 校准一个 ADC 原始码并返回 int32_t 结果。
 *
 * 标定范围外的输入钳位到相应端点值；无效配置返回 0。
 */
int32_t ADCCalibration_apply(
    const ADCCalibration_Config *config, uint16_t rawCode);

/** 校准一个 ADC 原始码，并把结果饱和到 int16_t 范围。 */
int16_t ADCCalibration_applyToInt16(
    const ADCCalibration_Config *config, uint16_t rawCode);

/** 将一块 ADC 原始码校准为 int32_t；sampleCount 可以为 0。 */
bool ADCCalibration_applyBlock(const ADCCalibration_Config *config,
    const uint16_t *input, int32_t *output, uint32_t sampleCount);

/** 将一块 ADC 原始码校准并饱和为 int16_t；sampleCount 可以为 0。 */
bool ADCCalibration_applyBlockToInt16(
    const ADCCalibration_Config *config,
    const uint16_t *input, int16_t *output, uint32_t sampleCount);

#endif
