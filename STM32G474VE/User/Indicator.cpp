#include "local.h"

void NON_BLOCKING_Delay(uint32_t* static_timestamp, uint32_t time, const std::function<void(void)>& func) {
    if (HAL_GetTick() - *static_timestamp > time) {
        *static_timestamp = HAL_GetTick();
        func();
    }
}

extern "C" int stdout_putchar(int ch)
{
    while ((USART2->ISR & 0x40) == 0);
    USART2->TDR = (uint8_t)ch;
    return ch;
}

void Bootloader::Indicator() {
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
}