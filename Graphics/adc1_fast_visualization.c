#include "Graphics/adc1_fast_visualization.h"

#include <stdbool.h>
#include <stdio.h>

#include "Drivers/lcd_8080.h"
#include "Graphics/lcd_graphics.h"
#include "Graphics/lcd_text.h"

#define ADC1_VIS_PLOT_X             (44U)
#define ADC1_VIS_PLOT_Y             (72U)
#define ADC1_VIS_PLOT_WIDTH         (786U)
#define ADC1_VIS_PLOT_HEIGHT        (350U)
#define ADC1_VIS_SPECTRUM_WIDTH     (550U)
#define ADC1_VIS_SPECTRUM_INNER_WIDTH \
    (ADC1_VIS_SPECTRUM_WIDTH - 2U)
#define ADC1_VIS_HARMONIC_PANEL_X   (610U)
#define ADC1_VIS_HARMONIC_PANEL_Y   (ADC1_VIS_PLOT_Y)
#define ADC1_VIS_HARMONIC_PANEL_WIDTH (220U)
#define ADC1_VIS_HARMONIC_PANEL_HEIGHT (ADC1_VIS_PLOT_HEIGHT)
#define ADC1_VIS_DISPLAY_HARMONICS  (6U)
#define ADC1_VIS_INNER_X            (ADC1_VIS_PLOT_X + 1U)
#define ADC1_VIS_INNER_Y            (ADC1_VIS_PLOT_Y + 1U)
#define ADC1_VIS_INNER_WIDTH        (ADC1_VIS_PLOT_WIDTH - 2U)
#define ADC1_VIS_INNER_HEIGHT       (ADC1_VIS_PLOT_HEIGHT - 2U)
#define ADC1_VIS_PLOT_BOTTOM        \
    (ADC1_VIS_PLOT_Y + ADC1_VIS_PLOT_HEIGHT - 1U)
#define ADC1_VIS_SCOPE_CENTER_Y     \
    (ADC1_VIS_PLOT_Y + (ADC1_VIS_PLOT_HEIGHT / 2U))
#define ADC1_VIS_ADC_MID_CODE       (2048U)
#define ADC1_VIS_ADC_FULL_SCALE     (4095U)
#define ADC1_VIS_ADC_REFERENCE_MV   (3300U)
#define ADC1_VIS_Q15_FULL_SCALE     (32768U)
#define ADC1_VIS_TRIGGER_MIN_RANGE  (8U)

typedef struct {
    uint16_t start;
    uint16_t count;
    bool triggered;
} ADC1FastVisualization_WaveWindow;

typedef enum {
    ADC1_VIS_PAGE_NONE = 0,
    ADC1_VIS_PAGE_SCOPE,
    ADC1_VIS_PAGE_SPECTRUM
} ADC1FastVisualization_Page;

typedef struct {
    uint16_t x;
    uint16_t y;
    bool valid;
} ADC1FastVisualization_Marker;

static ADC1FastVisualization_Page gADC1VisPage = ADC1_VIS_PAGE_NONE;
static uint16_t gADC1VisScopeTop[ADC1_VIS_INNER_WIDTH];
static uint16_t gADC1VisScopeBottom[ADC1_VIS_INNER_WIDTH];
static uint16_t gADC1VisSpectrumHeight[ADC1_VIS_SPECTRUM_INNER_WIDTH];
static ADC1FastVisualization_Marker
    gADC1VisMarkers[ADC1_VIS_DISPLAY_HARMONICS];
static bool gADC1VisScopeValid;

static uint16_t ADC1FastVisualization_rawToScopeY(uint16_t rawCode)
{
    int32_t code = rawCode;
    int32_t halfHeight = (int32_t)(ADC1_VIS_PLOT_HEIGHT / 2U) - 2;
    int32_t y;

    if (code > (int32_t)ADC1_VIS_ADC_FULL_SCALE) {
        code = ADC1_VIS_ADC_FULL_SCALE;
    }
    y = (int32_t)ADC1_VIS_SCOPE_CENTER_Y -
        ((code - (int32_t)ADC1_VIS_ADC_MID_CODE) * halfHeight) /
            (int32_t)ADC1_VIS_ADC_MID_CODE;

    if (y < (int32_t)ADC1_VIS_INNER_Y) {
        y = ADC1_VIS_INNER_Y;
    } else if (y > (int32_t)(ADC1_VIS_PLOT_BOTTOM - 1U)) {
        y = ADC1_VIS_PLOT_BOTTOM - 1U;
    }
    return (uint16_t)y;
}

