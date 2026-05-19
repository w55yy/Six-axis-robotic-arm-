/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * ????????? STM32CubeMX ????????? USER CODE ????????????
  *   ?C OLED??I2C2???????
  *   ?C ??????????????
  *   ?C ?????????????????????
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "chinese_font.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "pca9685.h"
#include "servo.h"
#include "keyboard.h"
#include "oled.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct
{
    float   angle[SERVO_COUNT];
    float   speed_dps;
    uint16_t hold_ms;
} ArmPose_TypeDef;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SERVO_STEP_DEG    3.0f
#define SERVO_MANUAL_DPS  30.0f
#define SERVO_AUTO_DPS    45.0f
#define LED_PIN           GPIO_PIN_6
#define LED_PORT          GPIOA
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define ARRAY_LEN(x)      (sizeof(x) / sizeof((x)[0]))
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c2;

/* USER CODE BEGIN PV */
/* Home pose (6 servos) */
static const float home_pose[SERVO_COUNT] = {90.0f, 90.0f, 90.0f, 90.0f, 90.0f, 30.0f};

/* ????????? */
static const ArmPose_TypeDef grab_sequence[] = {
    {{90.0f, 80.0f, 90.0f, 90.0f, 90.0f, 25.0f}, SERVO_AUTO_DPS, 200U},
    {{60.0f, 55.0f, 45.0f, 90.0f, 90.0f, 25.0f}, SERVO_AUTO_DPS, 300U},
    {{60.0f, 55.0f, 45.0f, 90.0f, 90.0f, 80.0f}, SERVO_AUTO_DPS, 300U},
    {{90.0f, 90.0f, 90.0f, 90.0f, 90.0f, 80.0f}, SERVO_AUTO_DPS, 300U},
    {{120.0f, 70.0f, 80.0f, 90.0f, 90.0f, 80.0f}, SERVO_AUTO_DPS, 300U},
    {{120.0f, 70.0f, 80.0f, 90.0f, 90.0f, 25.0f}, SERVO_AUTO_DPS, 300U},
    {{90.0f, 90.0f, 90.0f, 90.0f, 90.0f, 30.0f}, SERVO_AUTO_DPS, 0U}
};

/* ??????? */
static uint8_t selected_servo   = 0U;
static uint8_t sequence_running = 0U;
static uint8_t sequence_index   = 0U;
static uint32_t sequence_hold_until = 0U;
static uint32_t led_last_tick    = 0U;
static uint32_t oled_refresh_tick = 0U;
static uint32_t pca_retry_tick    = 0U;

#define OLED_KEY_LOG_LINES 6
#define OLED_KEY_FIRST_ROW 1U
static char key_history[OLED_KEY_LOG_LINES];
static uint8_t key_history_count = 0U;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2C2_Init(void);
/* USER CODE BEGIN PFP */
static void OLED_RefreshAll(void);
static void OLED_KeyHistory_Append(char key);
static void Arm_MovePose(const float *pose, float speed_dps);
static void Arm_StartSequence(void);
static void Arm_StopSequence(void);
static void Arm_UpdateSequence(void);
static void Arm_ProcessKey(char key);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static void I2C_ScanBus(I2C_HandleTypeDef *hi2c, char *buf, uint8_t buf_size)
{
    uint8_t pos = 0U;
    buf[0] = '\0';
    for (uint8_t addr = 2U; addr < 254U; addr += 2U) {
        if (HAL_I2C_IsDeviceReady(hi2c, (uint16_t)addr, 1U, 5U) == HAL_OK) {
            if (addr == 0x80U) {
                pos += (uint8_t)snprintf(buf + pos, (size_t)(buf_size - pos), "PCA");
            } else if (addr == 0x78U) {
                pos += (uint8_t)snprintf(buf + pos, (size_t)(buf_size - pos), "OLED");
            } else {
                pos += (uint8_t)snprintf(buf + pos, (size_t)(buf_size - pos), "0x%02X", addr >> 1);
            }
            if (pos < buf_size - 1U) {
                buf[pos++] = ' ';
                buf[pos] = '\0';
            }
        }
    }
    if (pos == 0U) {
        snprintf(buf, buf_size, "NONE");
    }
}


