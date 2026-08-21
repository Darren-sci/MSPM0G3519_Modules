#include "Drivers/oled_ssd1315.h"

#include "ti_msp_dl_config.h"

/* SSD1315四针I2C模块通常使用7位地址0x3C。 */
#define OLED_I2C_ADDRESS             (0x3CU)
#define OLED_CONTROL_COMMAND         (0x00U)
#define OLED_CONTROL_DATA            (0x40U)
#define OLED_I2C_FIFO_SIZE           (8U)
#define OLED_I2C_TIMEOUT_LOOPS       (1000000U)
#define OLED_DEFAULT_CONTRAST        (0x5FU)

/* 128x64单色显存正好占用1024字节，每个page对应垂直8个像素。 */
static uint8_t gOledBuffer[OLED_PAGE_COUNT][OLED_WIDTH];
static uint8_t gOledDirtyPages;
static bool gOledReady;

/*
 * 5x7 ASCII字库，按列存储，最低位对应最上方像素。
 * 绘制时额外补1列空白，因此1倍字符占用6x8像素。
 */
static const uint8_t gAscii5x7[96][5] = {
    {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x5F,0x00,0x00},
    {0x00,0x07,0x00,0x07,0x00}, {0x14,0x7F,0x14,0x7F,0x14},
    {0x24,0x2A,0x7F,0x2A,0x12}, {0x23,0x13,0x08,0x64,0x62},
    {0x36,0x49,0x55,0x22,0x50}, {0x00,0x05,0x03,0x00,0x00},
    {0x00,0x1C,0x22,0x41,0x00}, {0x00,0x41,0x22,0x1C,0x00},
    {0x14,0x08,0x3E,0x08,0x14}, {0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00}, {0x08,0x08,0x08,0x08,0x08},
    {0x00,0x60,0x60,0x00,0x00}, {0x20,0x10,0x08,0x04,0x02},
    {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1E},
    {0x00,0x36,0x36,0x00,0x00}, {0x00,0x56,0x36,0x00,0x00},
    {0x08,0x14,0x22,0x41,0x00}, {0x14,0x14,0x14,0x14,0x14},
    {0x00,0x41,0x22,0x14,0x08}, {0x02,0x01,0x51,0x09,0x06},
    {0x32,0x49,0x79,0x41,0x3E}, {0x7E,0x11,0x11,0x11,0x7E},
    {0x7F,0x49,0x49,0x49,0x36}, {0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C}, {0x7F,0x49,0x49,0x49,0x41},
    {0x7F,0x09,0x09,0x09,0x01}, {0x3E,0x41,0x49,0x49,0x7A},
    {0x7F,0x08,0x08,0x08,0x7F}, {0x00,0x41,0x7F,0x41,0x00},
    {0x20,0x40,0x41,0x3F,0x01}, {0x7F,0x08,0x14,0x22,0x41},
    {0x7F,0x40,0x40,0x40,0x40}, {0x7F,0x02,0x0C,0x02,0x7F},
    {0x7F,0x04,0x08,0x10,0x7F}, {0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06}, {0x3E,0x41,0x51,0x21,0x5E},
    {0x7F,0x09,0x19,0x29,0x46}, {0x46,0x49,0x49,0x49,0x31},
    {0x01,0x01,0x7F,0x01,0x01}, {0x3F,0x40,0x40,0x40,0x3F},
    {0x1F,0x20,0x40,0x20,0x1F}, {0x3F,0x40,0x38,0x40,0x3F},
    {0x63,0x14,0x08,0x14,0x63}, {0x07,0x08,0x70,0x08,0x07},
    {0x61,0x51,0x49,0x45,0x43}, {0x00,0x7F,0x41,0x41,0x00},
    {0x02,0x04,0x08,0x10,0x20}, {0x00,0x41,0x41,0x7F,0x00},
    {0x04,0x02,0x01,0x02,0x04}, {0x40,0x40,0x40,0x40,0x40},
    {0x00,0x01,0x02,0x04,0x00}, {0x20,0x54,0x54,0x54,0x78},
    {0x7F,0x48,0x44,0x44,0x38}, {0x38,0x44,0x44,0x44,0x20},
    {0x38,0x44,0x44,0x48,0x7F}, {0x38,0x54,0x54,0x54,0x18},
    {0x08,0x7E,0x09,0x01,0x02}, {0x0C,0x52,0x52,0x52,0x3E},
    {0x7F,0x08,0x04,0x04,0x78}, {0x00,0x44,0x7D,0x40,0x00},
    {0x20,0x40,0x44,0x3D,0x00}, {0x7F,0x10,0x28,0x44,0x00},
    {0x00,0x41,0x7F,0x40,0x00}, {0x7C,0x04,0x18,0x04,0x78},
    {0x7C,0x08,0x04,0x04,0x78}, {0x38,0x44,0x44,0x44,0x38},
    {0x7C,0x14,0x14,0x14,0x08}, {0x08,0x14,0x14,0x18,0x7C},
    {0x7C,0x08,0x04,0x04,0x08}, {0x48,0x54,0x54,0x54,0x20},
    {0x04,0x3F,0x44,0x40,0x20}, {0x3C,0x40,0x40,0x20,0x7C},
    {0x1C,0x20,0x40,0x20,0x1C}, {0x3C,0x40,0x30,0x40,0x3C},
    {0x44,0x28,0x10,0x28,0x44}, {0x0C,0x50,0x50,0x50,0x3C},
    {0x44,0x64,0x54,0x4C,0x44}, {0x00,0x08,0x36,0x41,0x00},
    {0x00,0x00,0x7F,0x00,0x00}, {0x00,0x41,0x36,0x08,0x00},
    {0x10,0x08,0x08,0x10,0x08}, {0x00,0x06,0x09,0x09,0x06}
};