static void ADC1FastVisualization_drawHeaderStatic(
    const char *title, uint32_t sampleRateHz)
{
    char text[64];

    LCD8080_fillRect(0U, 0U, LCD_WIDTH, 64U, LCD_COLOR_DARK_GRAY);
    LCDText_drawAsciiString(20U, 8U, title,
        LCD_COLOR_WHITE, LCD_COLOR_DARK_GRAY, 2U, 0U);
    (void)snprintf(text, sizeof(text), "Fs:%lu kS/s",
        (unsigned long)(sampleRateHz / 1000U));
    LCDText_drawAsciiString(20U, 38U, text,
        LCD_COLOR_GREEN, LCD_COLOR_DARK_GRAY, 1U, 0U);
    LCDText_drawAsciiString(150U, 38U, "DROP:",
        LCD_COLOR_GREEN, LCD_COLOR_DARK_GRAY, 1U, 0U);
    LCDText_drawAsciiString(300U, 38U, "K1:SCOPE  K2:FFT",
        LCD_COLOR_GREEN, LCD_COLOR_DARK_GRAY, 1U, 0U);
}

static void ADC1FastVisualization_drawOverrunCount(uint32_t overrunCount)
{
    char text[16];

    (void)snprintf(text, sizeof(text), "%10lu",
        (unsigned long)overrunCount);
    LCDText_drawAsciiString(180U, 38U, text,
        LCD_COLOR_GREEN, LCD_COLOR_DARK_GRAY, 1U, 0U);
}

static void ADC1FastVisualization_drawGridLines(
    bool centeredXAxis, uint16_t plotWidth)
{
    uint16_t division;

    for (division = 1U; division < 10U; division++) {
        uint16_t x = (uint16_t)(ADC1_VIS_PLOT_X +
            ((uint32_t)division * (plotWidth - 1U)) / 10U);
        LCD8080_fillRect(x, ADC1_VIS_INNER_Y, 1U,
            ADC1_VIS_INNER_HEIGHT, LCD_COLOR_DARK_GRAY);
    }
    for (division = 1U; division < 8U; division++) {
        uint16_t y = (uint16_t)(ADC1_VIS_PLOT_Y +
            ((uint32_t)division * (ADC1_VIS_PLOT_HEIGHT - 1U)) / 8U);
        LCD8080_fillRect(ADC1_VIS_INNER_X, y,
            plotWidth - 2U, 1U, LCD_COLOR_DARK_GRAY);
    }

    if (centeredXAxis) {
        LCD8080_fillRect(ADC1_VIS_INNER_X, ADC1_VIS_SCOPE_CENTER_Y,
            ADC1_VIS_PLOT_WIDTH - 2U, 1U, LCD_COLOR_GRAY);
    }
}

