#include "Algorithms/preprocessing/window_functions/flat_top/flat_top.h"

#include <limits.h>

/*
 * 本模块实现标准五项对称 Flat-top 窗，目标是降低正弦幅值对 FFT 频点偏移
 * 的敏感度。它适合精确幅值测量，但主瓣明显宽于 Hann 窗，不适合分辨
 * 距离很近的频率，也不应默认用于谐波间隔很窄的分析。
 *
 * Flat-top 窗的边缘系数会出现小负值，这是标准窗形的一部分，不是定点
 * 溢出或生成错误。相干增益约为 0.216，加窗后的 FFT 幅值必须使用实例
 * 中实际计算的 coherentGainQ15 修正；功率和噪声使用 powerGainQ30。
 *
 * FlatTop_init() 使用 31 次迭代的 Q30 CORDIC 生成到四倍角的余弦，计算
 * 量高于 Hann 初始化，因此应在采样开始前生成一次，不要每个 DMA 块重复
 * 调用。FlatTop_apply() 支持原地处理，但会覆盖原始时域数据。
 */

#define FLAT_TOP_Q30_ONE       (1073741824LL)
#define FLAT_TOP_Q45_SCALE     (35184372088832LL)
#define FLAT_TOP_Q45_ROUNDING  (FLAT_TOP_Q45_SCALE / 2LL)

/* 标准五项 Flat-top 系数，均使用 Q30。 */
#define FLAT_TOP_A0_Q30        (231476135LL)
#define FLAT_TOP_A1_Q30        (447354753LL)
#define FLAT_TOP_A2_Q30        (297709049LL)
#define FLAT_TOP_A3_Q30        (89742211LL)
#define FLAT_TOP_A4_Q30        (7459680LL)

/* atan(2^-i) 的二进制角度值，一整圈对应 2^32。 */
static const uint32_t gFlatTopCordicAngles[31] = {
    0x20000000U, 0x12E4051EU, 0x09FB385BU, 0x051111D4U,
    0x028B0D43U, 0x0145D7E1U, 0x00A2F61EU, 0x00517C55U,
    0x0028BE53U, 0x00145F2FU, 0x000A2F98U, 0x000517CCU,
    0x00028BE6U, 0x000145F3U, 0x0000A2FAU, 0x0000517DU,
    0x000028BEU, 0x0000145FU, 0x00000A30U, 0x00000518U,
    0x0000028CU, 0x00000146U, 0x000000A3U, 0x00000051U,
    0x00000029U, 0x00000014U, 0x0000000AU, 0x00000005U,
    0x00000003U, 0x00000001U, 0x00000001U
};

static bool FlatTop_isValid(const FlatTop_Window *window)
{
    return (window != 0) &&
           (window->coefficients != 0) &&
           (window->length >= 5U);
}

static int64_t FlatTop_shiftRight(int64_t value, uint8_t shift)
{
    if (value >= 0) {
        return value >> shift;
    }
    return -((-value) >> shift);
}

static int32_t FlatTop_cosineFirstQuadrantQ30(uint32_t angle)
{
    int64_t x = 652032874LL;
    int64_t y = 0;
    int64_t remainingAngle = angle;
    uint8_t iteration;

    /* Q30 CORDIC 避免 Cortex-M0+ 上的软件浮点三角函数。 */
    for (iteration = 0U; iteration < 31U; iteration++) {
        int64_t xShift = FlatTop_shiftRight(x, iteration);
        int64_t yShift = FlatTop_shiftRight(y, iteration);
        int64_t previousX = x;

        if (remainingAngle >= 0) {
            x -= yShift;
            y += xShift;
            remainingAngle -= gFlatTopCordicAngles[iteration];
        } else {
            x += yShift;
            y -= FlatTop_shiftRight(previousX, iteration);
            remainingAngle += gFlatTopCordicAngles[iteration];
        }
    }

    if (x > FLAT_TOP_Q30_ONE) {
        x = FLAT_TOP_Q30_ONE;
    } else if (x < 0) {
        x = 0;
    }
    return (int32_t) x;
}

static int32_t FlatTop_cosineQ30(uint32_t phase)
{
    uint8_t quadrant = (uint8_t) (phase >> 30);
    uint32_t offset = phase & 0x3FFFFFFFU;
    uint32_t reducedAngle;
    int32_t cosine;

    if ((quadrant == 0U) || (quadrant == 2U)) {
        reducedAngle = offset;
    } else {
        reducedAngle = 0x40000000U - offset;
    }

    cosine = FlatTop_cosineFirstQuadrantQ30(reducedAngle);
    return ((quadrant == 1U) || (quadrant == 2U)) ?
        -cosine : cosine;
}

