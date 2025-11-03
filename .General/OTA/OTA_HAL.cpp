#include "OTA.h"
#include "main.h"

extern UART_HandleTypeDef huart3;
#define huartx (huart3)

std::vector<uint8_t> OTA::HAL::Receive(uint8_t Size, uint32_t timeout_ms) {
    std::vector<uint8_t> buf(Size);
    uint32_t start_ms = HAL_GetTick();
    if (HAL_UART_Receive(&huartx, buf.data(), Size, timeout_ms) == HAL_OK) {
        if (logLevel >= LogLevel::Full) printf("[OTA][INFO] 接收完成, 用时: %u ms\n", HAL_GetTick() - start_ms);
        return buf;
    } else {
        if (logLevel >= LogLevel::Light) printf("[OTA][ERROR] 接收超时: %u\n", timeout_ms);
        return {};
    }
}

void OTA::HAL::Transmit(std::vector<uint8_t>& msg) {
    if (HAL_UART_Transmit(&huartx, msg.data(), msg.size(), 1000) != HAL_OK) {
        if (logLevel >= LogLevel::Light) printf("[OTA][ERROR] OTA发送错误!\n");
    }
}