static void ADC1FastVisualization_drawGrid(bool centeredXAxis)
{
    uint16_t plotWidth = centeredXAxis ?
        ADC1_VIS_PLOT_WIDTH : ADC1_VIS_SPECTRUM_WIDTH;

    LCD8080_fillRect(0U, 64U, LCD_WIDTH, LCD_HEIGHT - 64U,
        LCD_COLOR_BLACK);
    LCDGraphics_drawRect(ADC1_VIS_PLOT_X, ADC1_VIS_PLOT_Y,
        plotWidth, ADC1_VIS_PLOT_HEIGHT, LCD_COLOR_WHITE);
    ADC1FastVisualization_drawGridLines(centeredXAxis, plotWidth);

    if (centeredXAxis) {
        LCDText_drawAsciiString(4U, ADC1_VIS_PLOT_Y, "+FS",
            LCD_COLOR_GRAY, LCD_COLOR_BLACK, 1U, 0U);
        LCDText_drawAsciiString(20U, ADC1_VIS_SCOPE_CENTER_Y - 4U, "0",
            LCD_COLOR_WHITE, LCD_COLOR_BLACK, 1U, 0U);
        LCDText_drawAsciiString(4U, ADC1_VIS_PLOT_BOTTOM - 8U, "-FS",
            LCD_COLOR_GRAY, LCD_COLOR_BLACK, 1U, 0U);
    } else {
        LCDText_drawAsciiString(ADC1_VIS_PLOT_X, 423U, "0",
            LCD_COLOR_GRAY, LCD_COLOR_BLACK, 1U, 0U);
        LCDText_drawAsciiString(
            ADC1_VIS_PLOT_X + ADC1_VIS_SPECTRUM_WIDTH - 31U,
            423U, "Fs/2",
            LCD_COLOR_GRAY, LCD_COLOR_BLACK, 1U, 0U);
    }
}

static uint32_t ADC1FastVisualization_q15ToMilliVoltsPeak(
    uint32_t amplitudeQ15)
{
    return (uint32_t)(((uint64_t)amplitudeQ15 *
        (ADC1_VIS_ADC_REFERENCE_MV / 2U) +
        (ADC1_VIS_Q15_FULL_SCALE / 2U)) /
        ADC1_VIS_Q15_FULL_SCALE);
}

static void ADC1FastVisualization_drawHarmonicPanelStatic(void)
{
    LCDGraphics_drawRect(ADC1_VIS_HARMONIC_PANEL_X,
        ADC1_VIS_HARMONIC_PANEL_Y,
        ADC1_VIS_HARMONIC_PANEL_WIDTH,
        ADC1_VIS_HARMONIC_PANEL_HEIGHT, LCD_COLOR_WHITE);
    LCDText_drawAsciiString(626U, 80U, "HARMONICS",
        LCD_COLOR_CYAN, LCD_COLOR_BLACK, 2U, 0U);
    LCDText_drawAsciiString(627U, 101U, "FREQUENCY / Vpk",
        LCD_COLOR_GRAY, LCD_COLOR_BLACK, 1U, 0U);
}

static void ADC1FastVisualization_drawHarmonicPanel(
    const SignalAnalyzer_Result *result)
{
    uint16_t index;
    char text[64];

    for (index = 0U; index < ADC1_VIS_DISPLAY_HARMONICS; index++) {
        uint16_t y = (uint16_t)(126U + (uint32_t)index * 47U);
        bool valid = (result != 0) &&
            (index < result->harmonicCount) &&
            result->harmonics[index].valid;

        if (valid) {
            uint64_t frequencyMilliHz =
                result->harmonics[index].detectedFrequencyMilliHz;
            uint32_t amplitudeQ15 = result->harmonics[index].amplitudeQ15;
            uint32_t milliVoltsPeak;

            /* H1沿用抛物线插值后的F0，精度高于整数频点中心值。 */
            if ((index == 0U) && result->fundamental.valid) {
                frequencyMilliHz = result->fundamental.frequencyMilliHz;
                amplitudeQ15 = result->fundamental.amplitudeQ15;
            }
            milliVoltsPeak =
                ADC1FastVisualization_q15ToMilliVoltsPeak(amplitudeQ15);
            (void)snprintf(text, sizeof(text),
                "H%u %4lu.%03lu kHz",
                (unsigned int)(index + 1U),
                (unsigned long)(frequencyMilliHz / 1000000U),
                (unsigned long)((frequencyMilliHz % 1000000U) / 1000U));
            LCDText_drawAsciiString(616U, y, text,
                LCD_COLOR_YELLOW, LCD_COLOR_BLACK, 2U, 0U);
            (void)snprintf(text, sizeof(text),
                "   %4lu.%03lu Vpk",
                (unsigned long)(milliVoltsPeak / 1000U),
                (unsigned long)(milliVoltsPeak % 1000U));
            LCDText_drawAsciiString(616U, y + 17U, text,
                LCD_COLOR_GREEN, LCD_COLOR_BLACK, 2U, 0U);
        } else {
            (void)snprintf(text, sizeof(text), "H%u  ---.--- kHz",
                (unsigned int)(index + 1U));
            LCDText_drawAsciiString(616U, y, text,
                LCD_COLOR_GRAY, LCD_COLOR_BLACK, 2U, 0U);
            LCDText_drawAsciiString(616U, y + 17U, "    ---.--- Vpk",
                LCD_COLOR_GRAY, LCD_COLOR_BLACK, 2U, 0U);
        }
    }
}

