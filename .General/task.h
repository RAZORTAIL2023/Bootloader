#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <array>
#include <functional>
#include "ymodem.h"

enum class downloadMode
{
    By_Ymodem,
    By_Bluetooth,
    By_USB,
}extern const CurMode;

struct Bootloader
{
    /* 芯片ROM/Flash参数 */
    const uint32_t ROM_startAddr;
    const uint32_t ROM_size;
    const uint32_t ROM_endAddr;

    /* 芯片RAM/SRAM参数 */
    const uint32_t RAM_startAddr;
    const uint32_t RAM_size;
    const uint32_t RAM_endAddr;

    /* 用户定义Bootloader和APP的程序大小 */
    const uint32_t BOOT_size;
    const uint32_t APP_maxsize;
    const uint32_t APP_startAddr;
    const uint32_t APP_endAddr;

    /* 构造函数 */
    Bootloader
    (
        uint32_t _ROM_startAddr,
        uint32_t _ROM_size,
        uint32_t _RAM_startAddr,
        uint32_t _RAM_size,
        uint32_t _BOOT_size,
        uint32_t _APP_maxsize
    )
    :
        ROM_startAddr   (_ROM_startAddr),
        ROM_size        (_ROM_size),
        RAM_startAddr   (_RAM_startAddr),
        RAM_size        (_RAM_size),
        BOOT_size       (_BOOT_size),
        APP_maxsize     (_APP_maxsize),

        ROM_endAddr     (ROM_startAddr + ROM_size),
        RAM_endAddr     (RAM_startAddr + RAM_size),
        APP_startAddr   (ROM_startAddr + BOOT_size),
        APP_endAddr     (APP_startAddr + APP_maxsize)
    {
        assert(APP_endAddr <= ROM_endAddr && "ROM Overflow.");
    }

    char name[FILE_NAME_LENGTH+1];
    char size[FILE_SIZE_LENGTH+1];

    /* 用户定义方法开始 */
    void Delay(uint32_t ms);
    uint32_t GetTick();
    static void indicator();
    bool retainBootloaderCallback();
    void Ymodem_beforeReceiveCallback();
    bool Ymodem_RecvByte(uint8_t* pData, uint32_t Timeout);
    void Ymodem_SendByte(uint8_t  data);
    void FlashErase();
    bool FlashProgram(const uint32_t startAddr, const uint8_t* const pData, size_t size);
    void JumpToApp();
    /* 用户定义方法结束 */

    int Ymodem_CancelTransfer(int val) {
        Ymodem_SendByte(CA);
        Ymodem_SendByte(CA);
        return val;
    }
    int  Ymodem_Receive();
    int  Ymodem_Receive_Packet(uint8_t* pData, int* length, uint32_t Timeout);
    bool APPSizeCheck(size_t size) {
        return size > APP_maxsize ? false : true;
    }

}extern local_Bootloader;

void task_run();
void NON_BLOCKING_Delay(uint32_t* static_timeout, uint32_t time, const std::function<void(void)>& func);

#endif