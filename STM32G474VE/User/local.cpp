#include "task.h"
#include "main.h"
#include "stm32G474xx.h"
extern UART_HandleTypeDef huart2;
#define huartx huart2

Bootloader local_Bootloader
(
    0x08000000, // ROM起始地址
    0x80000,    // ROM大小
    0x20000000, // RAM起始地址
    0x20000,    // RAM大小
    0x20000,    // BOOT大小
    0x40000     // APP大小
);

extern "C" int stdout_putchar(int ch)
{
    while ((huartx.Instance->ISR & 0x40) == 0);
    huartx.Instance->TDR = (uint8_t)ch;
    return ch;
}

void Bootloader::Delay(uint32_t ms)
{
    HAL_Delay(ms);
}

uint32_t Bootloader::GetTick()
{
    return HAL_GetTick();
}

void Bootloader::indicator()
{
    static uint32_t static_timeout = 0; // 不属于对象
    NON_BLOCKING_Delay(&static_timeout, 500, [](){HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);} );
}

bool Bootloader::retainBootloaderCallback()
{
    // 看门狗复位，意味着APP卡死或者手动中止APP
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST))
    {
        __HAL_RCC_CLEAR_RESET_FLAGS();
        return true;
    }
    return false;
}

void Bootloader::Ymodem_beforeReceiveCallback()
{
    // DisableUARTReceiveIT
    __HAL_UART_DISABLE_IT(&huartx, UART_IT_RXNE);   // 禁用接收中断
    __HAL_UART_CLEAR_FLAG(&huartx, UART_FLAG_RXNE); // 清除接收中断标志

    // 清除错误标志
    volatile uint32_t tmpreg = 0x00U;
    tmpreg = huartx.Instance->ISR;
    tmpreg = huartx.Instance->TDR;
    UNUSED(tmpreg);

	huartx.RxState = HAL_UART_STATE_READY;
}

bool Bootloader::Ymodem_RecvByte(uint8_t* pData, uint32_t Timeout)
{
    return HAL_UART_Receive(&huartx, pData, 1, Timeout) == HAL_OK ? true : false;
}

void Bootloader::Ymodem_SendByte(uint8_t data)
{
    HAL_UART_Transmit(&huartx, &data, 1, HAL_MAX_DELAY);
}

void Bootloader::FlashErase()
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    HAL_FLASH_Unlock();
    {
        FLASH_EraseInitTypeDef EraseInitStruct;
        uint32_t PageError = 0;

        EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
        // Single BANK
        EraseInitStruct.Banks = FLASH_BANK_1;
        // 单BANK模式下每页4KB，即每页0x1000，由于bootloader使用128KB即0~31页，所以从第32页开始擦除
        EraseInitStruct.Page  = 32;
        EraseInitStruct.NbPages = APP_maxsize / 0x1000;

        HAL_FLASHEx_Erase(&EraseInitStruct, &PageError);
    }
    HAL_FLASH_Lock();

    if (!primask) __enable_irq();
}

bool Bootloader::FlashProgram(const uint32_t startAddr, const uint8_t* const pData, size_t size)
{
    if (size == 0) return true;
    
    HAL_FLASH_Unlock();
    {
        // 处理8字节对齐的部分
        size_t alignedSize = (size / 8) * 8;
        for (size_t i = 0; i < alignedSize; i += 8) {
            // 正确读取8字节数据作为双字
            uint64_t doubleWord = 0;
            for (int j = 0; j < 8; j++) {
                doubleWord |= ((uint64_t)pData[i + j]) << (j * 8);
            }
            
            HAL_StatusTypeDef result = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, startAddr + i, doubleWord);
            if (result != HAL_OK) {
                HAL_FLASH_Lock();
                return false;
            }
        }
        
        // 处理剩余不足8字节的部分
        size_t remainingBytes = size % 8;
        if (remainingBytes > 0) {
            uint64_t lastDoubleWord = 0xFFFFFFFFFFFFFFFF; // 填充0xFF (Flash擦除后的默认值)
            
            // 将剩余字节复制到lastDoubleWord的低位
            for (size_t i = 0; i < remainingBytes; i++) {
                lastDoubleWord &= ~((uint64_t)0xFF << (i * 8));  // 清除对应字节位置
                lastDoubleWord |= ((uint64_t)pData[alignedSize + i]) << (i * 8);  // 设置新值
            }
            
            HAL_StatusTypeDef result = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, startAddr + alignedSize, lastDoubleWord);
            if (result != HAL_OK) {
                HAL_FLASH_Lock();
                return false;
            }
        }
    }
    HAL_FLASH_Lock();
    return true;
}

void Bootloader::JumpToApp()
{
    /* 取出并检查栈顶 */
    uint32_t StackTop = *(uint32_t*)APP_startAddr;
    if(StackTop < RAM_startAddr || StackTop > RAM_endAddr) {
        puts("Error StackTop, APP Procedure may not exist.");
        printf("StackTop: 0x%08X\n", StackTop);
        return;
    }
    puts("Jump to APP...");
    
    /* 处理中断 */
    for(int i=0; i<8; i++) { NVIC->ICER[i]=0xFFFFFFFF; NVIC->ICPR[i]=0xFFFFFFFF; }
    
    /* 处理外设 */
    HAL_DeInit();

    /* 处理SysTick */
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    /* 设置中断向量表偏移 */
    SCB->VTOR = APP_startAddr;

    /* 执行APP的复位中断 */
    uint32_t Reset_Handler_APP_ADDRESS = *(uint32_t*)(APP_startAddr + 4); // 取出APP的复位中断地址
    typedef void (*pFunction)(void); pFunction Reset_Handler_APP = (pFunction)Reset_Handler_APP_ADDRESS;   // 将该地址解释为函数指针
    __set_MSP(StackTop);
    Reset_Handler_APP();
}