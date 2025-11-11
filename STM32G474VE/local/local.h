#pragma once
#include "task.h"
#include "main.h"

enum class YmodemWorkMode {
    FlashProgramming,
    OTAUpdate
} extern CurMode;

extern IWDG_HandleTypeDef hiwdg;
extern TIM_HandleTypeDef  htim20; // For IWDG
extern UART_HandleTypeDef huart2; // For PC
extern CRC_HandleTypeDef  hcrc;

#define PC_huart (huart2)