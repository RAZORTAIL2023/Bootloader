#include "local.h"

bool Bootloader::IsJumpToAPP()
{
    if (FLASH->OPTR & FLASH_OPTR_DBANK) {
        HAL_FLASH_Unlock();
        HAL_FLASH_OB_Unlock();
        HAL_FLASHEx_OB_DBankConfig(OB_DBANK_128_BITS);
        HAL_Delay(100);
        HAL_FLASH_OB_Launch();
        while(true);
    }

    if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST)) {
        __HAL_RCC_CLEAR_RESET_FLAGS();
        return false;
    }

    return true;
}

void Bootloader::JumpToApp()
{
#if 1
    /* 取出并检查栈顶 */
    uint32_t StackTop = *(uint32_t*)this->APP.StartAddr;
    if(StackTop < this->RAM.StartAddr || StackTop > this->RAM.EndAddr) {
        puts("Error StackTop, APP Procedure may not exist.");
        printf("StackTop: 0x%08X\n", StackTop);
        return;
    }
#else
#endif

    puts("Jump to APP...");
    
    /* 处理中断 */
    for (int i=0; i<8; i++) { NVIC->ICER[i]=0xFFFFFFFF; NVIC->ICPR[i]=0xFFFFFFFF; }
    
    /* 处理外设 */
    HAL_DeInit();

    /* 处理SysTick */
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    /* 设置中断向量表偏移 */
    SCB->VTOR = APP.StartAddr;

    /* 执行APP的复位中断 */
    uint32_t Reset_Handler_APP_ADDRESS = *(uint32_t*)(APP.StartAddr + 4); // 取出APP的复位中断地址
    typedef void (*pFunction)(void); pFunction Reset_Handler_APP = (pFunction)Reset_Handler_APP_ADDRESS;   // 将该地址解释为函数指针
    __set_MSP(StackTop);
    Reset_Handler_APP();
}