#include "local.h"

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
    
    if (huart == &PC_huart) {
        saveCmd.len = Size;
        UARTCmdQueue.push(saveCmd);
        memset(&saveCmd, 0x00, sizeof(saveCmd));
        HAL_UARTEx_ReceiveToIdle_DMA(&PC_huart, saveCmd.data.data(), sizeof(saveCmd.data)-1);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {

    if (huart == &PC_huart) {
        memset(&saveCmd, 0x00, sizeof(saveCmd));
        HAL_UARTEx_ReceiveToIdle_DMA(&PC_huart, saveCmd.data.data(), sizeof(saveCmd.data)-1);
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {

    if (htim == &IWDG_htim) {
        Local::IWDG_Refresh();
    }
}