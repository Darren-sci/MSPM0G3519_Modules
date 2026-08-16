#include "Algorithms/transforms/fft/fft.h"

#include <limits.h>
#include <stddef.h>

/*
 * 本模块实现原地基 2 按时间抽取正向 FFT。输入、旋转因子和输出均为 Q15。
 * 每一级蝶形固定除以 2，N 点变换最终累计右移 log2(N) 位，因此输出等于
 * 数学 DFT/N。后续幅值模块必须结合 scaleShift、单边谱系数和窗相干增益
 * 统一换算，不能直接把复数谱码值当作真实幅值。
 *
 * FFT_init() 使用 Q30 CORDIC 生成旋转因子，只应在上电或改变长度时调用。
 * FFT_execute() 会先进行位倒序，再覆盖输入工作区。需要原时域数据时必须
 * 提前保存。计划中的旋转因子是只读的，可供四个 ADC 通道顺序复用。
 *
 * 固定逐级缩放对实数 ADC 输入通常安全。任意满量程复数输入仍可能在旋转
 * 和蝶形中超出 Q15；代码会饱和并累计 saturationCount。只要该计数非零，
 * 就应降低输入幅度或在 FFT 前额外右移，而不是继续相信幅值结果。
 */

#define FFT_Q30_ONE    (1073741824LL)

/* atan(2^-i) 的二进制角度值，一整圈对应 2^32。 */
static const uint32_t gFFTCordicAngles[31] = {
    0x20000000U, 0x12E4051EU, 0x09FB385BU, 0x051111D4U,
    0x028B0D43U, 0x0145D7E1U, 0x00A2F61EU, 0x00517C55U,
    0x0028BE53U, 0x00145F2FU, 0x000A2F98U, 0x000517CCU,
    0x00028BE6U, 0x000145F3U, 0x0000A2FAU, 0x0000517DU,
    0x000028BEU, 0x0000145FU, 0x00000A30U, 0x00000518U,
    0x0000028CU, 0x00000146U, 0x000000A3U, 0x00000051U,
    0x00000029U, 0x00000014U, 0x0000000AU, 0x00000005U,
    0x00000003U, 0x00000001U, 0x00000001U
};

static bool FFT_isPlanValid(const FFT_PlanQ15 *plan)
{
    return (plan != 0) && plan->initialized &&
           FFT_isLengthSupported(plan->length) &&
           (plan->twiddles != 0) &&
           (plan->twiddleCount >= plan->length / 2U);
}

static bool FFT_rangesOverlap(const void *first, size_t firstSize,
    const void *second, size_t secondSize)
{
    uintptr_t firstStart = (uintptr_t) first;
    uintptr_t secondStart = (uintptr_t) second;

    return (firstStart < secondStart + secondSize) &&
           (secondStart < firstStart + firstSize);
}

static int64_t FFT_shiftRight(int64_t value, uint8_t shift)
{
    if (value >= 0) {
        return value >> shift;
    }
    return -((-value) >> shift);
}

static void FFT_cordicFirstQuadrantQ30(
    uint32_t angle, int32_t *cosine, int32_t *sine)
{
    int64_t x = 652032874LL;
    int64_t y = 0;
    int64_t remainingAngle = angle;
    uint8_t iteration;

    for (iteration = 0U; iteration < 31U; iteration++) {
        int64_t xShift = FFT_shiftRight(x, iteration);
        int64_t yShift = FFT_shiftRight(y, iteration);

        if (remainingAngle >= 0) {
            x -= yShift;
            y += xShift;
            remainingAngle -= gFFTCordicAngles[iteration];
        } else {
            x += yShift;
            y -= xShift;
            remainingAngle += gFFTCordicAngles[iteration];
        }
    }

    if (x < 0) {
        x = 0;
    } else if (x > FFT_Q30_ONE) {
        x = FFT_Q30_ONE;
    }
    if (y < 0) {
        y = 0;
    } else if (y > FFT_Q30_ONE) {
        y = FFT_Q30_ONE;
    }
    *cosine = (int32_t) x;
    *sine = (int32_t) y;
}