static void ADC1FastVisualization_drawHarmonicMarkers(
    const SignalAnalyzer_Result *result,
    uint16_t binCount, uint32_t maximum)
{
    uint16_t index;
    char label[4];

    if ((result == 0) || (binCount < 3U) || (maximum == 0U)) {
        return;
    }
    for (index = 0U;
        (index < ADC1_VIS_DISPLAY_HARMONICS) &&
        (index < result->harmonicCount); index++) {
        const HarmonicAnalysis_Component *component =
            &result->harmonics[index];

        if (component->valid && (component->detectedBin > 0U) &&
            (component->detectedBin < binCount)) {
            uint16_t x = (uint16_t)(ADC1_VIS_INNER_X +
                ((uint32_t)(component->detectedBin - 1U) *
                    (ADC1_VIS_SPECTRUM_INNER_WIDTH - 1U)) /
                    (binCount - 2U));
            uint16_t height = (uint16_t)(
                ((uint64_t)component->amplitudeQ15 *
                    (ADC1_VIS_INNER_HEIGHT - 1U)) / maximum);
            uint16_t peakY;
            uint16_t labelX;
            uint16_t labelY;

            if (height >= ADC1_VIS_INNER_HEIGHT) {
                height = ADC1_VIS_INNER_HEIGHT - 1U;
            }
            peakY = (uint16_t)(ADC1_VIS_PLOT_BOTTOM - 1U - height);
            labelX = (x > ADC1_VIS_PLOT_X + 8U) ?
                (uint16_t)(x - 8U) : ADC1_VIS_INNER_X;
            if (labelX > ADC1_VIS_PLOT_X +
                ADC1_VIS_SPECTRUM_WIDTH - 18U) {
                labelX = ADC1_VIS_PLOT_X +
                    ADC1_VIS_SPECTRUM_WIDTH - 18U;
            }
            labelY = (peakY > ADC1_VIS_INNER_Y + 12U) ?
                (uint16_t)(peakY - 12U) : ADC1_VIS_INNER_Y;
            (void)snprintf(label, sizeof(label), "H%u",
                (unsigned int)(index + 1U));
            LCDText_drawAsciiString(labelX, labelY, label,
                LCD_COLOR_CYAN, LCD_COLOR_BLACK, 1U, 0U);
            gADC1VisMarkers[index].x = labelX;
            gADC1VisMarkers[index].y = labelY;
            gADC1VisMarkers[index].valid = true;
        }
    }
}

static void ADC1FastVisualization_eraseSpectrum(void)
{
    uint16_t x;
    uint16_t index;

    for (index = 0U; index < ADC1_VIS_DISPLAY_HARMONICS; index++) {
        if (gADC1VisMarkers[index].valid) {
            LCD8080_fillRect(gADC1VisMarkers[index].x,
                gADC1VisMarkers[index].y, 12U, 8U, LCD_COLOR_BLACK);
            gADC1VisMarkers[index].valid = false;
        }
    }
    for (x = 0U; x < ADC1_VIS_SPECTRUM_INNER_WIDTH; x++) {
        uint16_t height = gADC1VisSpectrumHeight[x];

        if (height != 0U) {
            LCD8080_fillRect(ADC1_VIS_INNER_X + x,
                ADC1_VIS_PLOT_BOTTOM - 1U - height,
                1U, height + 1U, LCD_COLOR_BLACK);
            gADC1VisSpectrumHeight[x] = 0U;
        }
    }
    ADC1FastVisualization_drawGridLines(
        false, ADC1_VIS_SPECTRUM_WIDTH);
}

