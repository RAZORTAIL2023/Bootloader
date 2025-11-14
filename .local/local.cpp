#include "local.h"

YmodemWorkMode CurMode = YmodemWorkMode::FlashProgramming;

Bootloader bootloader
(
    0x08000000, // ROM起始地址
    0x80000,    // ROM大小
    0x20000000, // RAM起始地址
    0x20000,    // RAM大小
    0x10000,    // BOOT大小
    0x40000     // APP大小
);

void Local::Init() {
    HAL_UARTEx_ReceiveToIdle_DMA(&PC_huart, saveCmd.data.data(), sizeof(saveCmd.data)-1);
    HAL_TIM_Base_Start_IT(&IWDG_htim);
}

void Local::Work() {
    
}

void Local::IWDG_Refresh() {
    HAL_IWDG_Refresh(&hiwdg);
}