#include "task.h"

/* ----------- */
#include "main.h"
#include "stm32f405xx.h"
extern UART_HandleTypeDef huart4;

Bootloader local_Bootloader(0x08000000, 0x100000, 0x20000000, 0x1C000, 0x20000, 0x20000);

extern "C" int stdout_putchar(int ch)
{
    while ((huart4.Instance->SR & 0X40) == 0);
    huart4.Instance->DR = ch;
    return ch;
}

void Bootloader::Delay(uint32_t ms) {
    HAL_Delay(ms);
}

uint32_t Bootloader::GetTick() {
    return HAL_GetTick();
}

void Bootloader::indicator() {
    static uint32_t static_timeout = 0;
    NON_BLOCKING_Delay(&static_timeout, 500, []() {
        HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_2);
    });
}

bool Bootloader::retainBootloaderCallback() {
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST)) // 看门狗复位，意味着APP卡死或者手动中止APP
    {
        __HAL_RCC_CLEAR_RESET_FLAGS();
        return true;
    }
    return false;
}

void Bootloader::FlashErase() {
    HAL_FLASH_Unlock();
    // APP 使用扇区5
    FLASH_Erase_Sector(FLASH_SECTOR_5, VOLTAGE_RANGE_3);
    HAL_FLASH_Lock();
}

void Bootloader::Ymodem_beforeReceiveCallback() {
    // DisableUARTReceiveIT
    __HAL_UART_DISABLE_IT(&huart4, UART_IT_RXNE);   // 禁用接收中断
    __HAL_UART_CLEAR_FLAG(&huart4, UART_FLAG_RXNE); // 清除接收中断标志

    // 清除错误标志
    volatile uint32_t tmpreg = 0x00U;
    tmpreg = huart4.Instance->SR;
    tmpreg = huart4.Instance->DR;
    UNUSED(tmpreg);

	huart4.RxState = HAL_UART_STATE_READY;
}

bool Bootloader::Ymodem_RecvByte(uint8_t* pData, uint32_t Timeout) {
    // 可能需要处理485芯片之类的串口方向
    return HAL_UART_Receive(&huart4, pData, 1, Timeout) == HAL_OK ? true : false;
}

void Bootloader::Ymodem_SendByte(uint8_t data) {
    // 可能需要处理485芯片之类的串口方向
    HAL_UART_Transmit(&huart4, &data, 1, HAL_MAX_DELAY);
}

bool Bootloader::FlashProgram(const uint32_t startAddr, const uint8_t* const pData, size_t size)
{
    HAL_FLASH_Unlock();
    for (size_t i=0; i<size; i++) {
        HAL_StatusTypeDef result = HAL_FLASH_Program(TYPEPROGRAM_BYTE, startAddr+i, *(pData+i));
        if (result != HAL_OK) {
            HAL_FLASH_Lock();
            return false;
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