static ADC1FastVisualization_WaveWindow
ADC1FastVisualization_findWaveWindow(
    const uint16_t *samples, uint16_t sampleCount,
    uint16_t *minimum, uint16_t *maximum)
{
    ADC1FastVisualization_WaveWindow window = { 0U, sampleCount, false };
    uint16_t index;
    uint16_t firstCrossing = 0U;
    uint16_t secondCrossing = 0U;
    uint16_t lowThreshold;
    uint16_t highThreshold;
    uint16_t hysteresis;
    bool armed = false;
    bool foundFirst = false;

    *minimum = ADC1_VIS_ADC_FULL_SCALE;
    *maximum = 0U;
    for (index = 0U; index < sampleCount; index++) {
        if (samples[index] < *minimum) {
            *minimum = samples[index];
        }
        if (samples[index] > *maximum) {
            *maximum = samples[index];
        }
    }

    if ((*maximum - *minimum) < ADC1_VIS_TRIGGER_MIN_RANGE) {
        return window;
    }

    hysteresis = (uint16_t)((*maximum - *minimum) / 16U);
    if (hysteresis < 2U) {
        hysteresis = 2U;
    }
    lowThreshold = (uint16_t)((*minimum + *maximum) / 2U - hysteresis);
    highThreshold = (uint16_t)((*minimum + *maximum) / 2U + hysteresis);

    for (index = 0U; index < sampleCount; index++) {
        if (samples[index] <= lowThreshold) {
            armed = true;
        } else if (armed && (samples[index] >= highThreshold)) {
            if (!foundFirst) {
                firstCrossing = index;
                foundFirst = true;
            } else {
                secondCrossing = index;
                break;
            }
            armed = false;
        }
    }

    if (secondCrossing > firstCrossing) {
        uint16_t period = (uint16_t)(secondCrossing - firstCrossing);
        uint16_t cycles = 3U;

        while ((cycles > 1U) &&
            ((uint32_t)firstCrossing +
                (uint32_t)period * cycles >= sampleCount)) {
            cycles--;
        }
        if (((uint32_t)firstCrossing + (uint32_t)period * cycles) <
            sampleCount) {
            window.start = firstCrossing;
            window.count = (uint16_t)(period * cycles + 1U);
            window.triggered = true;
        }
    }
    return window;
}

static void ADC1FastVisualization_drawInterpolatedWave(
    const uint16_t *samples,
    const ADC1FastVisualization_WaveWindow *window)
{
    uint16_t x;
    uint16_t previousY =
        ADC1FastVisualization_rawToScopeY(samples[window->start]);

    gADC1VisScopeTop[0] = previousY;
    gADC1VisScopeBottom[0] = previousY;
    LCD8080_fillRect(ADC1_VIS_INNER_X, previousY,
        1U, 1U, LCD_COLOR_CYAN);

    for (x = 1U; x < ADC1_VIS_INNER_WIDTH; x++) {
        uint32_t sourceNumerator =
            (uint32_t)x * (uint32_t)(window->count - 1U);
        uint16_t sourceIndex = (uint16_t)(sourceNumerator /
            (ADC1_VIS_INNER_WIDTH - 1U));
        uint16_t remainder = (uint16_t)(sourceNumerator %
            (ADC1_VIS_INNER_WIDTH - 1U));
        int32_t sample = samples[window->start + sourceIndex];

        if ((sourceIndex + 1U) < window->count) {
            int32_t delta =
                (int32_t)samples[window->start + sourceIndex + 1U] - sample;
            sample += (delta * remainder) /
                (int32_t)(ADC1_VIS_INNER_WIDTH - 1U);
        }

        uint16_t currentY =
            ADC1FastVisualization_rawToScopeY((uint16_t)sample);

        gADC1VisScopeTop[x] = (previousY < currentY) ?
            previousY : currentY;
        gADC1VisScopeBottom[x] = (previousY > currentY) ?
            previousY : currentY;
        /* 每列实际绘制范围与历史缓存完全一致，下一帧可无残留擦除。 */
        LCD8080_fillRect(ADC1_VIS_INNER_X + x,
            gADC1VisScopeTop[x], 1U,
            gADC1VisScopeBottom[x] - gADC1VisScopeTop[x] + 1U,
            LCD_COLOR_CYAN);
        previousY = currentY;
    }
    gADC1VisScopeValid = true;
}

