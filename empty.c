/*
 * Copyright (c) 2021, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * * Neither the name of Texas Instruments Incorporated nor the names of
 *   its contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ti_msp_dl_config.h"

#include "Drivers/adc_multi.h"
#include "Drivers/adc1_fast.h"
#include "Drivers/dac_output.h"
#include "Drivers/key.h"
#include "Drivers/lcd_8080.h"
#include "Drivers/lcd_panel.h"
#include "Drivers/oled_ssd1315.h"
#include "Drivers/pwm_output.h"
#include "Drivers/spwm.h"
#include "Drivers/system_tick.h"
#include "Algorithms/analysis_pipeline/signal_analyzer.h"
#include "Graphics/adc1_fast_visualization.h"
#include "Graphics/adc_multi_visualization.h"
#include "Graphics/lcd_text.h"

/*
 * 功能开关：需要某项功能时改为1，不需要时改为0。
 * 被关闭功能的变量和处理代码不会参与编译，也不会占用SRAM。
 */
#ifndef ENABLE_LCD
#define ENABLE_LCD                    (1)
#endif
#ifndef ENABLE_OLED
#define ENABLE_OLED                   (0)
#endif
#ifndef ENABLE_ADC0_MULTI
#define ENABLE_ADC0_MULTI             (0)
#endif
#ifndef ENABLE_ADC1_FAST
#define ENABLE_ADC1_FAST              (1)
#endif
#ifndef ENABLE_ADC1_ANALYZER
#define ENABLE_ADC1_ANALYZER          (1)
#endif
#ifndef ENABLE_DAC_DC
#define ENABLE_DAC_DC                 (0)
#endif
#ifndef ENABLE_DAC_WAVEFORM
#define ENABLE_DAC_WAVEFORM           (0)
#endif
#ifndef ENABLE_FIXED_PWM_CH0
#define ENABLE_FIXED_PWM_CH0          (0)
#endif
#ifndef ENABLE_FIXED_PWM_CH1
#define ENABLE_FIXED_PWM_CH1          (0)
#endif
#ifndef ENABLE_SPWM_CH0
#define ENABLE_SPWM_CH0               (0)
#endif
#ifndef ENABLE_SPWM_CH1
#define ENABLE_SPWM_CH1               (0)
#endif
#ifndef ENABLE_STATE_MACHINE
#define ENABLE_STATE_MACHINE           (0)
#endif

/* 各模块常用参数集中放在这里，换题时不必进入驱动内部修改。 */
#define ADC0_FRAME_RATE_HZ            (10000U)
/* 用50 kHz已知信号、原显示12.536 kHz完成一次实测标定。 */
#define ADC1_ANALYZER_SAMPLE_RATE_HZ  (3988513U)
/* 每64个DMA块刷新一次LCD；按键切页后下一块会立即刷新。 */
#define ADC1_UI_REFRESH_BLOCKS         (64U)
/* ADC1每块1024点；每250块更新一次OLED，避免低速显示影响高速采集。 */
#define OLED_UI_REFRESH_BLOCKS         (250U)
#define DAC_DC_OUTPUT_CODE            (3072U)
#define FIXED_PWM_CH0_DUTY_PERMILLE   (300U)
#define FIXED_PWM_CH1_DUTY_PERMILLE   (600U)
/* 状态机任务周期，单位为毫秒；仅在 ENABLE_STATE_MACHINE 为 1 时生效。 */
#define STATE_MACHINE_PERIOD_MS        (10U)

/* 同一个通道不能同时由固定PWM和SPWM控制。 */
#if ENABLE_FIXED_PWM_CH0 && ENABLE_SPWM_CH0
#error "PA0 cannot output fixed PWM and SPWM at the same time"
#endif
#if ENABLE_FIXED_PWM_CH1 && ENABLE_SPWM_CH1
#error "PA1 cannot output fixed PWM and SPWM at the same time"
#endif
#if ENABLE_DAC_DC && ENABLE_DAC_WAVEFORM
#error "DAC0 cannot output DC and DMA waveform at the same time"
#endif
#if ENABLE_ADC1_ANALYZER && !ENABLE_ADC1_FAST
#error "SignalAnalyzer input is enabled but ADC1_FAST is disabled"
#endif

