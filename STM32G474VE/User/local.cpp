#include "local.h"

YmodemWorkMode CurMode = YmodemWorkMode::FlashProgramming;

Bootloader bootloader
(
    0x08000000, // ROM起始地址
    0x80000,    // ROM大小
    0x20000000, // RAM起始地址
    0x20000,    // RAM大小
    0x20000,    // BOOT大小
    0x40000     // APP大小
);