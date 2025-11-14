#pragma once

#include "task.h"
#include "main.h"

enum class YmodemWorkMode {
    FlashProgramming,
    OTAUpdate
} extern CurMode;

#if   defined(LOCAL_FLAG_STM32G474VE)

extern IWDG_HandleTypeDef hiwdg;
extern CRC_HandleTypeDef  hcrc;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;
extern TIM_HandleTypeDef  htim20;
#define PC_huart  (huart2)
#define LCD_huart (huart3)
#define IWDG_htim (htim20)

#elif defined(LOCAL_FLAG_STM32G474RB)

extern IWDG_HandleTypeDef hiwdg;
extern CRC_HandleTypeDef  hcrc;
extern UART_HandleTypeDef huart4;
extern UART_HandleTypeDef huart5;
extern TIM_HandleTypeDef  htim20;
#define PC_huart  (huart4)
#define LCD_huart (huart5)
#define IWDG_htim (htim20)

#endif