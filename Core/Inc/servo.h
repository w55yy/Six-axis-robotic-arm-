#ifndef __SERVO_H
#define __SERVO_H

#include "pca9685.h"

#define SERVO_MIN_TICK   153U
#define SERVO_MAX_TICK   512U
#define SERVO_COUNT      6U

typedef struct
{
    float current_angle;
    float target_angle;
    float speed_dps;
} Servo_StateTypeDef;

void Servo_SetAngle(I2C_HandleTypeDef *hi2c, uint8_t channel, float angle);
void Servo_InitAll(I2C_HandleTypeDef *hi2c, const float *home_angles);
void Servo_MoveTo(uint8_t channel, float angle, float speed_dps);
void Servo_Update(I2C_HandleTypeDef *hi2c);
uint8_t Servo_IsMoving(void);
float Servo_GetAngle(uint8_t channel);

#endif
