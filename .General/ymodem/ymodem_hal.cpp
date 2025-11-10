#include "ymodem.h"
#include "main.h"
#include "local.h"

extern UART_HandleTypeDef huart2;
extern CRC_HandleTypeDef  hcrc;
#define huartx (huart2)
#define hcrcx  (hcrc)

bool Ymodem::HAL::Init() {
    puts("[BOOT][INFO] Ymodem开始, 与PC的沟通已关闭");

    if (huartx.hdmarx != nullptr)  HAL_DMA_Abort(huartx.hdmarx);
    HAL_UART_AbortReceive(&huartx);
    uint32_t timeout = HAL_GetTick() + 100;
    while (huartx.RxState != HAL_UART_STATE_READY) {
        if (HAL_GetTick() > timeout) {
            puts("[BOOT][ERROR] UART状态超时");
            return false;
        }
    }
    __HAL_UART_DISABLE(&huartx);
    __HAL_UART_CLEAR_FLAG(&huartx, UART_CLEAR_PEF | UART_CLEAR_FEF | UART_CLEAR_NEF | UART_CLEAR_OREF | UART_CLEAR_IDLEF);
    CLEAR_BIT(huartx.Instance->CR1, USART_CR1_PEIE | USART_CR1_TXEIE_TXFNFIE | USART_CR1_TCIE | USART_CR1_RXNEIE_RXFNEIE | USART_CR1_IDLEIE);
    CLEAR_BIT(huartx.Instance->CR3, USART_CR3_EIE | USART_CR3_RXFTIE | USART_CR3_TXFTIE);
    CLEAR_BIT(huartx.Instance->CR3, USART_CR3_DMAR | USART_CR3_DMAT);
    __HAL_UART_SEND_REQ(&huartx, UART_RXDATA_FLUSH_REQUEST);
    __HAL_UART_SEND_REQ(&huartx, UART_TXDATA_FLUSH_REQUEST);
    HAL_Delay(1);
    __HAL_UART_ENABLE(&huartx);
    timeout = HAL_GetTick() + 100;
    while (!(__HAL_UART_GET_FLAG(&huartx, UART_FLAG_REACK))) {
        if (HAL_GetTick() > timeout) {
            puts("[BOOT][ERROR] UART REACK超时");
            return false;
        }
    }
    huartx.gState = HAL_UART_STATE_READY;
    huartx.RxState = HAL_UART_STATE_READY;
    huartx.ErrorCode = HAL_UART_ERROR_NONE;
    huartx.ReceptionType = HAL_UART_RECEPTION_STANDARD;

    Logger::release();

    if (CurMode == YmodemWorkMode::FlashProgramming) {
        log("[BOOT][INFO] 开始擦除APP区域: 起始地址: 0x%08X 大小: 0x%X字节\n", bootloader.APP.StartAddr, bootloader.APP.MAXSize);

        if (!bootloader.FlashErase()) {
            log("[BOOT][ERROR] 擦除APP区域失败, Ymodem已停止.");
            Ymodem::End();
        } else {
            log("[BOOT][INFO] 擦除APP区域成功");
        }
    }

    return true;
}

bool Ymodem::HAL::RecvByte(uint8_t* pData) {
    return HAL_UART_Receive(&huartx, pData, 1, 2000) == HAL_OK;
}

void Ymodem::HAL::SendByte(uint8_t data) {
    HAL_UART_Transmit(&huartx, &data, 1, HAL_MAX_DELAY);
}

bool Ymodem::HAL::RecvMultiByte(uint8_t* pData, uint16_t Size) {
    return HAL_UART_Receive(&huartx, pData, Size, 5000) == HAL_OK;
}

uint32_t Ymodem::HAL::EOT_Delay() {
    HAL_Delay(1000); // 防止NAK回复过快
    return 1000;
}

uint16_t Ymodem::HAL::CRC_Calculate(const uint8_t* pData, uint32_t Size) {
#if 0
    uint32_t crc = 0;
    for (uint32_t i = 0; i < Size + 2; ++i) {
        uint8_t byte = (i < Size) ? pData[i] : 0; // 后两次循环自动补两个0
        uint32_t in = byte | 0x100;
        do {
            crc <<= 1;
            in  <<= 1;
            if (in  & 0x100)   ++crc;
            if (crc & 0x10000) crc ^= 0x1021;
        } while (!(in & 0x10000));
    }
    return (uint16_t)(crc & 0xFFFFu);
#else
    return (uint16_t)HAL_CRC_Calculate(&hcrcx, (uint32_t*)pData, Size);
#endif
}