static bool OLED_waitControllerIdle(void)
{
    uint32_t timeout = OLED_I2C_TIMEOUT_LOOPS;

    while ((DL_I2C_getControllerStatus(OLED_I2C_INST) &
            DL_I2C_CONTROLLER_STATUS_IDLE) == 0U) {
        if (timeout-- == 0U) {
            return false;
        }
    }
    return true;
}

/*
 * 发送一个控制字节和后续负载。发送期间持续填充8字节硬件FIFO，因此
 * 刷新一页128字节数据时只产生一次START和STOP。
 */
static bool OLED_write(uint8_t control, const uint8_t *data, uint16_t length)
{
    uint16_t written;
    uint16_t initialCount;
    uint32_t timeout;
    uint32_t status;

    if ((data == 0) && (length != 0U)) {
        return false;
    }
    if (!OLED_waitControllerIdle()) {
        gOledReady = false;
        return false;
    }

    DL_I2C_flushControllerTXFIFO(OLED_I2C_INST);
    DL_I2C_transmitControllerData(OLED_I2C_INST, control);

    initialCount = length;
    if (initialCount > (OLED_I2C_FIFO_SIZE - 1U)) {
        initialCount = OLED_I2C_FIFO_SIZE - 1U;
    }
    written = DL_I2C_fillControllerTXFIFO(
        OLED_I2C_INST, data, initialCount);

    DL_I2C_startControllerTransfer(OLED_I2C_INST, OLED_I2C_ADDRESS,
        DL_I2C_CONTROLLER_DIRECTION_TX, (uint16_t)(length + 1U));

    /* MSPM0 I2C_ERR_13要求启动传输后等待至少3个I2C功能时钟。 */
    delay_cycles(16U);

    timeout = OLED_I2C_TIMEOUT_LOOPS;
    while (written < length) {
        status = DL_I2C_getControllerStatus(OLED_I2C_INST);
        if ((status & DL_I2C_CONTROLLER_STATUS_ERROR) != 0U) {
            DL_I2C_resetControllerTransfer(OLED_I2C_INST);
            DL_I2C_flushControllerTXFIFO(OLED_I2C_INST);
            gOledReady = false;
            return false;
        }
        if (!DL_I2C_isControllerTXFIFOFull(OLED_I2C_INST)) {
            DL_I2C_transmitControllerData(OLED_I2C_INST, data[written]);
            written++;
        } else if (timeout-- == 0U) {
            DL_I2C_resetControllerTransfer(OLED_I2C_INST);
            DL_I2C_flushControllerTXFIFO(OLED_I2C_INST);
            gOledReady = false;
            return false;
        }
    }

    timeout = OLED_I2C_TIMEOUT_LOOPS;
    while ((DL_I2C_getControllerStatus(OLED_I2C_INST) &
            DL_I2C_CONTROLLER_STATUS_BUSY) != 0U) {
        if (timeout-- == 0U) {
            DL_I2C_resetControllerTransfer(OLED_I2C_INST);
            DL_I2C_flushControllerTXFIFO(OLED_I2C_INST);
            gOledReady = false;
            return false;
        }
    }

    status = DL_I2C_getControllerStatus(OLED_I2C_INST);
    if ((status & DL_I2C_CONTROLLER_STATUS_ERROR) != 0U) {
        DL_I2C_resetControllerTransfer(OLED_I2C_INST);
        DL_I2C_flushControllerTXFIFO(OLED_I2C_INST);
        gOledReady = false;
        return false;
    }
    return true;
}

