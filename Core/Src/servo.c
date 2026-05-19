#include "servo.h"

static Servo_StateTypeDef servo_state[SERVO_COUNT];
static uint32_t servo_last_tick = 0U;

static float Servo_ClampAngle(float angle)
{
    if (angle < 0.0f) {
        return 0.0f;
    }
    if (angle > 180.0f) {
        return 180.0f;
    }
    return angle;
}

void Servo_SetAngle(I2C_HandleTypeDef *hi2c, uint8_t channel, float angle)
{
    uint16_t tick;

    if (channel >= SERVO_COUNT) {
        return;
    }

    angle = Servo_ClampAngle(angle);
    tick = (uint16_t)(SERVO_MIN_TICK + (angle * (float)(SERVO_MAX_TICK - SERVO_MIN_TICK) / 180.0f));
    if (PCA9685_IsOK()) {
        PCA9685_SetPWM(hi2c, channel, 0U, tick);
    }
}

void Servo_InitAll(I2C_HandleTypeDef *hi2c, const float *home_angles)
{
    uint8_t i;

    for (i = 0U; i < SERVO_COUNT; i++) {
        float angle = Servo_ClampAngle(home_angles[i]);
        servo_state[i].current_angle = angle;
        servo_state[i].target_angle = angle;
        servo_state[i].speed_dps = 60.0f;
        Servo_SetAngle(hi2c, i, angle);
    }

    servo_last_tick = HAL_GetTick();
}

void Servo_MoveTo(uint8_t channel, float angle, float speed_dps)
{
    if (channel >= SERVO_COUNT) {
        return;
    }

    servo_state[channel].target_angle = Servo_ClampAngle(angle);
    if (speed_dps < 1.0f) {
        speed_dps = 1.0f;
    }
    servo_state[channel].speed_dps = speed_dps;
}

void Servo_Update(I2C_HandleTypeDef *hi2c)
{
    uint8_t i;
    uint32_t now = HAL_GetTick();
    uint32_t elapsed = now - servo_last_tick;

    if (elapsed == 0U) {
        return;
    }

    servo_last_tick = now;

    for (i = 0U; i < SERVO_COUNT; i++) {
        float current = servo_state[i].current_angle;
        float target = servo_state[i].target_angle;
        float diff = target - current;
        float max_step = servo_state[i].speed_dps * ((float)elapsed / 1000.0f);

        if (diff > max_step) {
            current += max_step;
        } else if (diff < -max_step) {
            current -= max_step;
        } else {
            current = target;
        }

        if (current != servo_state[i].current_angle) {
            servo_state[i].current_angle = current;
            Servo_SetAngle(hi2c, i, current);
        }
    }
}

uint8_t Servo_IsMoving(void)
{
    uint8_t i;

    for (i = 0U; i < SERVO_COUNT; i++) {
        if (servo_state[i].current_angle != servo_state[i].target_angle) {
            return 1U;
        }
    }

    return 0U;
}

float Servo_GetAngle(uint8_t channel)
{
    if (channel >= SERVO_COUNT) {
        return 0.0f;
    }

    return servo_state[channel].current_angle;
}
