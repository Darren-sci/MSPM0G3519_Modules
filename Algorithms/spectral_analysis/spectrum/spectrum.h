#ifndef ALGORITHMS_SPECTRAL_ANALYSIS_SPECTRUM_SPECTRUM_H_
#define ALGORITHMS_SPECTRAL_ANALYSIS_SPECTRUM_SPECTRUM_H_

#include <stdbool.h>
#include <stdint.h>

#include "Algorithms/transforms/fft/fft.h"

/** 未加窗时使用的单位相干增益，允许精确表示 1.0。 */
#define SPECTRUM_UNITY_GAIN_Q15    (32768U)

/** 把 FFT 长度、采样率和窗相干增益绑定在一起。 */
typedef struct {
    uint32_t sampleRateHz;
    uint32_t coherentGainQ15;
    uint16_t fftLength;
    uint16_t realBinCount;
    bool initialized;
} Spectrum_Config;

/**
 * 初始化频谱配置。
 *
 * fftPlan 必须已经初始化。未加窗时 coherentGainQ15 传
 * SPECTRUM_UNITY_GAIN_Q15；Hann 或 Flat-top 则传对应窗口对象返回的
 * 实际相干增益。sampleRateHz 必须大于 0。
 */
bool Spectrum_init(Spectrum_Config *config,
    const FFT_PlanQ15 *fftPlan, uint32_t sampleRateHz,
    uint32_t coherentGainQ15);

/** 返回实数输入的单边频点数 N/2+1；无效配置返回 0。 */
uint16_t Spectrum_getBinCount(const Spectrum_Config *config);

/** 返回指定频点对应的频率，单位为 mHz；无效频点返回 0。 */
uint64_t Spectrum_binToFrequencyMilliHz(
    const Spectrum_Config *config, uint16_t bin);

/** 把频率换算到最近频点；超过奈奎斯特频率时返回最后一个频点。 */
uint16_t Spectrum_frequencyMilliHzToNearestBin(
    const Spectrum_Config *config, uint64_t frequencyMilliHz);

/** 返回一个复数频点的实部平方与虚部平方之和。 */
uint32_t Spectrum_magnitudeSquaredQ30(FFT_ComplexQ15 value);

/** 返回一个复数频点的模值，单位仍为输入 Q15 码值。 */
uint32_t Spectrum_magnitudeQ15(FFT_ComplexQ15 value);

/**
 * 计算一个频点经过单边谱系数和窗相干增益补偿后的幅值。
 *
 * spectrum 至少包含 fftLength 个复数。直流和奈奎斯特点不乘 2，其他
 * 单边频点乘 2。结果用 uint32_t 保存 Q15 码值，允许补偿后超过 32767，
 * 便于调用者发现削顶或错误的增益配置。
 */
bool Spectrum_getAmplitudeQ15(const Spectrum_Config *config,
    const FFT_ComplexQ15 *spectrum, uint16_t bin,
    uint32_t *amplitudeQ15);

/**
 * 生成完整单边幅值谱。outputCapacity 至少为 Spectrum_getBinCount()；
 * input 与 output 不能重叠。
 */
bool Spectrum_buildAmplitudeQ15(const Spectrum_Config *config,
    const FFT_ComplexQ15 *spectrum,
    uint32_t *output, uint16_t outputCapacity);

#endif
