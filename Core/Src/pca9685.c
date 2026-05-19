#include "pca9685.h"

#define PCA9685_I2C_TIMEOUT  100U
#define PCA9685_RETRY_MAX    3U

static uint8_t pca9685_ok = 0U;

uint8_t PCA9685_IsOK(void)
{
    return pca9685_ok;
}

static HAL_StatusTypeDef PCA9685_Write8(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t data)
{
    HAL_StatusTypeDef ret = HAL_ERROR;
    for (uint8_t i = 0U; i < PCA9685_RETRY_MAX; i++) {
        ret = HAL_I2C_Mem_Write(hi2c, PCA9685_ADDR, reg,
                                I2C_MEMADD_SIZE_8BIT, &data, 1, PCA9685_I2C_TIMEOUT);
        if (ret == HAL_OK) return HAL_OK;
    }
    return ret;
}

static HAL_StatusTypeDef PCA9685_Write16(I2C_HandleTypeDef *hi2c, uint8_t reg, uint16_t data)
{
    uint8_t buf[2];
    buf[0] = data & 0xFF;
    buf[1] = (data >> 8) & 0xFF;
    HAL_StatusTypeDef ret = HAL_ERROR;
    for (uint8_t i = 0U; i < PCA9685_RETRY_MAX; i++) {
        ret = HAL_I2C_Mem_Write(hi2c, PCA9685_ADDR, reg,
                                I2C_MEMADD_SIZE_8BIT, buf, 2, PCA9685_I2C_TIMEOUT);
        if (ret == HAL_OK) return HAL_OK;
    }
    return ret;
}

uint8_t PCA9685_Init(I2C_HandleTypeDef *hi2c)
{
    pca9685_ok = 0U;

    if (PCA9685_Write8(hi2c, PCA9685_MODE1, 0x10) != HAL_OK) return 0U;
    if (PCA9685_Write8(hi2c, PCA9685_PRESCALE, 121) != HAL_OK) return 0U;
    if (PCA9685_Write8(hi2c, PCA9685_MODE2, 0x04) != HAL_OK) return 0U;
    if (PCA9685_Write8(hi2c, PCA9685_MODE1, 0x20) != HAL_OK) return 0U;
    HAL_Delay(1);
    if (PCA9685_Write8(hi2c, PCA9685_MODE1, 0xA0) != HAL_OK) return 0U;

    pca9685_ok = 1U;
    return 1U;
}

uint8_t PCA9685_SetPWM(I2C_HandleTypeDef *hi2c, uint8_t channel, uint16_t on, uint16_t off)
{
    if (!pca9685_ok) return 0U;
    uint8_t reg = PCA9685_LED0_ON + (4U * channel);
    if (PCA9685_Write16(hi2c, reg, on) != HAL_OK)      { pca9685_ok = 0U; return 0U; }
    if (PCA9685_Write16(hi2c, reg + 2U, off) != HAL_OK) { pca9685_ok = 0U; return 0U; }
    return 1U;
}