static bool OLED_writeCommands(const uint8_t *commands, uint16_t count)
{
    return OLED_write(OLED_CONTROL_COMMAND, commands, count);
}

static uint8_t OLED_fontScale(OLED_Font font)
{
    uint8_t scale = (uint8_t)font;

    if ((scale < 1U) || (scale > 3U)) {
        scale = 1U;
    }
    return scale;
}

bool OLED_Init(void)
{
    /*
     * 初始化参数沿用原OLED程序的128x64、页寻址和内部电荷泵方案，
     * 默认对比度降低到0x5F，兼顾可读性与功耗。
     */
    static const uint8_t initCommands[] = {
        0xAEU,             /* 关闭显示，初始化期间避免花屏 */
        0xD5U, 0x80U,      /* 显示时钟分频 */
        0xA8U, 0x3FU,      /* 1/64复用率 */
        0xD3U, 0x00U,      /* 显示偏移为0 */
        0x40U,             /* 显示起始行为0 */
        0x8DU, 0x14U,      /* 开启内部电荷泵 */
        0x20U, 0x02U,      /* 页寻址模式 */
        0xA1U,             /* SEG左右映射 */
        0xC8U,             /* COM上下扫描方向 */
        0xDAU, 0x12U,      /* 128x64 COM引脚配置 */
        0x81U, OLED_DEFAULT_CONTRAST,
        0xD9U, 0xF1U,      /* 预充电周期 */
        0xDBU, 0x40U,      /* VCOMH电平 */
        0xA4U,             /* 按显存内容显示 */
        0xA6U,             /* 正常色 */
        0x2EU              /* 关闭滚动 */
    };

    gOledReady = false;
    gOledDirtyPages = 0U;
    delay_cycles(CPUCLK_FREQ / 20U); /* 上电后等待约50ms */

    if (!OLED_writeCommands(initCommands, sizeof(initCommands))) {
        return false;
    }

    gOledReady = true;
    OLED_ClearBuffer();
    if (!OLED_Refresh()) {
        return false;
    }
    return OLED_DisplayOn();
}

void OLED_ClearBuffer(void)
{
    uint8_t page;
    uint8_t column;

    for (page = 0U; page < OLED_PAGE_COUNT; page++) {
        for (column = 0U; column < OLED_WIDTH; column++) {
            gOledBuffer[page][column] = 0U;
        }
    }
    gOledDirtyPages = (uint8_t)((1U << OLED_PAGE_COUNT) - 1U);
}

bool OLED_ClearScreen(void)
{
    OLED_ClearBuffer();
    return OLED_Refresh();
}