static void FFT_sineCosineQ30(
    uint32_t phase, int32_t *cosine, int32_t *sine)
{
    uint8_t quadrant;
    uint32_t offset;
    int32_t firstCosine;
    int32_t firstSine;

    if (phase == 0U) {
        *cosine = (int32_t) FFT_Q30_ONE;
        *sine = 0;
        return;
    }
    if (phase == 0x40000000U) {
        *cosine = 0;
        *sine = (int32_t) FFT_Q30_ONE;
        return;
    }
    if (phase == 0x80000000U) {
        *cosine = (int32_t) -FFT_Q30_ONE;
        *sine = 0;
        return;
    }
    if (phase == 0xC0000000U) {
        *cosine = 0;
        *sine = (int32_t) -FFT_Q30_ONE;
        return;
    }

    quadrant = (uint8_t) (phase >> 30);
    offset = phase & 0x3FFFFFFFU;
    FFT_cordicFirstQuadrantQ30(
        offset, &firstCosine, &firstSine);

    switch (quadrant) {
        case 0U:
            *cosine = firstCosine;
            *sine = firstSine;
            break;
        case 1U:
            *cosine = -firstSine;
            *sine = firstCosine;
            break;
        case 2U:
            *cosine = -firstCosine;
            *sine = -firstSine;
            break;
        default:
            *cosine = firstSine;
            *sine = -firstCosine;
            break;
    }
}

static int16_t FFT_q30ToQ15(int32_t value)
{
    int64_t rounded;

    if (value >= 0) {
        rounded = ((int64_t) value + 16384) / 32768;
    } else {
        rounded = -((-(int64_t) value + 16384) / 32768);
    }

    if (rounded > INT16_MAX) {
        return INT16_MAX;
    }
    if (rounded < INT16_MIN) {
        return INT16_MIN;
    }
    return (int16_t) rounded;
}

static uint16_t FFT_reverseBits(uint16_t value, uint8_t bitCount)
{
    uint16_t reversed = 0U;
    uint8_t bit;

    for (bit = 0U; bit < bitCount; bit++) {
        reversed = (uint16_t) ((reversed << 1) | (value & 1U));
        value >>= 1;
    }
    return reversed;
}

static int32_t FFT_q30ProductToQ15(int64_t value)
{
    if (value >= 0) {
        return (int32_t) ((value + 16384) / 32768);
    }
    return (int32_t) -((-value + 16384) / 32768);
}

static int32_t FFT_halfRounded(int32_t value)
{
    if (value >= 0) {
        return (value + 1) / 2;
    }
    return -((-value + 1) / 2);
}

static int16_t FFT_saturateQ15(
    int32_t value, uint32_t *saturationCount)
{
    if (value > INT16_MAX) {
        if (*saturationCount != UINT32_MAX) {
            (*saturationCount)++;
        }
        return INT16_MAX;
    }
    if (value < INT16_MIN) {
        if (*saturationCount != UINT32_MAX) {
            (*saturationCount)++;
        }
        return INT16_MIN;
    }
    return (int16_t) value;
}

bool FFT_isLengthSupported(uint16_t length)
{
    return (length >= FFT_MIN_LENGTH) &&
           (length <= FFT_MAX_LENGTH) &&
           ((length & (length - 1U)) == 0U);
}

uint16_t FFT_getRequiredTwiddleCount(uint16_t length)
{
    return FFT_isLengthSupported(length) ? (uint16_t) (length / 2U) : 0U;
}

bool FFT_init(FFT_PlanQ15 *plan, uint16_t length,
    FFT_ComplexQ15 *twiddleBuffer, uint16_t twiddleCapacity)
{
    uint16_t requiredCount;
    uint16_t index;
    uint8_t stageCount = 0U;
    uint16_t stageLength;

    if (plan == 0) {
        return false;
    }

    plan->twiddles = 0;
    plan->length = 0U;
    plan->twiddleCount = 0U;
    plan->stageCount = 0U;
    plan->initialized = false;

    requiredCount = FFT_getRequiredTwiddleCount(length);
    if ((requiredCount == 0U) || (twiddleBuffer == 0) ||
        (twiddleCapacity < requiredCount)) {
        return false;
    }

    for (stageLength = length; stageLength > 1U; stageLength >>= 1) {
        stageCount++;
    }

    for (index = 0U; index < requiredCount; index++) {
        uint32_t phase = (uint32_t)
            (((uint64_t) index << 32) / length);
        int32_t cosine;
        int32_t sine;

        FFT_sineCosineQ30(phase, &cosine, &sine);
        twiddleBuffer[index].real = FFT_q30ToQ15(cosine);
        twiddleBuffer[index].imag = FFT_q30ToQ15(-sine);
    }

    plan->twiddles = twiddleBuffer;
    plan->length = length;
    plan->twiddleCount = requiredCount;
    plan->stageCount = stageCount;
    plan->initialized = true;
    return true;
}

