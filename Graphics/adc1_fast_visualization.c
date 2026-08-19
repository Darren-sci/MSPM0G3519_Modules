#include "Graphics/adc1_fast_visualization.h"

#include <stdio.h>

#include "Drivers/lcd_8080.h"
#include "Graphics/lcd_graphics.h"
#include "Graphics/lcd_text.h"

#define ADC1_VIS_WAVE_X           (40U)
#define ADC1_VIS_WAVE_Y           (82U)
#define ADC1_VIS_WAVE_WIDTH       (774U)
#define ADC1_VIS_WAVE_HEIGHT      (145U)
#define ADC1_VIS_SPECTRUM_X       (40U)
#define ADC1_VIS_SPECTRUM_Y       (285U)
#define ADC1_VIS_SPECTRUM_WIDTH   (774U)
#define ADC1_VIS_SPECTRUM_HEIGHT  (145U)
#define ADC1_VIS_ADC_FULL_SCALE   (4095U)

static uint16_t ADC1FastVisualization_rawToY(uint16_t rawCode)
{
    uint32_t code = rawCode;

    if (code > ADC1_VIS_ADC_FULL_SCALE) {
        code = ADC1_VIS_ADC_FULL_SCALE;
    }
    return (uint16_t)(ADC1_VIS_WAVE_Y + ADC1_VIS_WAVE_HEIGHT - 1U -
        (code * (ADC1_VIS_WAVE_HEIGHT - 1U)) /
            ADC1_VIS_ADC_FULL_SCALE);
}

static void ADC1FastVisualization_drawWaveform(
    const uint16_t *samples, uint16_t sampleCount)
{
    uint16_t x;
    uint16_t previousX = ADC1_VIS_WAVE_X;
    uint16_t previousY;

    LCD8080_fillRect(ADC1_VIS_WAVE_X, ADC1_VIS_WAVE_Y,
        ADC1_VIS_WAVE_WIDTH, ADC1_VIS_WAVE_HEIGHT, LCD_COLOR_BLACK);
    LCDGraphics_drawRect(ADC1_VIS_WAVE_X, ADC1_VIS_WAVE_Y,
        ADC1_VIS_WAVE_WIDTH, ADC1_VIS_WAVE_HEIGHT, LCD_COLOR_GRAY);

    if ((samples == 0) || (sampleCount < 2U)) {
        return;
    }

    previousY = ADC1FastVisualization_rawToY(samples[0]);
    for (x = 1U; x < ADC1_VIS_WAVE_WIDTH; x++) {
        uint32_t sampleIndex =
            ((uint32_t)x * (sampleCount - 1U)) /
            (ADC1_VIS_WAVE_WIDTH - 1U);
        uint16_t screenX = (uint16_t)(ADC1_VIS_WAVE_X + x);
        uint16_t screenY =
            ADC1FastVisualization_rawToY(samples[sampleIndex]);

        LCDGraphics_drawLine(previousX, previousY,
            screenX, screenY, LCD_COLOR_CYAN);
        previousX = screenX;
        previousY = screenY;
    }
}

