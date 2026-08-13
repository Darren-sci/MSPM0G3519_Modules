#ifndef GRAPHICS_ADC_VISUALIZATION_H_
#define GRAPHICS_ADC_VISUALIZATION_H_

#include <stdint.h>

/*
 * ADC_CAPTURE使用VDDA/VSSA作为参考电压。3300mV是开发板供电的标称值；
 * 如果需要提高绝对电压测量精度，应将此数值替换为实际测得的VDDA电压。
 */
#define ADC_VISUALIZATION_REFERENCE_MV    (3300U)

/**
 * @brief 计算并显示一批ADC采样数据的统计结果。
 *
 * 函数会显示平均ADC值、平均值对应的电压、最小值、最大值以及峰峰电压。
 * 调用本函数前必须先通过LCDPanel_init()完成LCD初始化。
 *
 * @param samples 指向无符号12位ADC采样数组的指针。
 * @param count samples数组中的有效采样数量。
 */
void ADCVisualization_drawData(const uint16_t *samples, uint32_t count);

#endif
