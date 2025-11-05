#pragma once
#include "task.h"
#include "main.h"

enum class YmodemWorkMode {
    FlashProgramming,
    OTAUpdate
} extern CurMode;

extern IWDG_HandleTypeDef hiwdg;
extern TIM_HandleTypeDef  htim20; // For IWDG 1s
extern UART_HandleTypeDef huart1; // For Printf
extern UART_HandleTypeDef huart2; // For PC

#define LOG_huart (huart1)
#define PC_huart  (huart2)