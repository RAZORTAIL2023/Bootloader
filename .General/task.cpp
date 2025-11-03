#include "task.h"

void task_run()
{
    if (bootloader.IsJumpToAPP()) bootloader.JumpToApp();

    puts("Bootloader running...");

    while (true)
    {
        NON_BLOCKING_DELAY(500, []() { bootloader.Indicator(); });
        Ymodem::Work();
    }
}