#ifndef __PCA9685_H
#define __PCA9685_H

#include "main.h"

// PCA9685 �Ĵ�������
#define PCA9685_ADDR 0x80
#define PCA9685_MODE1 0x00
#define PCA9685_MODE2 0x01
#define PCA9685_LED0_ON 0x06
#define PCA9685_PRESCALE 0xFE

uint8_t PCA9685_Init(I2C_HandleTypeDef *hi2c);
uint8_t PCA9685_SetPWM(I2C_HandleTypeDef *hi2c, uint8_t channel, uint16_t on, uint16_t off);
uint8_t PCA9685_IsOK(void);

#endif