void OLED_ClearArea(uint8_t x, uint8_t y, uint8_t width, uint8_t height)
{
    uint16_t xEnd = (uint16_t)x + width;
    uint16_t yEnd = (uint16_t)y + height;
    uint16_t drawX;
    uint16_t drawY;

    if (xEnd > OLED_WIDTH) {
        xEnd = OLED_WIDTH;
    }
    if (yEnd > OLED_HEIGHT) {
        yEnd = OLED_HEIGHT;
    }
    for (drawY = y; drawY < yEnd; drawY++) {
        for (drawX = x; drawX < xEnd; drawX++) {
            OLED_DrawPoint((uint8_t)drawX, (uint8_t)drawY, false);
        }
    }
}

void OLED_DrawPoint(uint8_t x, uint8_t y, bool on)
{
    uint8_t page;
    uint8_t mask;
    uint8_t oldValue;

    if ((x >= OLED_WIDTH) || (y >= OLED_HEIGHT)) {
        return;
    }
    page = y / 8U;
    mask = (uint8_t)(1U << (y & 7U));
    oldValue = gOledBuffer[page][x];
    if (on) {
        gOledBuffer[page][x] |= mask;
    } else {
        gOledBuffer[page][x] &= (uint8_t)~mask;
    }
    if (oldValue != gOledBuffer[page][x]) {
        gOledDirtyPages |= (uint8_t)(1U << page);
    }
}

void OLED_DrawLine(
    uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, bool on)
{
    int16_t x = x1;
    int16_t y = y1;
    int16_t endX = x2;
    int16_t endY = y2;
    int16_t deltaX = (endX >= x) ? (endX - x) : (x - endX);
    int16_t stepX = (x < endX) ? 1 : -1;
    int16_t deltaY = (endY >= y) ? (y - endY) : (endY - y);
    int16_t stepY = (y < endY) ? 1 : -1;
    int16_t error = deltaX + deltaY;

    while (true) {
        OLED_DrawPoint((uint8_t)x, (uint8_t)y, on);
        if ((x == endX) && (y == endY)) {
            break;
        }
        if ((int16_t)(2 * error) >= deltaY) {
            error += deltaY;
            x += stepX;
        }
        if ((int16_t)(2 * error) <= deltaX) {
            error += deltaX;
            y += stepY;
        }
    }
}

void OLED_DrawRectangle(uint8_t x, uint8_t y,
    uint8_t width, uint8_t height, bool filled)
{
    uint16_t right;
    uint16_t bottom;
    uint16_t drawX;
    uint16_t drawY;

    if ((width == 0U) || (height == 0U)) {
        return;
    }
    right = (uint16_t)x + width - 1U;
    bottom = (uint16_t)y + height - 1U;
    if (right >= OLED_WIDTH) {
        right = OLED_WIDTH - 1U;
    }
    if (bottom >= OLED_HEIGHT) {
        bottom = OLED_HEIGHT - 1U;
    }

    if (filled) {
        for (drawY = y; drawY <= bottom; drawY++) {
            for (drawX = x; drawX <= right; drawX++) {
                OLED_DrawPoint((uint8_t)drawX, (uint8_t)drawY, true);
            }
        }
    } else {
        OLED_DrawLine(x, y, (uint8_t)right, y, true);
        OLED_DrawLine(x, (uint8_t)bottom,
            (uint8_t)right, (uint8_t)bottom, true);
        OLED_DrawLine(x, y, x, (uint8_t)bottom, true);
        OLED_DrawLine((uint8_t)right, y,
            (uint8_t)right, (uint8_t)bottom, true);
    }
}