#define ENABLE_ANY_SPWM  (ENABLE_SPWM_CH0 || ENABLE_SPWM_CH1)
#define ENABLE_ANY_FIXED_PWM \
    (ENABLE_FIXED_PWM_CH0 || ENABLE_FIXED_PWM_CH1)

#if ENABLE_ADC0_MULTI
/* DMA缓冲区由驱动持有；这里保存当前只读指针以及最近一帧的安全副本。 */
static const ADCMulti_Frame *gADC0Frames;
static uint16_t gADC0FrameCount;
static ADCMulti_Frame gADC0LatestFrame;
static bool gADC0Started;
#endif

#if ENABLE_ADC1_FAST
/* 指针只在取得缓冲区到释放缓冲区之间有效，不能跨循环长期保存。 */
static const uint16_t *gADC1Samples;
static uint16_t gADC1SampleCount;
static volatile uint16_t gADC1LatestSample;
static bool gADC1Started;
#endif

#if ENABLE_OLED
/* OLED运行状态和更新事件分开管理，业务代码只需提出更新请求。 */
typedef enum {
    MAIN_OLED_STATE_UNINITIALIZED = 0,
    MAIN_OLED_STATE_ACTIVE,
    MAIN_OLED_STATE_ERROR
} Main_OLEDState;

#define MAIN_OLED_UPDATE_NONE          (0U)
#define MAIN_OLED_UPDATE_ADC1          (1U << 0)
#define MAIN_OLED_UPDATE_ALL           (1U << 7)

static Main_OLEDState gOledState = MAIN_OLED_STATE_UNINITIALIZED;
static uint8_t gOledUpdateFlags = MAIN_OLED_UPDATE_NONE;
#if ENABLE_ADC1_FAST
static uint16_t gOledUIRefreshBlocks = OLED_UI_REFRESH_BLOCKS;
#endif

/* 只重画动态数据区域；驱动会自动仅发送实际变化的显存页。 */
static void Main_drawOLEDADC1Data(void)
{
    OLED_ClearArea(0U, 32U, OLED_WIDTH, 32U);
#if ENABLE_ADC1_FAST
    OLED_ShowString(0U, 32U, "ADC1:", OLED_FONT_6X8);
    OLED_ShowUInt(36U, 32U, gADC1LatestSample, 4U, OLED_FONT_6X8);
    OLED_ShowString(0U, 48U, "OVERRUN:", OLED_FONT_6X8);
    OLED_ShowUInt(54U, 48U,
        ADC1Fast_getOverrunCount(), 0U, OLED_FONT_6X8);
#else
    OLED_ShowString(0U, 32U, "ADC1: DISABLED", OLED_FONT_6X8);
#endif
}

/* OLED统一出口：先完成全部显存绘制，最后只刷新一次。 */
static void Main_processOLED(void)
{
    uint8_t flags;

    if ((gOledState != MAIN_OLED_STATE_ACTIVE) ||
        (gOledUpdateFlags == MAIN_OLED_UPDATE_NONE)) {
        return;
    }

    flags = gOledUpdateFlags;
    gOledUpdateFlags = MAIN_OLED_UPDATE_NONE;

    if ((flags & MAIN_OLED_UPDATE_ALL) != 0U) {
        OLED_ClearBuffer();
        OLED_ShowString(0U, 0U, "SSD1315 OLED", OLED_FONT_6X8);
        OLED_DrawLine(0U, 10U, 127U, 10U, true);
        OLED_ShowString(0U, 16U, "STATE: RUN", OLED_FONT_6X8);
        Main_drawOLEDADC1Data();
    } else if ((flags & MAIN_OLED_UPDATE_ADC1) != 0U) {
        Main_drawOLEDADC1Data();
    }

    if (!OLED_Refresh()) {
        /* I2C超时或无应答后停止继续刷新，避免主循环永久阻塞。 */
        gOledState = MAIN_OLED_STATE_ERROR;
    }
}
#endif

#if ENABLE_ADC1_ANALYZER
/* 工作区约占十余KB，必须使用static，不能放到主函数栈中。 */
static SignalAnalyzer gSignalAnalyzer;
static SignalAnalyzer_Workspace gSignalAnalyzerWorkspace;
static SignalAnalyzer_Result gSignalAnalyzerResult;
static SignalAnalyzer_Status gSignalAnalyzerStatus;
static bool gSignalAnalyzerReady;

