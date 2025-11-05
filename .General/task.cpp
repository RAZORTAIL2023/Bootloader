#include "task.h"
#include "main.h"

void task_run()
{
    if (bootloader.IsJumpToAPP()) bootloader.JumpToAPP();
    Local::Init();
    puts("[BOOT][INFO] Bootloader启动");

    while (true) {
        Local::Indicator();
        Local::Work();
        Ymodem::Work();
        UARTCmdQueue_Work();
    }
}