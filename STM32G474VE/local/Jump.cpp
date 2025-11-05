#include "local.h"

bool Bootloader::IsJumpToAPP()
{
    if (FLASH->OPTR & FLASH_OPTR_DBANK) {
        puts("[BOOT][INFO] 检测到Flash为DUALBANK模式, 现在修改选项字节, BOOTLOADER将无法再次运行, 请等待2s后重新烧录...");
        HAL_FLASH_Unlock();
        HAL_FLASH_OB_Unlock();
        HAL_FLASHEx_OB_DBankConfig(OB_DBANK_128_BITS);
        HAL_Delay(100);
        HAL_FLASH_OB_Launch();
        while(true);
    }

    if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST)) {
        __HAL_RCC_CLEAR_RESET_FLAGS();
        puts("[BOOT][INFO] 硬件看门狗复位");
        return false;
    }

    if (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST)) {
        __HAL_RCC_CLEAR_RESET_FLAGS();
        puts("[BOOT][INFO] 窗口看门狗复位");
        return false;
    }

    return true;
}

void Bootloader::JumpToAPP() {

    /* 取出并检查栈顶 */
    uint32_t StackTop = *(uint32_t*)this->APP.StartAddr;
    if (StackTop == 0xFFFFFFFF) {
        puts("[BOOT][INFO] 空Flash.");
        return;
    } else if (StackTop < this->RAM.StartAddr || StackTop > this->RAM.EndAddr) {
        printf("[BOOT][Error] 跳转失败, 栈顶值异常: 0x%08X\n", StackTop);
        return;
    }

    puts("[BOOT][INFO] 准备跳转到APP...");
    
    /* 刷新看门狗 */
    Local::IWDG_Refresh();

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
    typedef void (*pFunction)(void);
    pFunction Reset_Handler_APP = (pFunction)Reset_Handler_APP_ADDRESS;   // 将该地址解释为函数指针
    __set_MSP(StackTop);
    Reset_Handler_APP();
}