static bool Main_initSignalAnalyzer(void)
{
    SignalAnalyzer_Config config;

    if (!SignalAnalyzer_getDefaultConfig(&config,
            ADC1_ANALYZER_SAMPLE_RATE_HZ,
            ADC1_FAST_SAMPLE_COUNT)) {
        return false;
    }

    /* 0、2048、4095是模板值，正式测量时替换为模拟前端的实测标定码。 */
    config.rawCalibrationEnabled = ADCCalibration_initBipolarQ15(
        &config.rawCalibration, 0U, 2048U, 4095U);
    /* 示波页面不再使用独立的时域F，关闭该项以减少每帧计算。 */
    config.enabledFeatures &= ~SIGNAL_ANALYZER_FEATURE_FREQUENCY;
    config.enabledFeatures |=
        SIGNAL_ANALYZER_FEATURE_SPECTRUM |
        SIGNAL_ANALYZER_FEATURE_FUNDAMENTAL |
        SIGNAL_ANALYZER_FEATURE_HARMONICS |
        SIGNAL_ANALYZER_FEATURE_THD;
    config.windowType = SIGNAL_ANALYZER_WINDOW_HANN;
    config.minimumFrequencyMilliHz = 100U * 1000U;
    config.maximumFrequencyMilliHz =
        (uint64_t)ADC1_ANALYZER_SAMPLE_RATE_HZ * 500U;

    gSignalAnalyzerStatus = SignalAnalyzer_init(
        &gSignalAnalyzer, &config, &gSignalAnalyzerWorkspace);
    return gSignalAnalyzerStatus == SIGNAL_ANALYZER_STATUS_OK;
}

static void Main_analyzeADC1Block(
    const uint16_t *samples, uint16_t sampleCount)
{
    uint32_t remaining = sampleCount;

    while (remaining != 0U) {
        uint32_t consumed = 0U;

        gSignalAnalyzerStatus = SignalAnalyzer_pushRawADC(
            &gSignalAnalyzer, samples, remaining, 1U,
            &consumed, &gSignalAnalyzerResult);
        samples += consumed;
        remaining -= consumed;

        if ((gSignalAnalyzerStatus != SIGNAL_ANALYZER_STATUS_RESULT_READY) &&
            (gSignalAnalyzerStatus != SIGNAL_ANALYZER_STATUS_NEED_MORE_DATA)) {
            break;
        }
        if (consumed == 0U) {
            break;
        }
    }
}
#endif

#if ENABLE_LCD && ENABLE_ADC1_FAST && ENABLE_ADC1_ANALYZER
/* 两个页面只由这个状态选择：KEY1示波器，KEY2频谱。 */
typedef enum {
    MAIN_ADC1_PAGE_SCOPE = 0,
    MAIN_ADC1_PAGE_SPECTRUM
} Main_ADC1Page;

static Main_ADC1Page gADC1Page = MAIN_ADC1_PAGE_SCOPE;
static uint16_t gADC1DisplaySamples[ADC1_FAST_SAMPLE_COUNT];
static uint16_t gADC1DisplaySampleCount;
static uint16_t gADC1UIRefreshBlocks = ADC1_UI_REFRESH_BLOCKS;

/*
 * 检查相隔一个基波周期的波形是否重复。它不要求波形像正弦，因此基波叠加
 * 谐波、方波和削顶波仍可通过；采样块中出现不连续跳变时会被拒绝。
 */
