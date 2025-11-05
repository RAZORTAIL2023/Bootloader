#include <cstdint>
#include <stdexcept>

class Bootloader {
public:
    struct ROM_t { const uint32_t StartAddr, Size,    EndAddr; } ROM; // 芯片ROM/Flash参数
    struct RAM_t { const uint32_t StartAddr, Size,    EndAddr; } RAM; // 芯片RAM/SRAM参数
    struct APP_t { const uint32_t StartAddr, MAXSize, EndAddr; } APP; // APP参数

    constexpr Bootloader(
        uint32_t ROM_StartAddr, uint32_t ROM_Size,
        uint32_t RAM_StartAddr, uint32_t RAM_Size,
        uint32_t BOOT_Size, uint32_t APP_Maxsize
    ):
        ROM{ROM_StartAddr, ROM_Size, ROM_StartAddr + ROM_Size},
        RAM{RAM_StartAddr, RAM_Size, RAM_StartAddr + RAM_Size},
        APP{ROM_StartAddr + BOOT_Size, APP_Maxsize, ROM_StartAddr + BOOT_Size + APP_Maxsize}
    {
        if (APP.EndAddr > ROM.EndAddr) {
            // 嵌入式不支持throw
            // throw std::logic_error("ROM Overflow: APP size exceeds available ROM space."); // 在constexpr函数里相当于static_assert
        }
    }

    bool IsJumpToAPP();
    void JumpToAPP();
    bool FlashErase();
    bool FlashProgram(const uint32_t startAddr, const uint8_t* const pData, size_t size);

} extern bootloader;