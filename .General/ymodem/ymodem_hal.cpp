#include "ymodem.h"
#include "main.h"
#include "local.h"

extern UART_HandleTypeDef huart2;
extern CRC_HandleTypeDef  hcrc;
#define huartx (&huart2)
#define hcrcx  (hcrc)

bool Ymodem::HAL::Init() {
    puts("[BOOT][INFO] Ymodem开始, 与PC的沟通已关闭");

    if (HAL_UART_AbortReceive(huartx) != HAL_OK) {
        if (huartx->hdmarx != nullptr) HAL_DMA_Abort(huartx->hdmarx);
        Ymodem::End();
        HAL_UARTEx_ReceiveToIdle_DMA(&PC_huart, saveCmd.data.data(), sizeof(saveCmd.data)-1);
        puts("[BOOT][ERROR] HAL_UART_AbortReceive失败, 重新打开串口空闲中断.");
        return false;
    }

    HAL_UART_DeInit(huartx);
    HAL_UART_Init(huartx);

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
    return HAL_UART_Receive(huartx, pData, 1, 2000) == HAL_OK;
}

void Ymodem::HAL::SendByte(uint8_t data) {
    HAL_UART_Transmit(huartx, &data, 1, HAL_MAX_DELAY);
}

bool Ymodem::HAL::RecvMultiByte(uint8_t* pData, uint16_t Size) {
    return HAL_UART_Receive(huartx, pData, Size, 5000) == HAL_OK;
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