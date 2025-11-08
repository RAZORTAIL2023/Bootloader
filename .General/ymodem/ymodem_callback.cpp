#include "ymodem.h"
#include "OTA.h"
#include "local.h"

static uint32_t flash_destination;

void Ymodem::Callback::HeaderPacketCallback(std::string filename, uint32_t filesize) {
    
    static auto logic_flash = []() {
        flash_destination = bootloader.APP.StartAddr;
    };

    static auto logic_OTA = [&filesize]() {
        if (!OTA::start(filesize)) {
            log("[BOOT][ERROR] OTA启动失败, 终止ymodem...");
            Ymodem::End();
        } else {
            log("[BOOT][INFO] OTA启动成功, 等待数据传输...");
        }
    };

    if (CurMode == YmodemWorkMode::FlashProgramming) {
        logic_flash();
    } else if (CurMode == YmodemWorkMode::OTAUpdate) {
        logic_OTA();
    }
}

bool Ymodem::Callback::GetDataCallback(const uint8_t* pData, size_t len, bool isLastPacket) {

    static auto logic_flash = [&pData, &len, &isLastPacket]() {
        static uint32_t flash_destination = bootloader.APP.StartAddr;
        if (bootloader.FlashProgram(flash_destination, pData, len) == true) {
            flash_destination += len;
            return true;
        } else {
            log("[BOOT][ERROR] %s: Flash编程失败, 地址: 0x%08X, 长度: %u\n", __func__, flash_destination, len);
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
            if (len != 1024) {log("[BOOT][ERROR] %s: 异常数据长度: %u\n", __func__, len); return false;}
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
    if (CurMode == YmodemWorkMode::OTAUpdate) {
        bool result = OTA::update();
        if (!result) {log("[BOOT][ERROR] OTA更新失败"); return;}
        result = OTA::verify();
        if (!result) {log("[BOOT][ERROR] OTA验证失败"); return;}
        result = OTA::unzip();
        if (!result) {log("[BOOT][ERROR] OTA解压失败"); return;}
        log("[BOOT][INFO] OTA流程结束");
    }

    HAL_UARTEx_ReceiveToIdle_DMA(&PC_huart, saveCmd.data.data(), sizeof(saveCmd.data)-1);
    log("[BOOT][INFO] Ymodem完成, 重新打开串口空闲中断.");
    log("[BOOT][INFO] 现在打印所有Ymodem传输日志:");
    Logger::printAll();
}

void Ymodem::Callback::ErrorCallback() {
    HAL_UARTEx_ReceiveToIdle_DMA(&PC_huart, saveCmd.data.data(), sizeof(saveCmd.data)-1);
    log("[BOOT][INFO] Ymodem完成, 重新打开串口空闲中断.");
    log("[BOOT][INFO] 现在打印所有Ymodem传输日志:");
    Logger::printAll();
}