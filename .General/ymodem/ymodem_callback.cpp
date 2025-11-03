#include "ymodem.h"
#include "OTA.h"
#include "local.h"

static uint32_t flash_destination;

void Ymodem::Callback::HeaderPacketCallback(std::string filename, uint32_t filesize) {

    // 此函数是静态函数, 所以可以使用static变量

    static auto logic_flash = []() {
        flash_destination = bootloader.APP.StartAddr;
    };

    static auto logic_OTA = [&filesize]() {
        if (!OTA::start(filesize)) {
            puts("[USER][ERROR] OTA启动失败, 终止ymodem...");
            Ymodem::End();
        } else {
            puts("[USER][INFO] OTA启动成功, 等待数据传输...");
        }
    };

    if (CurMode == YmodemWorkMode::FlashProgramming) {
        logic_flash();
    } else if (CurMode == YmodemWorkMode::OTAUpdate) {
        logic_OTA();
    }
}

bool Ymodem::Callback::GetDataCallback(const uint8_t* pData, size_t len, bool isLastPacket) {

    // 此函数是静态函数, 所以可以使用static变量

    static auto logic_flash = [&pData, &len, &isLastPacket]() {
        static uint32_t flash_destination = bootloader.APP.StartAddr;
        if (bootloader.FlashProgram(flash_destination, pData, len) == true) {
            flash_destination += len;
            return true;
        } else {
            printf("[USER][ERROR] %s: Flash编程失败, 地址: 0x%08X, 长度: %u\n", __func__, flash_destination, len);
            return false;
        }
    };

    static auto logic_OTA = [&pData, &len, &isLastPacket]() {
        static uint8_t packet[512];
        static uint8_t sn;

        static auto CopyAndTransmit = [](const uint8_t* _pData, size_t _len) -> bool {
            memcpy(packet, _pData, _len);
            bool _result = OTA::transmit(sn++, packet, _len);
            if (!_result) sn--;
            return _result;
        };

        if (!isLastPacket) {
            if (len != 1024) {printf("[USER][ERROR] %s: 异常数据长度: %u\n", __func__, len); return false;}
            if (!CopyAndTransmit(pData, 512)) return false;
            if (!CopyAndTransmit(pData+512, 512)) return false;
            return true;
        }

        if (len > 512) {
            if (!CopyAndTransmit(pData, 512)) return false;
            if (!CopyAndTransmit(pData+512, len-512)) return false;
            return true;
        } else {
            return CopyAndTransmit(pData, len);
        }
    };

    if (CurMode == YmodemWorkMode::FlashProgramming) {
        return logic_flash();
    } else if (CurMode == YmodemWorkMode::OTAUpdate) {
        return logic_OTA();
    }

    return false;
}

void Ymodem::Callback::CpltCallback() {
    static auto logic_flash = []() {
        /* DO NOTHING */
    };

    static auto logic_OTA = []() {
        bool result = OTA::update();
        if (!result) {puts("[USER][ERROR] OTA更新失败"); return;}
        result = OTA::verify();
        if (!result) {puts("[USER][ERROR] OTA验证失败"); return;}
        result = OTA::unzip();
        if (!result) {puts("[USER][ERROR] OTA解压失败"); return;}

        puts("[USER][INFO] OTA流程结束");
    };

    if (CurMode == YmodemWorkMode::FlashProgramming) {
        logic_flash();
    } else if (CurMode == YmodemWorkMode::OTAUpdate) {
        logic_OTA();
    }
}