static void ADC1FastVisualization_drawSpectrum(
    const SignalAnalyzer *analyzer)
{
    const uint32_t *amplitudes;
    uint16_t binCount;
    uint16_t bin;
    uint32_t maximum = 0U;
    uint16_t previousX = ADC1_VIS_SPECTRUM_X;
    uint16_t previousY =
        (uint16_t)(ADC1_VIS_SPECTRUM_Y + ADC1_VIS_SPECTRUM_HEIGHT - 1U);

    LCD8080_fillRect(ADC1_VIS_SPECTRUM_X, ADC1_VIS_SPECTRUM_Y,
        ADC1_VIS_SPECTRUM_WIDTH, ADC1_VIS_SPECTRUM_HEIGHT,
        LCD_COLOR_BLACK);
    LCDGraphics_drawRect(ADC1_VIS_SPECTRUM_X, ADC1_VIS_SPECTRUM_Y,
        ADC1_VIS_SPECTRUM_WIDTH, ADC1_VIS_SPECTRUM_HEIGHT,
        LCD_COLOR_GRAY);

    if ((analyzer == 0) ||
        !SignalAnalyzer_getAmplitudeSpectrum(
            analyzer, &amplitudes, &binCount) ||
        (binCount < 3U)) {
        return;
    }

    /* 忽略直流点，按当前频谱最大交流分量自动缩放纵轴。 */
    for (bin = 1U; bin < binCount; bin++) {
        if (amplitudes[bin] > maximum) {
            maximum = amplitudes[bin];
        }
    }
    if (maximum == 0U) {
        return;
    }

    for (bin = 1U; bin < binCount; bin++) {
        uint16_t screenX = (uint16_t)(ADC1_VIS_SPECTRUM_X +
            ((uint32_t)(bin - 1U) * (ADC1_VIS_SPECTRUM_WIDTH - 1U)) /
                (binCount - 2U));
        uint16_t screenY = (uint16_t)(ADC1_VIS_SPECTRUM_Y +
            ADC1_VIS_SPECTRUM_HEIGHT - 1U -
            ((uint64_t)amplitudes[bin] *
                (ADC1_VIS_SPECTRUM_HEIGHT - 1U)) / maximum);

        LCDGraphics_drawLine(previousX, previousY,
            screenX, screenY, LCD_COLOR_YELLOW);
        previousX = screenX;
        previousY = screenY;
    }
}

void ADC1FastVisualization_draw(
    const uint16_t *samples,
    uint16_t sampleCount,
    uint32_t sampleRateHz,
    const SignalAnalyzer *analyzer,
    const SignalAnalyzer_Result *result,
    uint32_t overrunCount)
{
    uint16_t index;
    uint16_t minimum = ADC1_VIS_ADC_FULL_SCALE;
    uint16_t maximum = 0U;
    uint32_t frequencyMilliHz = 0U;
    char text[96];

    if ((samples == 0) || (sampleCount == 0U) || (result == 0)) {
        return;
    }

    for (index = 0U; index < sampleCount; index++) {
        uint16_t value = samples[index];
        if (value < minimum) {
            minimum = value;
        }
        if (value > maximum) {
            maximum = value;
        }
    }

    if ((result->validFeatures & SIGNAL_ANALYZER_FEATURE_FREQUENCY) != 0U) {
        frequencyMilliHz = result->frequency.frequencyMilliHz;
    }

    LCD8080_fillRect(0U, 0U, LCD_WIDTH, 75U, LCD_COLOR_DARK_GRAY);
    (void)snprintf(text, sizeof(text),
        "ADC1 FAST  Fs:%lu kS/s  DROP:%lu",
        (unsigned long)(sampleRateHz / 1000U),
        (unsigned long)overrunCount);
    LCDText_drawAsciiString(24U, 12U, text,
        LCD_COLOR_WHITE, LCD_COLOR_DARK_GRAY, 2U, 0U);

    if (frequencyMilliHz != 0U) {
        (void)snprintf(text, sizeof(text),
            "FREQ:%lu.%03lu Hz  CODE P-P:%u",
            (unsigned long)(frequencyMilliHz / 1000U),
            (unsigned long)(frequencyMilliHz % 1000U),
            (unsigned int)(maximum - minimum));
    } else {
        (void)snprintf(text, sizeof(text),
            "FREQ:---  CODE P-P:%u",
            (unsigned int)(maximum - minimum));
    }
    LCDText_drawAsciiString(24U, 42U, text,
        LCD_COLOR_GREEN, LCD_COLOR_DARK_GRAY, 2U, 0U);

    LCDText_drawAsciiString(40U, 64U, "WAVEFORM",
        LCD_COLOR_CYAN, LCD_COLOR_BLACK, 1U, 0U);
    ADC1FastVisualization_drawWaveform(samples, sampleCount);

    LCDText_drawAsciiString(40U, 267U, "SPECTRUM: 0 Hz TO Fs/2",
        LCD_COLOR_YELLOW, LCD_COLOR_BLACK, 1U, 0U);
    ADC1FastVisualization_drawSpectrum(analyzer);
}
