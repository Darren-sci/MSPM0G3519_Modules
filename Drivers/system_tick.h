#ifndef DRIVERS_SYSTEM_TICK_H_
#define DRIVERS_SYSTEM_TICK_H_

#include <stdbool.h>
#include <stdint.h>

/* 启动由 SysConfig 配置好的 1 ms 系统节拍定时器。 */
void SystemTick_start(void);

/* 返回系统节拍启动后累计的毫秒数。 */
uint32_t SystemTick_getMs(void);

/**
 * 判断一个软件任务是否到达执行周期。
 *
 * 每个任务必须使用自己独立的 lastTimeMs 变量，并将该变量静态初始化为 0。
 * 到达周期时返回一次 true，同时自动更新该任务的上次执行时间。
 *
 * @param lastTimeMs 任务上次执行时间变量的地址。
 * @param periodMs   任务执行周期，单位为毫秒；0 表示禁用该任务。
 * @return 到达执行周期时返回 true，否则返回 false。
 */
bool SystemTick_isDue(uint32_t *lastTimeMs, uint32_t periodMs);

#endif
