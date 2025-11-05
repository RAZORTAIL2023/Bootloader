#include "local.h"

void NON_BLOCKING_Delay(uint32_t* static_timestamp, uint32_t time, const std::function<void(void)>& func) {
    if (HAL_GetTick() - *static_timestamp > time) {
        *static_timestamp = HAL_GetTick();
        func();
    }
}

extern "C" int stdout_putchar(int ch)
{
    while ((LOG_huart.Instance->ISR & 0x40) == 0);
    LOG_huart.Instance->TDR = (uint8_t)ch;
    return ch;
}

void Local::Indicator() {
    static auto func = []() { HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin); };
    NON_BLOCKING_DELAY(500, func);
}