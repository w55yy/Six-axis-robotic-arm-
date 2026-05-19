#include "keyboard.h"

/* 4��4 ������ PC0~PC3 ������� PC4~PC7 ������������ main ��ԭ����һ�£� */
#define KEY_ROW_PORT GPIOC
#define KEY_ROW0_PIN GPIO_PIN_0
#define KEY_ROW1_PIN GPIO_PIN_1
#define KEY_ROW2_PIN GPIO_PIN_2
#define KEY_ROW3_PIN GPIO_PIN_3

#define KEY_COL_PORT GPIOC
#define KEY_COL0_PIN GPIO_PIN_4
#define KEY_COL1_PIN GPIO_PIN_5
#define KEY_COL2_PIN GPIO_PIN_6
#define KEY_COL3_PIN GPIO_PIN_7

static const uint16_t key_row_pins[4] = {
    KEY_ROW0_PIN, KEY_ROW1_PIN, KEY_ROW2_PIN, KEY_ROW3_PIN};
static const uint16_t key_col_pins[4] = {
    KEY_COL0_PIN, KEY_COL1_PIN, KEY_COL2_PIN, KEY_COL3_PIN};

static const char key_map[4][4] = {
    {'*', '7', '4', '1'},
    {'0', '8', '5', '2'},
    {'#', '9', '6', '3'},
    {'D', 'C', 'B', 'A'}};

char Keypad_ReadRaw(void)
{
    uint8_t row, col;

    HAL_GPIO_WritePin(KEY_ROW_PORT,
                      KEY_ROW0_PIN | KEY_ROW1_PIN | KEY_ROW2_PIN | KEY_ROW3_PIN,
                      GPIO_PIN_SET);

    for (row = 0U; row < 4U; ++row) {
        HAL_GPIO_WritePin(KEY_ROW_PORT, key_row_pins[row], GPIO_PIN_RESET);
        HAL_Delay(1U);

        for (col = 0U; col < 4U; ++col) {
            if (HAL_GPIO_ReadPin(KEY_COL_PORT, key_col_pins[col]) == GPIO_PIN_RESET) {
                HAL_GPIO_WritePin(KEY_ROW_PORT, key_row_pins[row], GPIO_PIN_SET);
                return key_map[row][col];
            }
        }

        HAL_GPIO_WritePin(KEY_ROW_PORT, key_row_pins[row], GPIO_PIN_SET);
    }
    return 0;
}

char Keyboard_GetKey(void)
{
    static char last_raw = 0;
    static char debounced = 0;
    static uint32_t changed_tick = 0U;
    char raw = Keypad_ReadRaw();
    uint32_t now = HAL_GetTick();

    if (raw != last_raw) {
        last_raw = raw;
        changed_tick = now;
    }

    if ((now - changed_tick) >= 25U) {
        debounced = raw;
    }

    return debounced;
}

void Key_Board_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitStruct.Pin = KEY_ROW0_PIN | KEY_ROW1_PIN | KEY_ROW2_PIN | KEY_ROW3_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(KEY_ROW_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(KEY_ROW_PORT,
                      KEY_ROW0_PIN | KEY_ROW1_PIN | KEY_ROW2_PIN | KEY_ROW3_PIN,
                      GPIO_PIN_SET);

    GPIO_InitStruct.Pin = KEY_COL0_PIN | KEY_COL1_PIN | KEY_COL2_PIN | KEY_COL3_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(KEY_COL_PORT, &GPIO_InitStruct);
}
