#include "Algorithms/preprocessing/window_functions/hann/hann.h"

/*
 * 本模块生成首尾严格为零的对称 Hann 窗，并以 Q15 形式应用到时域数据。
 * 这种窗适合一般频谱、基波、谐波和 THD 分析，但会使相干正弦幅值约
 * 缩小一半，同时拓宽频谱主瓣。FFT 幅值和功率计算必须分别使用本模块
 * 返回的 coherentGainQ15 和 powerGainQ30 修正，不能把加窗后的 FFT 码值
 * 直接当作真实幅值。
 *
 * Hann_init() 应在采样开始前调用一次，不要在每个 DMA 数据块中重复生成
 * 系数。窗口长度必须与实际数据块和 FFT 长度完全一致，调用者提供的系数
 * 缓冲区在窗口使用期间必须保持有效。
 *
 * Hann_apply() 支持原地处理，但会覆盖原始时域数据。峰峰值、普通 RMS、
 * 上升时间和原始波形显示应在加窗前完成，或者使用另一份数据副本。
 */

/* 0～90 度四分之一正弦表，数值范围为 Q15 的 0～32767。 */
static const uint16_t gHannSineQuarter[65] = {
       0,   804,  1608,  2410,  3212,  4011,  4808,  5602,
    6393,  7179,  7962,  8739,  9512, 10278, 11039, 11793,
   12539, 13279, 14010, 14732, 15446, 16151, 16846, 17530,
   18204, 18868, 19519, 20159, 20787, 21403, 22005, 22594,
   23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790,
   27245, 27683, 28105, 28510, 28898, 29268, 29621, 29956,
   30273, 30571, 30852, 31113, 31356, 31580, 31785, 31971,
   32137, 32285, 32412, 32521, 32609, 32678, 32728, 32757,
   32767
};

static bool Hann_isValid(const Hann_Window *window)
{
    return (window != 0) &&
           (window->coefficients != 0) &&
           (window->length >= 3U);
}

static int16_t Hann_sineAtIndex(uint8_t index)
{
    uint8_t quadrant = index >> 6;
    uint8_t offset = index & 0x3FU;
    uint16_t magnitude;

    if ((quadrant == 1U) || (quadrant == 3U)) {
        offset = (uint8_t) (64U - offset);
    }
    magnitude = gHannSineQuarter[offset];

    return (quadrant >= 2U) ?
        (int16_t) -(int32_t) magnitude : (int16_t) magnitude;
}

static int16_t Hann_sineQ15(uint32_t phase)
{
    uint8_t index = (uint8_t) (phase >> 24);
    uint8_t nextIndex = (uint8_t) (index + 1U);
    uint16_t fraction = (uint16_t) ((phase & 0x00FFFFFFU) >> 8);
    int32_t first = Hann_sineAtIndex(index);
    int32_t second = Hann_sineAtIndex(nextIndex);
    int32_t difference = second - first;
    int32_t interpolation = difference * fraction;

    /*
     * 小表配合线性插值可支持任意窗口长度，避免运行时浮点三角函数。
     * 表和插值存在很小的定点误差，但左右系数会在初始化时直接镜像，
     * 因此最终窗口仍保持严格对称。
     */
    if (interpolation >= 0) {
        interpolation = (interpolation + 32768) / 65536;
    } else {
        interpolation = -((-interpolation + 32768) / 65536);
    }
    return (int16_t) (first + interpolation);
}

static int16_t Hann_coefficientQ15(uint32_t phase)
{
    int32_t cosine = Hann_sineQ15(phase + 0x40000000U);
    return (int16_t) ((32767 - cosine + 1) / 2);
}

static int16_t Hann_multiplyQ15(int16_t sample, int16_t coefficient)
{
    int32_t product = (int32_t) sample * coefficient;

    if (product >= 0) {
        return (int16_t) ((product + 16384) / 32768);
    }
    return (int16_t) -((-product + 16384) / 32768);
}

bool Hann_init(Hann_Window *window,
    int16_t *coefficients, uint16_t length)
{
    uint16_t index;
    uint16_t halfLength;
    int64_t coefficientSum = 0;
    uint64_t squareSum = 0U;

    if ((window == 0) || (coefficients == 0) || (length < 3U)) {
        return false;
    }

    /* 只生成前半窗并镜像，既减少运算，也消除两侧独立舍入的差异。 */
    halfLength = (uint16_t) ((length + 1U) / 2U);
    for (index = 0U; index < halfLength; index++) {
        uint32_t phase = (uint32_t)
            (((uint64_t) index << 32) / (length - 1U));
        int16_t coefficient = Hann_coefficientQ15(phase);

        coefficients[index] = coefficient;
        coefficients[length - 1U - index] = coefficient;
    }

    for (index = 0U; index < length; index++) {
        int32_t coefficient = coefficients[index];
        coefficientSum += coefficient;
        squareSum += (uint64_t) (coefficient * coefficient);
    }

    window->coefficients = coefficients;
    window->length = length;
    /*
     * 不把理论增益写死：对称 Hann 窗的实际平均值与长度有关，Q15 系数
     * 量化也会带来细小变化。使用实际生成系数统计出的增益更可靠。
     */
    window->coherentGainQ15 = (uint16_t)
        ((coefficientSum + length / 2U) / length);
    window->powerGainQ30 = (uint32_t)
        ((squareSum + length / 2U) / length);
    return true;
}

bool Hann_apply(const Hann_Window *window,
    const int16_t *input, int16_t *output)
{
    uint16_t index;

    if (!Hann_isValid(window) || (input == 0) || (output == 0)) {
        return false;
    }

    /* input 与 output 可相同；每个位置在覆盖前只读取一次，因此原地安全。 */
    for (index = 0U; index < window->length; index++) {
        output[index] = Hann_multiplyQ15(
            input[index], window->coefficients[index]);
    }
    return true;
}

uint16_t Hann_getLength(const Hann_Window *window)
{
    return (window != 0) ? window->length : 0U;
}

uint16_t Hann_getCoherentGainQ15(const Hann_Window *window)
{
    return Hann_isValid(window) ? window->coherentGainQ15 : 0U;
}

uint32_t Hann_getPowerGainQ30(const Hann_Window *window)
{
    return Hann_isValid(window) ? window->powerGainQ30 : 0U;
}