static void ADC1FastVisualization_drawEnvelopeWave(
    const uint16_t *samples,
    const ADC1FastVisualization_WaveWindow *window)
{
    uint16_t x;

    for (x = 0U; x < ADC1_VIS_INNER_WIDTH; x++) {
        uint16_t first = (uint16_t)(((uint32_t)x * window->count) /
            ADC1_VIS_INNER_WIDTH);
        uint16_t end = (uint16_t)(((uint32_t)(x + 1U) * window->count) /
            ADC1_VIS_INNER_WIDTH);
        uint16_t index;
        uint16_t minimum;
        uint16_t maximum;

        if (end <= first) {
            end = (uint16_t)(first + 1U);
        }
        if (end > window->count) {
            end = window->count;
        }
        minimum = samples[window->start + first];
        maximum = minimum;
        for (index = (uint16_t)(first + 1U); index < end; index++) {
            uint16_t value = samples[window->start + index];
            if (value < minimum) {
                minimum = value;
            }
            if (value > maximum) {
                maximum = value;
            }
        }
        gADC1VisScopeTop[x] =
            ADC1FastVisualization_rawToScopeY(maximum);
        gADC1VisScopeBottom[x] =
            ADC1FastVisualization_rawToScopeY(minimum);
        LCD8080_fillRect(ADC1_VIS_INNER_X + x,
            gADC1VisScopeTop[x], 1U,
            gADC1VisScopeBottom[x] - gADC1VisScopeTop[x] + 1U,
            LCD_COLOR_CYAN);
    }
    gADC1VisScopeValid = true;
}

static void ADC1FastVisualization_eraseScope(void)
{
    uint16_t x;

    if (!gADC1VisScopeValid) {
        return;
    }
    for (x = 0U; x < ADC1_VIS_INNER_WIDTH; x++) {
        LCD8080_fillRect(ADC1_VIS_INNER_X + x,
            gADC1VisScopeTop[x], 1U,
            gADC1VisScopeBottom[x] - gADC1VisScopeTop[x] + 1U,
            LCD_COLOR_BLACK);
    }
    ADC1FastVisualization_drawGridLines(true, ADC1_VIS_PLOT_WIDTH);
    gADC1VisScopeValid = false;
}

void ADC1FastVisualization_drawScope(
    const uint16_t *samples,
    uint16_t sampleCount,
    uint32_t sampleRateHz,
    const SignalAnalyzer_Result *result,
    uint32_t overrunCount)
{
    ADC1FastVisualization_WaveWindow window;
    uint16_t minimum;
    uint16_t maximum;
    uint32_t peakToPeakMilliVolts;
    char text[64];

    if ((samples == 0) || (sampleCount < 2U)) {
        return;
    }

    window = ADC1FastVisualization_findWaveWindow(
        samples, sampleCount, &minimum, &maximum);
    (void)result;
    peakToPeakMilliVolts =
        ((uint32_t)(maximum - minimum) * ADC1_VIS_ADC_REFERENCE_MV +
            ADC1_VIS_ADC_FULL_SCALE / 2U) /
        ADC1_VIS_ADC_FULL_SCALE;

    if (gADC1VisPage != ADC1_VIS_PAGE_SCOPE) {
        gADC1VisPage = ADC1_VIS_PAGE_SCOPE;
        gADC1VisScopeValid = false;
        ADC1FastVisualization_drawHeaderStatic(
            "OSCILLOSCOPE", sampleRateHz);
        ADC1FastVisualization_drawGrid(true);
    } else {
        ADC1FastVisualization_eraseScope();
    }
    ADC1FastVisualization_drawOverrunCount(overrunCount);
    if (window.count <= ADC1_VIS_INNER_WIDTH) {
        ADC1FastVisualization_drawInterpolatedWave(samples, &window);
    } else {
        ADC1FastVisualization_drawEnvelopeWave(samples, &window);
    }

    (void)snprintf(text, sizeof(text),
        "P-P:%lu.%03lu V  TRIG:%s",
        (unsigned long)(peakToPeakMilliVolts / 1000U),
        (unsigned long)(peakToPeakMilliVolts % 1000U),
        window.triggered ? "OK" : "FREE");
    LCD8080_fillRect(12U, 442U, 400U, 36U, LCD_COLOR_BLACK);
    LCDText_drawAsciiString(20U, 447U, text,
        LCD_COLOR_GREEN, LCD_COLOR_BLACK, 2U, 0U);
}