static int16_t FlatTop_saturateQ15(int64_t value)
{
    int64_t rounded;

    if (value >= 0) {
        rounded = (value + FLAT_TOP_Q45_ROUNDING) /
            FLAT_TOP_Q45_SCALE;
    } else {
        rounded = -((-value + FLAT_TOP_Q45_ROUNDING) /
            FLAT_TOP_Q45_SCALE);
    }

    if (rounded > INT16_MAX) {
        return INT16_MAX;
    }
    if (rounded < INT16_MIN) {
        return INT16_MIN;
    }
    return (int16_t) rounded;
}

static int16_t FlatTop_coefficientQ15(uint32_t phase)
{
    int64_t valueQ60 = FLAT_TOP_A0_Q30 * FLAT_TOP_Q30_ONE;

    valueQ60 -= FLAT_TOP_A1_Q30 * FlatTop_cosineQ30(phase);
    valueQ60 += FLAT_TOP_A2_Q30 * FlatTop_cosineQ30(phase * 2U);
    valueQ60 -= FLAT_TOP_A3_Q30 * FlatTop_cosineQ30(phase * 3U);
    valueQ60 += FLAT_TOP_A4_Q30 * FlatTop_cosineQ30(phase * 4U);

    return FlatTop_saturateQ15(valueQ60);
}

static int16_t FlatTop_multiplyQ15(
    int16_t sample, int16_t coefficient)
{
    int64_t product = (int32_t) sample * coefficient;
    int64_t rounded;

    if (product >= 0) {
        rounded = (product + 16384) / 32768;
    } else {
        rounded = -((-product + 16384) / 32768);
    }

    if (rounded > INT16_MAX) {
        return INT16_MAX;
    }
    if (rounded < INT16_MIN) {
        return INT16_MIN;
    }
    return (int16_t) rounded;
}

bool FlatTop_init(FlatTop_Window *window,
    int16_t *coefficients, uint16_t length)
{
    uint16_t index;
    uint16_t halfLength;
    int64_t coefficientSum = 0;
    uint64_t squareSum = 0U;
    int64_t coherentGain;

    if ((window == 0) || (coefficients == 0) || (length < 5U)) {
        return false;
    }

    /* 只生成前半窗并镜像，确保定点舍入后仍严格左右对称。 */
    halfLength = (uint16_t) ((length + 1U) / 2U);
    for (index = 0U; index < halfLength; index++) {
        uint32_t phase = (uint32_t)
            (((uint64_t) index << 32) / (length - 1U));
        int16_t coefficient = FlatTop_coefficientQ15(phase);

        coefficients[index] = coefficient;
        coefficients[length - 1U - index] = coefficient;
    }

    for (index = 0U; index < length; index++) {
        int32_t coefficient = coefficients[index];
        coefficientSum += coefficient;
        squareSum += (uint64_t) (coefficient * coefficient);
    }

    coherentGain = (coefficientSum + length / 2U) / length;
    if (coherentGain < 0) {
        coherentGain = 0;
    } else if (coherentGain > INT16_MAX) {
        coherentGain = INT16_MAX;
    }

    window->coefficients = coefficients;
    window->length = length;
    /* 增益由实际量化系数统计，不能用理论常数替代。 */
    window->coherentGainQ15 = (uint16_t) coherentGain;
    window->powerGainQ30 = (uint32_t)
        ((squareSum + length / 2U) / length);
    return true;
}

bool FlatTop_apply(const FlatTop_Window *window,
    const int16_t *input, int16_t *output)
{
    uint16_t index;

    if (!FlatTop_isValid(window) || (input == 0) || (output == 0)) {
        return false;
    }

    /* input 与 output 可以相同；需要原波形时必须在加窗前保留副本。 */
    for (index = 0U; index < window->length; index++) {
        output[index] = FlatTop_multiplyQ15(
            input[index], window->coefficients[index]);
    }
    return true;
}

uint16_t FlatTop_getLength(const FlatTop_Window *window)
{
    return (window != 0) ? window->length : 0U;
}

uint16_t FlatTop_getCoherentGainQ15(const FlatTop_Window *window)
{
    return FlatTop_isValid(window) ? window->coherentGainQ15 : 0U;
}

uint32_t FlatTop_getPowerGainQ30(const FlatTop_Window *window)
{
    return FlatTop_isValid(window) ? window->powerGainQ30 : 0U;
}
