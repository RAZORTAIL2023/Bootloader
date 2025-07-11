#include "task.h"


const downloadMode CurMode = downloadMode::By_Ymodem;

void task_run()
{
    local_Bootloader.Delay(500);
    if(local_Bootloader.retainBootloaderCallback() == false) local_Bootloader.JumpToApp();
    puts("Bootloader running...");

    while (true)
    {
        Bootloader::indicator();
        switch (CurMode) {
        case downloadMode::By_Ymodem:
            {
                puts("Ymodem download start...");
                
                puts("Now Erase Flash..");
                local_Bootloader.FlashErase();
                puts("Erase Flash Finished.");
                
                local_Bootloader.Ymodem_beforeReceiveCallback();
                puts("Now lanuch Blocking Ymodem, Please send file...");
                int result = local_Bootloader.Ymodem_Receive();
                
                local_Bootloader.Delay(2500);
                switch (result)
                {
                case 0:
                    printf("File Name: %s\n", local_Bootloader.name);
                    printf("File Size: %s\n", local_Bootloader.size);
                    puts("Ymodem download finished, Now Jump to APP...\n");
                    local_Bootloader.JumpToApp();
                    break;

                case -1: puts("File Size Too Large.\n"); break;
                case -2: puts("Flash Program Error.\n"); break;
                default: break;
                }
                puts("Unknown Error, Now Retry...\n"); break;
            }
            break;
        case downloadMode::By_Bluetooth: break;
        case downloadMode::By_USB: break;
        }
    }
}

void NON_BLOCKING_Delay(uint32_t* static_timeout, uint32_t time, const std::function<void(void)>& func)
{
    if (local_Bootloader.GetTick() - *static_timeout > time)
    {
        *static_timeout = local_Bootloader.GetTick();
        func();
    }
}