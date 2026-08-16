#include "Algorithms/transforms/ifft/ifft.h"

#include <limits.h>
#include <stddef.h>

/*
 * 本模块与 Algorithms/transforms/fft 配套。FFT 输出已经在每一级除以 2，
 * 即保存 DFT/N；因此逆变换蝶形不再缩小，正好恢复原时域尺度。
 *
 * 逆变换的旋转方向与正向 FFT 相反。本模块直接复用 FFT 计划中的旋转
 * 因子，在复数乘法时把其虚部符号反转，不再保存第二份系数表。
 *
 * 未缩放蝶形的中间结果会逐级增大，所以必须先复制到 int32_t 工作区。
 * 不要为了节省内存直接在 Q15 频谱上执行未缩放蝶形，否则中间级很容易
 * 饱和。1024 点 IFFT 的工作区需要 1024 * 8 = 8192 字节。
 */

static bool IFFT_rangesOverlap(const void *first, size_t firstSize,
    const void *second, size_t secondSize)
{
    uintptr_t firstStart = (uintptr_t) first;
    uintptr_t secondStart = (uintptr_t) second;

    return (firstStart < secondStart + secondSize) &&
           (secondStart < firstStart + firstSize);
}

static uint16_t IFFT_reverseBits(uint16_t value, uint8_t bitCount)
{
    uint16_t reversed = 0U;
    uint8_t bit;

    for (bit = 0U; bit < bitCount; bit++) {
        reversed = (uint16_t) ((reversed << 1) | (value & 1U));
        value >>= 1;
    }
    return reversed;
}

static int32_t IFFT_q15ProductRounded(int64_t value)
{
    if (value >= 0) {
        return (int32_t) ((value + 16384) / 32768);
    }
    return (int32_t) -((-value + 16384) / 32768);
}

static int32_t IFFT_saturateI32(
    int64_t value, uint32_t *saturationCount)
{
    if (value > INT32_MAX) {
        if (*saturationCount != UINT32_MAX) {
            (*saturationCount)++;
        }
        return INT32_MAX;
    }
    if (value < INT32_MIN) {
        if (*saturationCount != UINT32_MAX) {
            (*saturationCount)++;
        }
        return INT32_MIN;
    }
    return (int32_t) value;
}

static int16_t IFFT_saturateQ15(
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

uint16_t IFFT_getRequiredWorkspaceCount(const FFT_PlanQ15 *fftPlan)
{
    return FFT_getLength(fftPlan);
}

bool IFFT_execute(const FFT_PlanQ15 *fftPlan,
    const FFT_ComplexQ15 *spectrum,
    IFFT_ComplexI32 *workspace,
    FFT_ComplexQ15 *output,
    IFFT_ExecutionInfo *info)
{
    uint16_t length = FFT_getLength(fftPlan);
    uint16_t index;
    uint16_t butterflySize;
    uint32_t internalSaturationCount = 0U;
    uint32_t outputSaturationCount = 0U;

    if ((length == 0U) || (spectrum == 0) ||
        (workspace == 0) || (output == 0)) {
        return false;
    }

    if (IFFT_rangesOverlap(workspace,
            (size_t) length * sizeof(*workspace),
            spectrum, (size_t) length * sizeof(*spectrum)) ||
        IFFT_rangesOverlap(workspace,
            (size_t) length * sizeof(*workspace),
            output, (size_t) length * sizeof(*output))) {
        return false;
    }

    /* 先完整复制到宽位工作区，因此 spectrum 与 output 可以相同。 */
    for (index = 0U; index < length; index++) {
        workspace[index].real = spectrum[index].real;
        workspace[index].imag = spectrum[index].imag;
    }

    for (index = 0U; index < length; index++) {
        uint16_t reversed =
            IFFT_reverseBits(index, fftPlan->stageCount);

        if (reversed > index) {
            IFFT_ComplexI32 temporary = workspace[index];
            workspace[index] = workspace[reversed];
            workspace[reversed] = temporary;
        }
    }

    for (butterflySize = 2U;
         butterflySize <= length;
         butterflySize <<= 1) {
        uint16_t halfSize = (uint16_t) (butterflySize / 2U);
        uint16_t twiddleStep = (uint16_t) (length / butterflySize);
        uint16_t blockStart;

        for (blockStart = 0U; blockStart < length;
             blockStart = (uint16_t) (blockStart + butterflySize)) {
            uint16_t pair;

            for (pair = 0U; pair < halfSize; pair++) {
                uint16_t topIndex = (uint16_t) (blockStart + pair);
                uint16_t bottomIndex =
                    (uint16_t) (topIndex + halfSize);
                uint16_t twiddleIndex =
                    (uint16_t) (pair * twiddleStep);
                const FFT_ComplexQ15 *twiddle =
                    &fftPlan->twiddles[twiddleIndex];
                int32_t bottomReal = workspace[bottomIndex].real;
                int32_t bottomImag = workspace[bottomIndex].imag;
                int32_t rotatedReal;
                int32_t rotatedImag;
                int32_t topReal = workspace[topIndex].real;
                int32_t topImag = workspace[topIndex].imag;

                /* FFT 表保存 cos-j*sin；逆变换使用 cos+j*sin。 */
                if (twiddleIndex == 0U) {
                    rotatedReal = bottomReal;
                    rotatedImag = bottomImag;
                } else {
                    rotatedReal = IFFT_q15ProductRounded(
                        (int64_t) bottomReal * twiddle->real +
                        (int64_t) bottomImag * twiddle->imag);
                    rotatedImag = IFFT_q15ProductRounded(
                        -(int64_t) bottomReal * twiddle->imag +
                        (int64_t) bottomImag * twiddle->real);
                }

                workspace[topIndex].real = IFFT_saturateI32(
                    (int64_t) topReal + rotatedReal,
                    &internalSaturationCount);
                workspace[topIndex].imag = IFFT_saturateI32(
                    (int64_t) topImag + rotatedImag,
                    &internalSaturationCount);
                workspace[bottomIndex].real = IFFT_saturateI32(
                    (int64_t) topReal - rotatedReal,
                    &internalSaturationCount);
                workspace[bottomIndex].imag = IFFT_saturateI32(
                    (int64_t) topImag - rotatedImag,
                    &internalSaturationCount);
            }
        }
    }

    for (index = 0U; index < length; index++) {
        output[index].real = IFFT_saturateQ15(
            workspace[index].real, &outputSaturationCount);
        output[index].imag = IFFT_saturateQ15(
            workspace[index].imag, &outputSaturationCount);
    }

    if (info != 0) {
        info->stageCount = fftPlan->stageCount;
        info->internalSaturationCount = internalSaturationCount;
        info->outputSaturationCount = outputSaturationCount;
    }
    return true;
}
