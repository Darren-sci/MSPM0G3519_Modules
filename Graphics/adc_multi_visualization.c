#include "Graphics/adc_multi_visualization.h"

#include <stdio.h>

#include "Drivers/lcd_8080.h"
#include "Graphics/lcd_text.h"

#define ADC_MULTI_VIS_REFERENCE_MV    (3300U)
#define ADC_MULTI_VIS_FULL_SCALE      (4095U)

#define ADC_MULTI_VIS_X               (30U)
#define ADC_MULTI_VIS_Y               (80U)
#define ADC_MULTI_VIS_WIDTH           (790U)
#define ADC_MULTI_VIS_HEIGHT          (310U)
#define ADC_MULTI_VIS_LINE_HEIGHT     (42U)
#define ADC_MULTI_VIS_TEXT_SCALE      (2U)

static uint32_t ADCMultiVisualization_toMillivolts(uint32_t code)
{
    return (code * ADC_MULTI_VIS_REFERENCE_MV +
        ADC_MULTI_VIS_FULL_SCALE / 2U) / ADC_MULTI_VIS_FULL_SCALE;
}

static void ADCMultiVisualization_drawLine(
    uint16_t line, const char *text, uint16_t color)
{
    LCDText_drawAsciiString(ADC_MULTI_VIS_X + 10U,
        (uint16_t)(ADC_MULTI_VIS_Y + 10U + line * ADC_MULTI_VIS_LINE_HEIGHT),
        text, color, LCD_COLOR_BLACK, ADC_MULTI_VIS_TEXT_SCALE, 0U);
}

void ADCMultiVisualization_draw(
    const ADCMulti_Frame *frames,
    uint16_t frameCount,
    uint32_t frameRateHz,
    uint32_t overrunCount)
{
    static const char *const names[ADC_MULTI_CHANNEL_COUNT] = {
        "VIN ", "IIN ", "VOUT", "IOUT"
    };
    uint32_t sums[ADC_MULTI_CHANNEL_COUNT] = {0U, 0U, 0U, 0U};
    uint16_t i;
    uint8_t channel;
    char text[56];

    if ((frames == 0) || (frameCount == 0U)) {
        ADCMultiVisualization_drawError("ADC: NO DATA");
        return;
    }

    for (i = 0U; i < frameCount; i++) {
        sums[ADC_MULTI_VIN]  += frames[i].vin;
        sums[ADC_MULTI_IIN]  += frames[i].iin;
        sums[ADC_MULTI_VOUT] += frames[i].vout;
        sums[ADC_MULTI_IOUT] += frames[i].iout;
    }

    LCD8080_fillRect(ADC_MULTI_VIS_X, ADC_MULTI_VIS_Y,
        ADC_MULTI_VIS_WIDTH, ADC_MULTI_VIS_HEIGHT, LCD_COLOR_BLACK);

    for (channel = 0U; channel < ADC_MULTI_CHANNEL_COUNT; channel++) {
        uint32_t average = (sums[channel] + frameCount / 2U) / frameCount;
        uint32_t millivolts =
            ADCMultiVisualization_toMillivolts(average);

        (void)snprintf(text, sizeof(text),
            "%s  ADC:%4lu  %lu.%03lu V",
            names[channel],
            (unsigned long)average,
            (unsigned long)(millivolts / 1000U),
            (unsigned long)(millivolts % 1000U));
        ADCMultiVisualization_drawLine(channel, text, LCD_COLOR_CYAN);
    }

    (void)snprintf(text, sizeof(text), "RATE: %lu frame/s",
        (unsigned long)frameRateHz);
    ADCMultiVisualization_drawLine(4U, text, LCD_COLOR_GREEN);

    (void)snprintf(text, sizeof(text), "OVERRUN: %lu",
        (unsigned long)overrunCount);
    ADCMultiVisualization_drawLine(5U, text,
        (overrunCount == 0U) ? LCD_COLOR_GREEN : LCD_COLOR_YELLOW);
}

void ADCMultiVisualization_drawError(const char *message)
{
    LCD8080_fillRect(ADC_MULTI_VIS_X, ADC_MULTI_VIS_Y,
        ADC_MULTI_VIS_WIDTH, ADC_MULTI_VIS_HEIGHT, LCD_COLOR_BLACK);
    ADCMultiVisualization_drawLine(0U,
        (message != 0) ? message : "ADC: ERROR", LCD_COLOR_RED);
}