static void OLED_RefreshAll(void)
{
    OLED_ClearBuffer();

    /* Page 0: status line + uptime */
    char line[17];
    uint32_t tick = HAL_GetTick();
    if (PCA9685_IsOK()) {
        snprintf(line, sizeof(line), "S%u:%3.0f' %lus",
                 selected_servo + 1U,
                 (double)Servo_GetAngle(selected_servo),
                 (unsigned long)(tick / 1000U));
    } else {
        snprintf(line, sizeof(line), "ERR %-3lus NO PCA",
                 (unsigned long)(tick / 1000U));
    }
    OLED_Draw6x8String(0, 0, line);

    for (uint8_t row = 0U; row < key_history_count; row++) {
        char kline[2] = {key_history[row], '\0'};
        OLED_Draw6x8String(0, OLED_KEY_FIRST_ROW + row, kline);
    }

    /* Page 7: raw key scan for debug */
    char raw = Keypad_ReadRaw();
    if (raw) {
        char dbg[3] = {'R', raw, '\0'};
        OLED_Draw6x8String(0, 7, dbg);
    }
}

static void Arm_MovePose(const float *pose, float speed_dps)
{
    for (uint8_t i = 0U; i < SERVO_COUNT; ++i) {
        Servo_MoveTo(i, pose[i], speed_dps);
    }
}

static void Arm_StartSequence(void)
{
    sequence_running   = 1U;
    sequence_index     = 0U;
    sequence_hold_until = 0U;
    Arm_MovePose(grab_sequence[0].angle, grab_sequence[0].speed_dps);
}

static void Arm_StopSequence(void)
{
    sequence_running = 0U;
    for (uint8_t i = 0U; i < SERVO_COUNT; ++i) {
        Servo_MoveTo(i, Servo_GetAngle(i), SERVO_MANUAL_DPS);
    }
}

static void Arm_UpdateSequence(void)
{
    uint32_t now = HAL_GetTick();

    if (!sequence_running) {
        return;
    }

    if (Servo_IsMoving()) {
        return;
    }

    if (sequence_hold_until == 0U) {
        sequence_hold_until = now + grab_sequence[sequence_index].hold_ms;
        return;
    }

    if ((int32_t)(now - sequence_hold_until) < 0) {
        return;
    }

    ++sequence_index;
    sequence_hold_until = 0U;

    if (sequence_index >= ARRAY_LEN(grab_sequence)) {
        sequence_running = 0U;
        return;
    }

    Arm_MovePose(grab_sequence[sequence_index].angle,
                 grab_sequence[sequence_index].speed_dps);
}

static void Arm_ProcessKey(char key)
{
    float angle;

    if (key >= '1' && key <= '6') {
        selected_servo = (uint8_t)(key - '1');
        return;
    }

    switch (key) {
        case 'A':
            Arm_StopSequence();
            angle = Servo_GetAngle(selected_servo) + SERVO_STEP_DEG;
            Servo_MoveTo(selected_servo, angle, SERVO_MANUAL_DPS);
            break;

        case 'B':
            Arm_StopSequence();
            angle = Servo_GetAngle(selected_servo) - SERVO_STEP_DEG;
            Servo_MoveTo(selected_servo, angle, SERVO_MANUAL_DPS);
            break;

        case 'C':
            Arm_StartSequence();
            break;

        case 'D':
            Arm_StopSequence();
            Arm_MovePose(home_pose, SERVO_AUTO_DPS);
            break;

        case '*':
            Arm_StopSequence();
            Servo_MoveTo(5U, 25.0f, SERVO_MANUAL_DPS);
            break;

        case '#':
            Arm_StopSequence();
            Servo_MoveTo(5U, 80.0f, SERVO_MANUAL_DPS);
            break;

        case '0':
            Arm_StopSequence();
            break;

        default:
            break;
    }
}

