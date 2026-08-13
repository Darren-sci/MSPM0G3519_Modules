#include "Graphics/adc_visualization.h"

#include <stdio.h>

#include "Drivers/lcd_8080.h"
#include "Graphics/lcd_text.h"

#define ADC_VIS_ADC_FULL_SCALE          (4095U)

#define ADC_VIS_PANEL_X                 (30U)
#define ADC_VIS_PANEL_Y                 (175U)
#define ADC_VIS_PANEL_WIDTH             (600U)
#define ADC_VIS_PANEL_HEIGHT            (150U)

#define ADC_VIS_TEXT_X                  (40U)
#define ADC_VIS_TEXT_Y_AVERAGE          (185U)
#define ADC_VIS_TEXT_Y_VOLTAGE          (220U)
#define ADC_VIS_TEXT_Y_RANGE            (255U)
#define ADC_VIS_TEXT_Y_PEAK_TO_PEAK     (290U)
#define ADC_VIS_TEXT_SCALE              (2U)

static uint32_t ADCVisualization_codeToMillivolts(uint32_t adcCode)
{
    /* 除法前加上除数的一半，使整数除法结果按四舍五入取整。 */
    return (adcCode * ADC_VISUALIZATION_REFERENCE_MV +
               (ADC_VIS_ADC_FULL_SCALE / 2U)) /
           ADC_VIS_ADC_FULL_SCALE;
}

static void ADCVisualization_drawLine(
    uint16_t y, const char *text, uint16_t color)
{
    LCDText_drawAsciiString(ADC_VIS_TEXT_X, y, text, color,
        LCD_COLOR_BLACK, ADC_VIS_TEXT_SCALE, 0U);
}

void ADCVisualization_drawData(const uint16_t *samples, uint32_t count)
{
    uint16_t minimum;
    uint16_t maximum;
    uint16_t average;
    uint32_t sum = 0U;
    uint32_t averageMillivolts;
    uint32_t peakToPeakMillivolts;
    uint32_t i;
    char text[48];

    if ((samples == NULL) || (count == 0U)) {
        LCD8080_fillRect(ADC_VIS_PANEL_X, ADC_VIS_PANEL_Y,
            ADC_VIS_PANEL_WIDTH, ADC_VIS_PANEL_HEIGHT, LCD_COLOR_BLACK);
        ADCVisualization_drawLine(
            ADC_VIS_TEXT_Y_AVERAGE, "ADC DATA: NO SAMPLES", LCD_COLOR_RED);
        return;
    }

    minimum = samples[0];
    maximum = samples[0];

    for (i = 0U; i < count; i++) {
        uint16_t value = samples[i];

        sum += value;
        if (value < minimum) {
            minimum = value;
        }
        if (value > maximum) {
            maximum = value;
        }
    }

    average = (uint16_t)((sum + (count / 2U)) / count);
    averageMillivolts = ADCVisualization_codeToMillivolts(average);
    peakToPeakMillivolts =
        ADCVisualization_codeToMillivolts((uint32_t)maximum - minimum);

    /* 清除完整的数据显示区域，防止新字符串较短时残留旧字符。 */
    LCD8080_fillRect(ADC_VIS_PANEL_X, ADC_VIS_PANEL_Y,
        ADC_VIS_PANEL_WIDTH, ADC_VIS_PANEL_HEIGHT, LCD_COLOR_BLACK);

    (void)snprintf(text, sizeof(text), "ADC AVG: %u",
        (unsigned int)average);
    ADCVisualization_drawLine(
        ADC_VIS_TEXT_Y_AVERAGE, text, LCD_COLOR_YELLOW);

    (void)snprintf(text, sizeof(text), "VOLTAGE: %lu.%03lu V",
        (unsigned long)(averageMillivolts / 1000U),
        (unsigned long)(averageMillivolts % 1000U));
    ADCVisualization_drawLine(
        ADC_VIS_TEXT_Y_VOLTAGE, text, LCD_COLOR_GREEN);

    (void)snprintf(text, sizeof(text), "MIN: %u  MAX: %u",
        (unsigned int)minimum, (unsigned int)maximum);
    ADCVisualization_drawLine(
        ADC_VIS_TEXT_Y_RANGE, text, LCD_COLOR_CYAN);

    (void)snprintf(text, sizeof(text), "VPP: %lu.%03lu V",
        (unsigned long)(peakToPeakMillivolts / 1000U),
        (unsigned long)(peakToPeakMillivolts % 1000U));
    ADCVisualization_drawLine(
        ADC_VIS_TEXT_Y_PEAK_TO_PEAK, text, LCD_COLOR_MAGENTA);
}
