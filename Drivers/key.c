#include "Drivers/key.h"

#include <stdint.h>

#include "ti_msp_dl_config.h"

/* 按键机械抖动通常持续数毫秒，这里按 20 ms 进行二次确认。 */
#define KEY_DEBOUNCE_TIME_MS    (20U)
#define KEY_DEBOUNCE_CYCLES     \
    ((CPUCLK_FREQ / 1000U) * KEY_DEBOUNCE_TIME_MS)

/* 逻辑按键编号与 SysConfig 生成的 GPIO 引脚一一对应。 */
static const uint32_t gKeyPins[KEY_COUNT] = {
    KEYS_KEY1_PIN,
    KEYS_KEY2_PIN,
    KEYS_KEY3_PIN,
    KEYS_KEY4_PIN,
    KEYS_KEY5_PIN,
    KEYS_KEY6_PIN,
    KEYS_KEY7_PIN,
    KEYS_KEY8_PIN,
    KEYS_KEY9_PIN,
    KEYS_KEY10_PIN,
    KEYS_KEY11_PIN,
    KEYS_KEY12_PIN
};

/* 记录各按键是否已经确认按下，用于在释放时只产生一次单击事件。 */
static bool gKeyWasPressed[KEY_COUNT];

/*
 * 按键使用内部上拉并连接到 GND：低电平表示按下，高电平表示释放。
 */
static bool Key_isPressed(KeyId key)
{
    return DL_GPIO_readPins(KEYS_PORT, gKeyPins[key]) == 0U;
}

bool Key_wasClicked(KeyId key)
{
    /* 防止错误编号导致数组越界。 */
    if ((uint32_t)key >= (uint32_t)KEY_COUNT) {
        return false;
    }

    if (!gKeyWasPressed[key]) {
        if (!Key_isPressed(key)) {
            return false;
        }

        /* 首次检测到按下，延时后再次确认，滤除按下抖动。 */
        delay_cycles(KEY_DEBOUNCE_CYCLES);
        if (Key_isPressed(key)) {
            gKeyWasPressed[key] = true;
        }

        return false;
    }

    if (Key_isPressed(key)) {
        return false;
    }

    /* 检测到释放，延时后再次确认，滤除释放抖动。 */
    delay_cycles(KEY_DEBOUNCE_CYCLES);
    if (!Key_isPressed(key)) {
        gKeyWasPressed[key] = false;
        return true;
    }

    return false;
}



//功能实例
// while (1) {
//     if (Key_wasClicked(KEY_1)) {
//         /* KEY1单击功能 */
//     }

//     if (Key_wasClicked(KEY_2)) {
//         /* KEY2单击功能 */
//     }
// }