static void OLED_KeyHistory_Append(char key)
{
    if (key_history_count < OLED_KEY_LOG_LINES) {
        key_history[key_history_count++] = key;
    } else {
        memmove(key_history, key_history + 1, (size_t)(OLED_KEY_LOG_LINES - 1));
        key_history[OLED_KEY_LOG_LINES - 1] = key;
    }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_I2C2_Init();
  /* USER CODE BEGIN 2 */
    Key_Board_Init();
    OLED_Init();
    OLED_Clear();

    /* Step 1: scan I2C1 bus, show found devices */
    char scan_buf[32];
    OLED_ShowString(0, 0, "I2C1 scan...     ");
    I2C_ScanBus(&hi2c1, scan_buf, sizeof(scan_buf));
    OLED_ShowString(0, 2, scan_buf);

    /* Step 2: try init PCA9685, show result */
    if (PCA9685_Init(&hi2c1)) {
        Servo_InitAll(&hi2c1, home_pose);
        OLED_RefreshAll();
    } else {
        OLED_ShowString(0, 4, "PCA9685 FAIL     ");
        OLED_ShowString(0, 6, "Check wiring!    ");
        /* LED on = error; recovery handled in main loop */
        HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);
    }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
        uint32_t now = HAL_GetTick();
        char key = Keyboard_GetKey();
        static char prev_key = 0;
        static uint32_t last_move_tick = 0U;

        /* Continuous A/B: move target by elapsed * speed for smooth motion */
        if (key == 'A' || key == 'B') {
            uint32_t dt = now - last_move_tick;
            if (dt > 100U) dt = 1U;  /* clamp gap: first press or long idle */
            if (dt > 0U) {
                float step = SERVO_MANUAL_DPS * ((float)dt / 1000.0f);
                float angle = Servo_GetAngle(selected_servo);
                if (key == 'B') step = -step;
                Arm_StopSequence();
                Servo_MoveTo(selected_servo, angle + step, SERVO_MANUAL_DPS);
                last_move_tick = now;
            }
        } else {
            last_move_tick = now;  /* reset when A/B released */
        }

        /* Other keys: fire on rising edge only */
        if (key && key != prev_key && key != 'A' && key != 'B') {
            HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
            Arm_ProcessKey(key);
            OLED_KeyHistory_Append(key);
        }
        prev_key = key;

        Servo_Update(&hi2c1);

        Arm_UpdateSequence();

        /* PCA9685 recovery: re-init every 2 seconds if bus error */
        if (!PCA9685_IsOK() && HAL_GetTick() - pca_retry_tick >= 2000U) {
            pca_retry_tick = HAL_GetTick();
            if (PCA9685_Init(&hi2c1)) {
                Servo_InitAll(&hi2c1, home_pose);
                HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);
            }
        }

        /* OLED staggered refresh: rebuild every 150ms, flush 1 page/loop */
        static uint8_t oled_flush_page = OLED_PAGE_NUM;
        if (HAL_GetTick() - oled_refresh_tick >= 150U) {
            oled_refresh_tick = HAL_GetTick();
            OLED_RefreshAll();        /* draw into framebuffer, no I2C */
            oled_flush_page = 0U;     /* restart flush cycle */
        }
        if (oled_flush_page < OLED_PAGE_NUM) {
            OLED_FlushPage(oled_flush_page++);
        }

        /* LED heartbeat 500ms — only when PCA9685 is OK */
        if (PCA9685_IsOK() && HAL_GetTick() - led_last_tick >= 500U) {
            HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
            led_last_tick = HAL_GetTick();
        }
  /* USER CODE END 3 */
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.ClockSpeed = 100000;
  hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO clocks: keypad uses PC0-PC7 in Key_Board_Init() */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* LED (PA6) */
  HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin = LED_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_PORT, &GPIO_InitStruct);

  /* PA4, PA5, PA7: set as analog to avoid floating (IOC configured as EXTI) */
  GPIO_InitStruct.Pin = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
    __disable_irq();
    while (1) {
    }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