static bool Main_isADC1DisplayFrameValid(
    const uint16_t *samples, uint16_t sampleCount,
    const SignalAnalyzer_Result *result)
{
    uint64_t frequencyMilliHz;
    uint64_t expectedPeriod64;
    uint16_t expectedPeriod;
    uint16_t searchRadius;
    uint16_t firstLag;
    uint16_t lastLag;
    uint16_t lag;
    uint16_t index;
    uint16_t minimum = 4095U;
    uint16_t maximum = 0U;
    uint32_t span;
    uint32_t largeDifference;
    uint64_t bestMeanError = (uint64_t)-1;
    uint32_t bestOutliers = UINT32_MAX;
    uint16_t bestComparisonCount = 0U;

    if ((samples == 0) || (sampleCount < 2U) || (result == 0)) {
        return false;
    }

    /* 周期不足两次或尚无可靠F0时没有足够依据，保持原有显示行为。 */
    if (!result->fundamental.valid ||
        (result->fundamental.frequencyMilliHz == 0U)) {
        return true;
    }
    frequencyMilliHz = result->fundamental.frequencyMilliHz;
    expectedPeriod64 =
        ((uint64_t)ADC1_ANALYZER_SAMPLE_RATE_HZ * 1000U +
            frequencyMilliHz / 2U) / frequencyMilliHz;
    if ((expectedPeriod64 < 4U) ||
        (expectedPeriod64 * 2U >= sampleCount)) {
        return true;
    }
    expectedPeriod = (uint16_t)expectedPeriod64;

    for (index = 0U; index < sampleCount; index++) {
        if (samples[index] < minimum) {
            minimum = samples[index];
        }
        if (samples[index] > maximum) {
            maximum = samples[index];
        }
    }
    span = (uint32_t)maximum - minimum;
    if (span < 16U) {
        return true;
    }

    searchRadius = (uint16_t)(expectedPeriod / 20U);
    if (searchRadius < 2U) {
        searchRadius = 2U;
    }
    firstLag = (expectedPeriod > searchRadius) ?
        (uint16_t)(expectedPeriod - searchRadius) : 2U;
    if (firstLag < 2U) {
        firstLag = 2U;
    }
    lastLag = (uint16_t)(expectedPeriod + searchRadius);
    if (lastLag >= sampleCount) {
        lastLag = (uint16_t)(sampleCount - 1U);
    }
    largeDifference = span / 4U;
    if (largeDifference < 8U) {
        largeDifference = 8U;
    }

    for (lag = firstLag; lag <= lastLag; lag++) {
        uint16_t comparisonCount = (uint16_t)(sampleCount - lag);
        uint64_t errorSum = 0U;
        uint32_t outliers = 0U;
        uint64_t meanError;

        for (index = 0U; index < comparisonCount; index++) {
            int32_t difference =
                (int32_t)samples[index + lag] - samples[index];
            uint32_t absoluteDifference = (difference < 0) ?
                (uint32_t)(-difference) : (uint32_t)difference;

            errorSum += absoluteDifference;
            if (absoluteDifference > largeDifference) {
                outliers++;
            }
        }
        meanError = errorSum / comparisonCount;
        if ((meanError < bestMeanError) ||
            ((meanError == bestMeanError) && (outliers < bestOutliers))) {
            bestMeanError = meanError;
            bestOutliers = outliers;
            bestComparisonCount = comparisonCount;
        }
    }

    /* 平均周期误差不超过峰峰值的1/8，大跳变样本不超过比较点的2%。 */
    return (bestComparisonCount != 0U) &&
        (bestMeanError * 8U <= span) &&
        ((uint64_t)bestOutliers * 50U <= bestComparisonCount);
}
#endif

#if ENABLE_DAC_WAVEFORM
/* 8点方波：前4点低电平，后4点高电平 */
static const uint16_t gDACWaveTable[8] = {
    512U, 512U, 512U, 512U,
    3584U, 3584U, 3584U, 3584U
};
#endif

#if ENABLE_STATE_MACHINE
/* 每个周期任务都要保存自己独立的上次执行时间。 */
static uint32_t gStateMachineLastTimeMs;
#endif