uint16_t FFT_getLength(const FFT_PlanQ15 *plan)
{
    return FFT_isPlanValid(plan) ? plan->length : 0U;
}

uint16_t FFT_getRealBinCount(const FFT_PlanQ15 *plan)
{
    return FFT_isPlanValid(plan) ?
        (uint16_t) (plan->length / 2U + 1U) : 0U;
}

bool FFT_loadReal(const FFT_PlanQ15 *plan,
    const int16_t *input, FFT_ComplexQ15 *output)
{
    uint16_t index;

    if (!FFT_isPlanValid(plan) || (input == 0) || (output == 0) ||
        FFT_rangesOverlap(input,
            (size_t) plan->length * sizeof(*input),
            output, (size_t) plan->length * sizeof(*output))) {
        return false;
    }

    for (index = 0U; index < plan->length; index++) {
        output[index].real = input[index];
        output[index].imag = 0;
    }
    return true;
}

bool FFT_execute(const FFT_PlanQ15 *plan,
    FFT_ComplexQ15 *data, FFT_ExecutionInfo *info)
{
    uint16_t index;
    uint16_t butterflySize;
    uint32_t saturationCount = 0U;

    if (!FFT_isPlanValid(plan) || (data == 0)) {
        return false;
    }

    for (index = 0U; index < plan->length; index++) {
        uint16_t reversed = FFT_reverseBits(index, plan->stageCount);

        if (reversed > index) {
            FFT_ComplexQ15 temporary = data[index];
            data[index] = data[reversed];
            data[reversed] = temporary;
        }
    }

    for (butterflySize = 2U;
         butterflySize <= plan->length;
         butterflySize <<= 1) {
        uint16_t halfSize = (uint16_t) (butterflySize / 2U);
        uint16_t twiddleStep =
            (uint16_t) (plan->length / butterflySize);
        uint16_t blockStart;

        for (blockStart = 0U; blockStart < plan->length;
             blockStart = (uint16_t) (blockStart + butterflySize)) {
            uint16_t pair;

            for (pair = 0U; pair < halfSize; pair++) {
                uint16_t topIndex = (uint16_t) (blockStart + pair);
                uint16_t bottomIndex = (uint16_t) (topIndex + halfSize);
                uint16_t twiddleIndex = (uint16_t) (pair * twiddleStep);
                const FFT_ComplexQ15 *twiddle =
                    &plan->twiddles[twiddleIndex];
                int32_t bottomReal = data[bottomIndex].real;
                int32_t bottomImag = data[bottomIndex].imag;
                int32_t rotatedReal;
                int32_t rotatedImag;
                int32_t topReal = data[topIndex].real;
                int32_t topImag = data[topIndex].imag;

                /* W[0] 数学上等于 1，直接复制可避免 Q15 的 32767
                 * 近似值在每一级重复造成直流幅值衰减。 */
                if (twiddleIndex == 0U) {
                    rotatedReal = bottomReal;
                    rotatedImag = bottomImag;
                } else {
                    rotatedReal = FFT_q30ProductToQ15(
                        (int64_t) bottomReal * twiddle->real -
                        (int64_t) bottomImag * twiddle->imag);
                    rotatedImag = FFT_q30ProductToQ15(
                        (int64_t) bottomReal * twiddle->imag +
                        (int64_t) bottomImag * twiddle->real);
                }

                data[topIndex].real = FFT_saturateQ15(
                    FFT_halfRounded(topReal + rotatedReal),
                    &saturationCount);
                data[topIndex].imag = FFT_saturateQ15(
                    FFT_halfRounded(topImag + rotatedImag),
                    &saturationCount);
                data[bottomIndex].real = FFT_saturateQ15(
                    FFT_halfRounded(topReal - rotatedReal),
                    &saturationCount);
                data[bottomIndex].imag = FFT_saturateQ15(
                    FFT_halfRounded(topImag - rotatedImag),
                    &saturationCount);
            }
        }
    }

    if (info != 0) {
        info->stageCount = plan->stageCount;
        info->scaleShift = plan->stageCount;
        info->saturationCount = saturationCount;
    }
    return true;
}

bool FFT_executeReal(const FFT_PlanQ15 *plan, const int16_t *input,
    FFT_ComplexQ15 *output, FFT_ExecutionInfo *info)
{
    return FFT_loadReal(plan, input, output) &&
           FFT_execute(plan, output, info);
}
