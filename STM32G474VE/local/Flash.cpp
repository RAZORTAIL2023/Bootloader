#include "local.h"

static bool Erase_Core() {
    /**
     * 单BANK模式下每页4KB，即每页0x1000;
     * 由于bootloader使用128KB(0x20000)即0~31页，所以从第32页开始擦除
     */
    FLASH_EraseInitTypeDef EraseInit;
    EraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
    EraseInit.Banks = FLASH_BANK_1;
    EraseInit.Page = 32;
    EraseInit.NbPages = 0x40000 / 0x1000;
    uint32_t PageError = 0;
    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&EraseInit, &PageError);

    if (status != HAL_OK || PageError != 0xFFFFFFFF) {
        return false;
    } else {
        return true;
    }
}

bool Bootloader::FlashErase() {
    HAL_Delay(500);
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    HAL_FLASH_Unlock();
    bool result = Erase_Core();
    HAL_FLASH_Lock();
    if (!primask) __enable_irq();
    return result;
}

bool Bootloader::FlashProgram(const uint32_t startAddr, const uint8_t* const pData, size_t size) {
    if (size == 0) return true;
    HAL_FLASH_Unlock();
    
    int alignedSize = (size / 8) * 8;
    int remainingBytes = size % 8;
    
    // 处理8字节对齐的部分
    for (int i = 0; i < alignedSize; i += 8) {
        uint64_t doubleWord = 0;
        for (int j=0; j<8; j++) doubleWord |= ((uint64_t)pData[i + j]) << (j * 8);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, startAddr + i, doubleWord) != HAL_OK) {
            HAL_FLASH_Lock();
            return false;
        }
    }
    
    // 处理剩余不足8字节的部分
    if (remainingBytes > 0) {
        uint64_t lastDoubleWord = 0xFFFFFFFFFFFFFFFF;
        
        // 将剩余字节复制到lastDoubleWord的低位
        for (int i=0; i<remainingBytes; i++) {
            // 清除对应字节位置
            lastDoubleWord &= ~((uint64_t)0xFF << (i * 8));
            // 设置新值
            lastDoubleWord |= ((uint64_t)pData[alignedSize + i]) << (i * 8);
        }
        
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, startAddr + alignedSize, lastDoubleWord) != HAL_OK) {
            HAL_FLASH_Lock();
            return false;
        }
    }
    
    HAL_FLASH_Lock();
    return true;
}