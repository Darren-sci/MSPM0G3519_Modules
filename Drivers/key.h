#ifndef DRIVERS_KEY_H_
#define DRIVERS_KEY_H_

#include <stdbool.h>

/* 12 个按键的逻辑编号，调用时不需要关心对应的 GPIO 引脚。 */
typedef enum
{
    KEY_1 = 0,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,
    KEY_10,
    KEY_11,
    KEY_12,
    KEY_COUNT
} KeyId;

/**
 * 判断指定按键是否完成了一次单击。
 *
 * 本函数需要放在主循环中反复调用。按下并稳定释放后仅返回一次 true，
 * 按住期间以及没有新单击时返回 false。
 *
 * @param key 需要判断的按键编号，例如 KEY_1。
 * @return 完成一次单击时返回 true，否则返回 false。
 */
bool Key_wasClicked(KeyId key);

#endif
