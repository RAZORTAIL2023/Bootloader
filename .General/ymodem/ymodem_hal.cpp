#include "ymodem.h"
#include "main.h"
#include "local.h"

static bool User_UART_DeInit() {

    if (PC_huart.hdmarx != nullptr) HAL_DMA_Abort(PC_huart.hdmarx);

    HAL_StatusTypeDef status = HAL_UART_AbortReceive(&PC_huart);
    HAL_Delay(100);
    if (status != HAL_OK || PC_huart.RxState != HAL_UART_STATE_READY) return false;
    
    __HAL_UART_DISABLE(&PC_huart);

    __HAL_UART_CLEAR_FLAG(&PC_huart,
        UART_CLEAR_PEF |
        UART_CLEAR_FEF |
        UART_CLEAR_NEF |
        UART_CLEAR_OREF |
        UART_CLEAR_IDLEF
    );

    CLEAR_BIT(PC_huart.Instance->CR1,
        USART_CR1_PEIE |
        USART_CR1_TXEIE_TXFNFIE |
        USART_CR1_TCIE |
        USART_CR1_RXNEIE_RXFNEIE |
        USART_CR1_IDLEIE
    );

    CLEAR_BIT(PC_huart.Instance->CR3,
        USART_CR3_EIE |
        USART_CR3_RXFTIE |
        USART_CR3_TXFTIE |
        USART_CR3_DMAR |
        USART_CR3_DMAT
    );

    HAL_Delay(100);
    __HAL_UART_ENABLE(&PC_huart);
    HAL_Delay(100);
    if (__HAL_UART_GET_FLAG(&PC_huart, UART_FLAG_REACK) == RESET || __HAL_UART_GET_FLAG(&PC_huart, UART_FLAG_TEACK) == RESET) return false;
    
    return true;
}

bool Ymodem::HAL::Init() {
    puts("[BOOT][INFO] Ymodem开始, 与PC的沟通已关闭");
    HAL_Delay(100);
    if (!User_UART_DeInit()) { puts("[BOOT][ERROR] UART中止接收失败"); return false; }
    Logger::release();

    static auto logic_flash = []() {
        log("[BOOT][INFO] 开始擦除APP区域: 起始地址: 0x%08X 大小: 0x%X字节\n", bootloader.APP.StartAddr, bootloader.APP.MAXSize);
        if (!bootloader.FlashErase()) {
            log("[BOOT][ERROR] 擦除APP区域失败, Ymodem已停止.");
            Ymodem::End();
        } else {
            log("[BOOT][INFO] 擦除APP区域成功");
        }
    };

    static auto logic_OTA = []() {
        /* DO NOTHING */
    };

    if (CurMode == YmodemWorkMode::FlashProgramming) {
        logic_flash();
    } else if (CurMode == YmodemWorkMode::OTAUpdate) {
        logic_OTA();
    }

    return true;
}

bool Ymodem::HAL::RecvByte(uint8_t* pData) {
    return HAL_UART_Receive(&PC_huart, pData, 1, 2000) == HAL_OK;
}

void Ymodem::HAL::SendByte(uint8_t data) {
    HAL_UART_Transmit(&PC_huart, &data, 1, HAL_MAX_DELAY);
}

bool Ymodem::HAL::RecvMultiByte(uint8_t* pData, uint16_t Size) {
    return HAL_UART_Receive(&PC_huart, pData, Size, 5000) == HAL_OK;
}

uint32_t Ymodem::HAL::EOT_Delay() {
    HAL_Delay(500);
    return 500;
}

uint16_t Ymodem::HAL::CRC_Calculate(const uint8_t* pData, uint32_t Size) {
    return (uint16_t)HAL_CRC_Calculate(&hcrc, (uint32_t*)pData, Size);
}