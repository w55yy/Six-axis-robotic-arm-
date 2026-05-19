#ifndef __OLED_H
#define __OLED_H

#include "main.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* OLED on I2C2 (hi2c2). Change OLED_I2C_HANDLE if wired differently. */
#define OLED_I2C_HANDLE       hi2c2
#define OLED_ADDRESS          0x78U

#define OLED_WIDTH            128U
#define OLED_HEIGHT           64U
#define OLED_PAGE_NUM         (OLED_HEIGHT / 8U)

#define OLED_CMD              0x00U
#define OLED_DATA             0x40U

/* Many 0.96" modules are SH1106: visible columns start at offset 2. Use 0 for pure SSD1306. */
#ifndef OLED_COLUMN_OFFSET
#define OLED_COLUMN_OFFSET    2U
#endif

/* Brief full-panel flash after init = bus OK. Set to 0 to disable. */
#ifndef OLED_PANEL_SELFTEST
#define OLED_PANEL_SELFTEST   1
#endif

void OLED_Init(void);
void OLED_Clear(void);
void OLED_Display_On(void);
void OLED_Display_Off(void);

void OLED_Set_Pos(uint8_t x, uint8_t y);
void OLED_WR_Byte(uint8_t dat, uint8_t cmd);

void OLED_ShowChar(uint8_t x, uint8_t y, char chr);
void OLED_ShowString(uint8_t x, uint8_t y, const char *str);
void OLED_ShowNumber(uint8_t x, uint8_t y, int num, uint8_t len);
void OLED_ShowFloat(uint8_t x, uint8_t y, float num,
                    uint8_t int_len, uint8_t dec_len);

void OLED_DrawBMP(uint8_t x0, uint8_t y0,
                  uint8_t x1, uint8_t y1,
                  const uint8_t *bmp);

/* Framebuffer API: draw in RAM, then flush once (no flicker) */
void OLED_ClearBuffer(void);
void OLED_FlushBuffer(void);
void OLED_FlushPage(uint8_t page);
void OLED_Draw6x8String(uint8_t x, uint8_t page, const char *str);

extern const uint8_t F6x8[][6];
extern const uint8_t F8X16[][16];

extern I2C_HandleTypeDef hi2c2;

#ifdef __cplusplus
}
#endif

#endif /* __OLED_H */
