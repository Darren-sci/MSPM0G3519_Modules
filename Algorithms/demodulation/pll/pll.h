#ifndef ALGORITHMS_DEMODULATION_PLL_PLL_H_
#define ALGORITHMS_DEMODULATION_PLL_PLL_H_

#include <stdbool.h>
#include <stdint.h>

/** 数字 PLL 配置。频率相关参数均使用 mHz。 */
typedef struct {
    uint32_t sampleRateHz;
    uint64_t initialFrequencyMilliHz;
    uint64_t minimumFrequencyMilliHz;
    uint64_t maximumFrequencyMilliHz;
    uint64_t proportionalGainMilliHz;
    uint64_t integralGainMilliHzPerSample;
    uint16_t detectorFilterQ15;
    uint16_t minimumInputAmplitudeQ15;
    uint16_t lockErrorThresholdQ15;
    uint32_t lockSampleCount;
} PLL_Config;

/** 每处理一个 ADC 采样返回的参考信号和跟踪状态。 */
typedef struct {
    uint64_t frequencyMilliHz;
    uint32_t phaseWord;
    int32_t filteredPhaseErrorQ15;
    uint32_t filteredAmplitudeQ15;
    int16_t sineQ15;
    int16_t cosineQ15;
    bool locked;
} PLL_Output;

/** PLL 运行状态。初始化后不要直接修改成员。 */
typedef struct {
    uint32_t sampleRateHz;
    uint32_t phaseWord;
    uint32_t frequencyWord;
    uint32_t initialFrequencyWord;
    uint32_t minimumFrequencyWord;
    uint32_t maximumFrequencyWord;
    uint32_t proportionalGainWord;
    uint32_t integralGainWord;
    int32_t filteredPhaseErrorQ15;
    uint32_t filteredAmplitudeQ15;
    uint32_t lockCounter;
    uint32_t lockSampleCount;
    uint16_t detectorFilterQ15;
    uint16_t minimumInputAmplitudeQ15;
    uint16_t lockErrorThresholdQ15;
    bool locked;
    bool initialized;
} PLL_State;

/** 初始化用于实数 Q15 ADC 正弦跟踪的数字 PLL。 */
bool PLL_init(PLL_State *state, const PLL_Config *config);

/**
 * 清除环路历史并恢复初始频率。initialPhaseWord 的完整 uint32_t 范围
 * 对应0～一整周；0对应输出正弦的上升过零点。
 */
bool PLL_reset(PLL_State *state, uint32_t initialPhaseWord);

/**
 * 处理一个已经去直流的 Q15 ADC 采样。
 * 输出正弦与输入同相锁定，余弦比正弦超前90度。
 */
bool PLL_processSample(PLL_State *state,
    int16_t inputQ15, PLL_Output *output);

/**
 * 连续处理一块 Q15 ADC 数据。sineOutput、cosineOutput 和 lastOutput 可以
 * 分别为空，但三者不能同时为空；两个参考输出数组不能是同一数组。
 * sampleCount 可以为0。
 */
bool PLL_processBlock(PLL_State *state,
    const int16_t *inputQ15,
    int16_t *sineOutput, int16_t *cosineOutput,
    uint32_t sampleCount, PLL_Output *lastOutput);

/** 返回当前中心频率，单位 mHz；无效状态返回0。 */
uint64_t PLL_getFrequencyMilliHz(const PLL_State *state);

/** 返回当前相位累加器；无效状态返回0。 */
uint32_t PLL_getPhaseWord(const PLL_State *state);

/** 返回当前锁定标志；无效状态返回 false。 */
bool PLL_isLocked(const PLL_State *state);

/** 根据32位相位产生 Q15 正弦，可单独用于 DAC/NCO。 */
int16_t PLL_phaseToSineQ15(uint32_t phaseWord);

/** 根据32位相位产生 Q15 余弦，可单独用于 DAC/NCO。 */
int16_t PLL_phaseToCosineQ15(uint32_t phaseWord);

#endif