int main(void)
{

    /* 外设和驱动只需在上电后初始化一次。 */
    SYSCFG_DL_init();

#if ENABLE_STATE_MACHINE
    /* TIMG7 已由 SysConfig 配置为 1 ms 周期，这里只需启动一次。 */
    SystemTick_start();
#endif

    /* SysConfig预配置了DAC FIFO，先统一进入安全的0码停止状态。 */
    DACOutput_init();

#if ENABLE_ANY_SPWM
    /* SPWM初始化会停止TIMA0，并把PA0、PA1先置为50%中点。 */
    SPWM_init();
#endif

#if ENABLE_LCD
    LCDPanel_init();
    LCD8080_clear(LCD_COLOR_BLACK);
    // LCDText_drawAsciiString(
    // 40U, 40U,
    // "LCD OK\nADC0 WAITING...",
    // LCD_COLOR_WHITE,
    // LCD_COLOR_BLUE,
    // 3U,
    // 0U);
#endif

#if ENABLE_OLED
    /* 在高速ADC启动前完成首次整屏刷新，避免初始化传输造成DMA溢出。 */
    if (OLED_Init()) {
        gOledState = MAIN_OLED_STATE_ACTIVE;
        gOledUpdateFlags = MAIN_OLED_UPDATE_ALL;
        Main_processOLED();
    } else {
        gOledState = MAIN_OLED_STATE_ERROR;
    }
#endif

#if ENABLE_ADC1_ANALYZER
    gSignalAnalyzerReady = Main_initSignalAnalyzer();
    if (!gSignalAnalyzerReady) {
#if ENABLE_LCD
        ADCMultiVisualization_drawError("ANALYZER INIT FAILED");
#endif
    }
#endif

#if ENABLE_ADC0_MULTI
    ADCMulti_init();
    gADC0Started = ADCMulti_start(ADC0_FRAME_RATE_HZ);
    if (!gADC0Started) {
#if ENABLE_LCD
        ADCMultiVisualization_drawError("ADC START FAILED");
#endif
    }
#endif

#if ENABLE_ADC1_FAST
    ADC1Fast_init();
    gADC1Started = ADC1Fast_start();
    if (!gADC1Started) {
#if ENABLE_LCD
        ADCMultiVisualization_drawError("ADC1 START FAILED");
#endif
    }
#endif

#if ENABLE_DAC_DC
    /* PA15输出固定DAC码；可改用DACOutput_setMilliVolts()。 */
    (void)DACOutput_setCode(DAC_DC_OUTPUT_CODE);
#elif ENABLE_DAC_WAVEFORM
    /* PA15使用DMA_CH2循环输出示例三角波。 */
    (void)DACOutput_startWaveform(gDACWaveTable,
        (uint16_t)(sizeof(gDACWaveTable) / sizeof(gDACWaveTable[0])),
        DAC_OUTPUT_RATE_8_KHZ);
#endif

#if ENABLE_FIXED_PWM_CH0
    PWMOutput_setChannel0Duty(FIXED_PWM_CH0_DUTY_PERMILLE);
#endif
#if ENABLE_FIXED_PWM_CH1
    PWMOutput_setChannel1Duty(FIXED_PWM_CH1_DUTY_PERMILLE);
#endif

#if ENABLE_SPWM_CH0
    SPWM_set(SPWM_CHANNEL_0, 1000U, 800U, 0U);
    SPWM_start(SPWM_CHANNEL_0);
#endif
#if ENABLE_SPWM_CH1
    SPWM_set(SPWM_CHANNEL_1, 2000U, 600U, 90U);
    SPWM_start(SPWM_CHANNEL_1);
#endif

#if ENABLE_ANY_FIXED_PWM && !ENABLE_ANY_SPWM
    /* 只有固定PWM时没有SPWM_start()帮忙启动TIMA0，需要在此显式启动。 */
    DL_TimerA_startCounter(PWM_0_INST);
#endif

    while (1) {
#if ENABLE_STATE_MACHINE
        if (SystemTick_isDue(
                &gStateMachineLastTimeMs, STATE_MACHINE_PERIOD_MS)) {
            /* 在这里编写需要按固定周期反复执行的状态机代码。 */
        }
#endif

#if ENABLE_LCD && ENABLE_ADC1_FAST && ENABLE_ADC1_ANALYZER
        if (Key_wasClicked(KEY_1)) {
            gADC1Page = MAIN_ADC1_PAGE_SCOPE;
            gADC1UIRefreshBlocks = ADC1_UI_REFRESH_BLOCKS;
        }
        if (Key_wasClicked(KEY_2)) {
            gADC1Page = MAIN_ADC1_PAGE_SPECTRUM;
            gADC1UIRefreshBlocks = ADC1_UI_REFRESH_BLOCKS;
        }
#endif

#if ENABLE_ADC0_MULTI
        /*
         * ADC0四通道：非阻塞取得DMA块。复制最近帧后，可以在此加入
         * 功率、效率、纹波或四通道分析，再及时释放驱动缓冲区。
         */
        if (gADC0Started &&
            ADCMulti_getReadyBuffer(&gADC0Frames, &gADC0FrameCount)) {
            if (gADC0FrameCount != 0U) {
                gADC0LatestFrame = gADC0Frames[gADC0FrameCount - 1U];
            }
#if ENABLE_LCD
            ADCMultiVisualization_draw(gADC0Frames, gADC0FrameCount,
                ADCMulti_getActualFrameRate(),
                ADCMulti_getOverrunCount());
#endif
            ADCMulti_releaseBuffer(gADC0Frames);
            gADC0Frames = 0;
        }
#endif

#if ENABLE_ADC1_FAST
        /*
         * ADC1高速单通道：必须在releaseBuffer()之前完成算法处理或复制。
         * 不要在每个高速数据块到达时进行全屏LCD刷新。
         */
        if (gADC1Started &&
            ADC1Fast_getReadyBuffer(&gADC1Samples, &gADC1SampleCount)) {
#if ENABLE_LCD && ENABLE_ADC1_ANALYZER
            bool refreshADC1UI = false;
#endif
            if (gADC1SampleCount != 0U) {
                gADC1LatestSample = gADC1Samples[gADC1SampleCount - 1U];
            }
#if ENABLE_OLED
            gOledUIRefreshBlocks++;
            if (gOledUIRefreshBlocks >= OLED_UI_REFRESH_BLOCKS) {
                gOledUIRefreshBlocks = 0U;
                gOledUpdateFlags |= MAIN_OLED_UPDATE_ADC1;
            }
#endif
#if ENABLE_ADC1_ANALYZER
#if ENABLE_LCD
            gADC1UIRefreshBlocks++;
            if (gADC1UIRefreshBlocks >= ADC1_UI_REFRESH_BLOCKS) {
                uint16_t index;

                gADC1DisplaySampleCount = gADC1SampleCount;
                if (gADC1DisplaySampleCount > ADC1_FAST_SAMPLE_COUNT) {
                    gADC1DisplaySampleCount = ADC1_FAST_SAMPLE_COUNT;
                }
                for (index = 0U; index < gADC1DisplaySampleCount; index++) {
                    gADC1DisplaySamples[index] = gADC1Samples[index];
                }
                gADC1UIRefreshBlocks = 0U;
                refreshADC1UI = true;
            }
#else
            /* 无LCD时保留连续分析行为，但仍在释放DMA缓冲区前完成读取。 */
            if (gSignalAnalyzerReady) {
                Main_analyzeADC1Block(gADC1Samples, gADC1SampleCount);
            }
#endif
#endif
            ADC1Fast_releaseBuffer(gADC1Samples);
            gADC1Samples = 0;

#if ENABLE_LCD && ENABLE_ADC1_ANALYZER
            /*
             * 仅分析需要显示的快照。DMA缓冲区已经释放，FFT和LCD再慢也
             * 不会延长CPU对双缓冲区的占用时间。
             */
            if (refreshADC1UI) {
                bool displayFrameValid = false;

                if (gSignalAnalyzerReady) {
                    Main_analyzeADC1Block(
                        gADC1DisplaySamples, gADC1DisplaySampleCount);
                    displayFrameValid = Main_isADC1DisplayFrameValid(
                        gADC1DisplaySamples, gADC1DisplaySampleCount,
                        &gSignalAnalyzerResult);
                }
                if (!displayFrameValid) {
                    /* 保留LCD中的上一幅正常画面，并在下一完整块立即重试。 */
                    gADC1UIRefreshBlocks = ADC1_UI_REFRESH_BLOCKS;
                } else if (gADC1Page == MAIN_ADC1_PAGE_SCOPE) {
                    ADC1FastVisualization_drawScope(
                        gADC1DisplaySamples, gADC1DisplaySampleCount,
                        ADC1_ANALYZER_SAMPLE_RATE_HZ,
                        &gSignalAnalyzerResult,
                        ADC1Fast_getOverrunCount());
                } else {
                    ADC1FastVisualization_drawSpectrum(
                        &gSignalAnalyzer,
                        ADC1_ANALYZER_SAMPLE_RATE_HZ,
                        &gSignalAnalyzerResult,
                        ADC1Fast_getOverrunCount());
                }
            }
#endif
        }
#endif

#if ENABLE_OLED
        /* 所有高速DMA缓冲区均已释放后，才允许进行相对较慢的I2C显示。 */
        Main_processOLED();
#endif

#if !ENABLE_ADC0_MULTI && !ENABLE_ADC1_FAST
        /* 没有连续采集任务时进入休眠，等待已启用的外设中断。 */
        __WFI();
#endif
    }
}
