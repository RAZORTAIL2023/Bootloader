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

/**
 * @brief
 * - 不生成函数序言/尾声
 * - 不使用栈变量
 * - Cortex‑M 遵循 AAPCS，函数的第一个参数在寄存器r0中
 * - volatile防止编译器优化掉汇编代码
 * @param 栈顶
 */
__attribute__((naked))
static void Jump(uint32_t)
{
    __ASM volatile(
        "ldr r1, [r0]\n"
        "ldr r2, [r0, #4]\n"
        "msr msp, r1\n"
        "dsb\n"
        "isb\n"
        "bx r2\n"
    );
}

void Bootloader::JumpToAPP() {

    /* 取出并检查栈顶 */
    uint32_t StackTop = *(__IO uint32_t*)this->APP.StartAddr;
    if (StackTop == 0xFFFFFFFF) {
        puts("[BOOT][INFO] 没有检测到APP");
        return;
    } else if (StackTop < this->RAM.StartAddr || StackTop > this->RAM.EndAddr) {
        printf("[BOOT][Error] 找到不可用的代码段, 栈顶: 0x%08X\n", StackTop);
        return;
    }

    /* 取出并检查复位中断 */
    uint32_t Reset_Handler = *(__IO uint32_t*)(this->APP.StartAddr + 4);
    if (Reset_Handler == 0xFFFFFFFF) {
        puts("[BOOT][INFO] 没有检测到APP");
        return;
    } else if (Reset_Handler < this->APP.StartAddr || Reset_Handler > this->APP.EndAddr) {
        printf("[BOOT][Error] 找到不可用的代码段, 栈顶(正常): 0x%08X, 复位中断(异常): 0x%08X\n", StackTop, Reset_Handler);
        return;
    }

    puts("[BOOT][INFO] 准备跳转到APP...");
    HAL_Delay(100);
    
    Local::IWDG_Refresh();

    __disable_irq();
    for (int i=0; i<8; i++) { NVIC->ICER[i]=0xFFFFFFFF; NVIC->ICPR[i]=0xFFFFFFFF; }

    HAL_DeInit();
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

#if (__ICACHE_PRESENT == 1)
    SCB_InvalidateICache();
#endif
#if (__DCACHE_PRESENT == 1)
    SCB_CleanInvalidateDCache();
#endif

    SCB->VTOR = APP.StartAddr;
    __DSB(); __ISB();
    Jump(this->APP.StartAddr);
    __builtin_unreachable();
}