void ADC1FastVisualization_drawSpectrum(
    const SignalAnalyzer *analyzer,
    uint32_t sampleRateHz,
    const SignalAnalyzer_Result *result,
    uint32_t overrunCount)
{
    const uint32_t *amplitudes = 0;
    uint16_t binCount = 0U;
    uint16_t bin;
    uint32_t maximum = 0U;
    char text[64];

    if (gADC1VisPage != ADC1_VIS_PAGE_SPECTRUM) {
        uint16_t index;

        gADC1VisPage = ADC1_VIS_PAGE_SPECTRUM;
        for (index = 0U;
            index < ADC1_VIS_SPECTRUM_INNER_WIDTH; index++) {
            gADC1VisSpectrumHeight[index] = 0U;
        }
        for (index = 0U;
            index < ADC1_VIS_DISPLAY_HARMONICS; index++) {
            gADC1VisMarkers[index].valid = false;
        }
        ADC1FastVisualization_drawHeaderStatic(
            "SPECTRUM  0..Fs/2", sampleRateHz);
        ADC1FastVisualization_drawGrid(false);
        ADC1FastVisualization_drawHarmonicPanelStatic();
    } else {
        ADC1FastVisualization_eraseSpectrum();
    }
    ADC1FastVisualization_drawOverrunCount(overrunCount);

    if ((analyzer != 0) &&
        SignalAnalyzer_getAmplitudeSpectrum(
            analyzer, &amplitudes, &binCount) &&
        (binCount >= 3U)) {
        for (bin = 1U; bin < binCount; bin++) {
            if (amplitudes[bin] > maximum) {
                maximum = amplitudes[bin];
            }
        }
        if (maximum != 0U) {
            for (bin = 1U; bin < binCount; bin++) {
                uint16_t x = (uint16_t)(ADC1_VIS_INNER_X +
                    ((uint32_t)(bin - 1U) *
                        (ADC1_VIS_SPECTRUM_INNER_WIDTH - 1U)) /
                        (binCount - 2U));
                uint16_t height = (uint16_t)(
                    ((uint64_t)amplitudes[bin] *
                        (ADC1_VIS_INNER_HEIGHT - 1U)) / maximum);

                if (height != 0U) {
                    LCD8080_fillRect(x,
                        ADC1_VIS_PLOT_BOTTOM - 1U - height,
                        1U, height + 1U, LCD_COLOR_YELLOW);
                    if (height >
                        gADC1VisSpectrumHeight[x - ADC1_VIS_INNER_X]) {
                        gADC1VisSpectrumHeight[x - ADC1_VIS_INNER_X] =
                            height;
                    }
                }
            }
        }
    }

    ADC1FastVisualization_drawHarmonicMarkers(
        result, binCount, maximum);
    ADC1FastVisualization_drawHarmonicPanel(result);

    if ((result != 0) && result->thd.valid) {
        (void)snprintf(text, sizeof(text),
            "THD:%lu.%03lu%%",
            (unsigned long)(result->thd.thdMilliPercent / 1000U),
            (unsigned long)(result->thd.thdMilliPercent % 1000U));
    } else {
        (void)snprintf(text, sizeof(text), "THD:---");
    }
    LCD8080_fillRect(12U, 442U, 240U, 36U, LCD_COLOR_BLACK);
    LCDText_drawAsciiString(20U, 447U, text,
        LCD_COLOR_GREEN, LCD_COLOR_BLACK, 2U, 0U);
}