void OLED_ShowChar(uint8_t x, uint8_t y, char character, OLED_Font font)
{
    uint8_t scale = OLED_fontScale(font);
    uint8_t index;
    uint8_t column;
    uint8_t row;
    uint8_t scaleX;
    uint8_t scaleY;

    if (((uint8_t)character < 0x20U) || ((uint8_t)character > 0x7EU)) {
        character = '?';
    }
    index = (uint8_t)character - 0x20U;

    for (column = 0U; column < 6U; column++) {
        uint8_t bits = (column < 5U) ? gAscii5x7[index][column] : 0U;
        for (row = 0U; row < 8U; row++) {
            bool pixelOn = ((bits >> row) & 1U) != 0U;
            for (scaleY = 0U; scaleY < scale; scaleY++) {
                for (scaleX = 0U; scaleX < scale; scaleX++) {
                    uint16_t drawX = (uint16_t)x + column * scale + scaleX;
                    uint16_t drawY = (uint16_t)y + row * scale + scaleY;
                    if ((drawX < OLED_WIDTH) && (drawY < OLED_HEIGHT)) {
                        OLED_DrawPoint(
                            (uint8_t)drawX, (uint8_t)drawY, pixelOn);
                    }
                }
            }
        }
    }
}

void OLED_ShowString(
    uint8_t x, uint8_t y, const char *string, OLED_Font font)
{
    uint8_t scale = OLED_fontScale(font);
    uint16_t cursorX = x;
    uint16_t cursorY = y;

    if (string == 0) {
        return;
    }
    while ((*string != '\0') && (cursorY < OLED_HEIGHT)) {
        if (*string == '\n') {
            cursorX = x;
            cursorY += 8U * scale;
        } else {
            if (cursorX < OLED_WIDTH) {
                OLED_ShowChar(
                    (uint8_t)cursorX, (uint8_t)cursorY, *string, font);
            }
            cursorX += 6U * scale;
        }
        string++;
    }
}

void OLED_ShowUInt(uint8_t x, uint8_t y,
    uint32_t value, uint8_t digits, OLED_Font font)
{
    char reversed[10];
    char text[11];
    uint8_t count = 0U;
    uint8_t index;

    if (digits > 10U) {
        digits = 10U;
    }
    do {
        reversed[count++] = (char)('0' + (value % 10U));
        value /= 10U;
    } while ((value != 0U) && (count < sizeof(reversed)));

    while ((count < digits) && (count < sizeof(reversed))) {
        reversed[count++] = '0';
    }
    for (index = 0U; index < count; index++) {
        text[index] = reversed[count - index - 1U];
    }
    text[count] = '\0';
    OLED_ShowString(x, y, text, font);
}

bool OLED_Refresh(void)
{
    uint8_t page;

    if (!gOledReady) {
        return false;
    }
    for (page = 0U; page < OLED_PAGE_COUNT; page++) {
        uint8_t pageMask = (uint8_t)(1U << page);
        uint8_t addressCommands[3];

        if ((gOledDirtyPages & pageMask) == 0U) {
            continue;
        }
        addressCommands[0] = (uint8_t)(0xB0U | page);
        addressCommands[1] = 0x00U;
        addressCommands[2] = 0x10U;
        if (!OLED_writeCommands(addressCommands, sizeof(addressCommands)) ||
            !OLED_write(OLED_CONTROL_DATA,
                &gOledBuffer[page][0], OLED_WIDTH)) {
            return false;
        }
        gOledDirtyPages &= (uint8_t)~pageMask;
    }
    return true;
}

bool OLED_SetContrast(uint8_t contrast)
{
    uint8_t commands[2] = {0x81U, contrast};

    if (!gOledReady) {
        return false;
    }
    return OLED_writeCommands(commands, sizeof(commands));
}

bool OLED_DisplayOn(void)
{
    static const uint8_t commands[] = {0x8DU, 0x14U, 0xAFU};

    if (!gOledReady) {
        return false;
    }
    return OLED_writeCommands(commands, sizeof(commands));
}

bool OLED_DisplayOff(void)
{
    static const uint8_t commands[] = {0xAEU, 0x8DU, 0x10U};

    if (!gOledReady) {
        return false;
    }
    return OLED_writeCommands(commands, sizeof(commands));
}

bool OLED_IsReady(void)
{
    return gOledReady;
}
