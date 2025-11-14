#include "OTA.h"
#include <numeric>

std::vector<uint8_t> OTA::Generate_StartDownloadMsg(const uint32_t filesize) {
    std::vector<uint8_t> msg;
    
    msg.push_back(0xEE);
    msg.push_back(0xB6);

    // Len
    const int Len = 1 + 1 + 4 + 4 + filename.size()-1; // 不要\0
    msg.push_back((Len >> 8) & 0xFF);
    msg.push_back(Len & 0xFF);

    msg.push_back(0x88);
    msg.push_back(0x11); // 开始下载
    
    // Baudrate
    msg.push_back((Baudrate >> 24) & 0xFF);
    msg.push_back((Baudrate >> 16) & 0xFF);
    msg.push_back((Baudrate >> 8u) & 0xFF);
    msg.push_back(Baudrate & 0xFF);

    // filesize
    msg.push_back((filesize >> 24) & 0xFF);
    msg.push_back((filesize >> 16) & 0xFF);
    msg.push_back((filesize >> 8u) & 0xFF);
    msg.push_back(filesize & 0xFF);

    // down_name
    for (const char c : filename) {
        msg.push_back(c);
    }

    msg.push_back(0xFF);
    msg.push_back(0xFC);
    msg.push_back(0xFF);
    msg.push_back(0xFF);

    // 0xEE 0xB6 Len(2) 0x88 0x11 Baudrate(4) filesize(4) down_name(ota.bin) 0xFF 0xFC 0xFF 0xFF
    // Start from 0x88, Len = 1 + 1 + 4 + 4 + 7 = 17
    return msg;
}

std::vector<uint8_t> OTA::Generate_DataMsg(const std::vector<uint8_t>& data, const uint8_t sn) {
    std::vector<uint8_t> msg;

    msg.push_back(0xEE);
    msg.push_back(0xB6);

    // Len
    const int Len = 1 + 1 + 1 + data.size() + 2;
    msg.push_back((Len >> 8) & 0xFF);
    msg.push_back(Len & 0xFF);

    // 0x88 0x22
    msg.push_back(0x88);
    msg.push_back(0x22); // 数据传输

    // sn 包序号，0-255循环重复使用
    msg.push_back(sn);

    // packet
    for (const auto& byte : data) msg.push_back(byte);

    // checksum 0xB6+Len+0x88+0x22+sn+packet，求和取反，高字节在前，低字节在后
    if (msg.size() < 1) return {}; // 防止越界
    uint32_t sum = std::accumulate(msg.begin()+1, msg.end(), 0U);
    uint16_t checksum = ~(sum & 0xFFFF);
    msg.push_back((checksum >> 8) & 0xFF);
    msg.push_back(checksum & 0xFF);

    msg.push_back(0xFF);
    msg.push_back(0xFC);
    msg.push_back(0xFF);
    msg.push_back(0xFF);

    // 0xEE 0xB6 Len(2) 0x88 0x22 sn(1) packet(0~512) checksum(2) 0xFF 0xFC 0xFF 0xFF
    // Start from 0x88, Len = 1 + 1 + 1 + n + 2 = n + 5
    return msg;
}

std::vector<uint8_t> OTA::Generate_StartUpdateMsg() {
    std::vector<uint8_t> msg;

    msg.push_back(0xEE);
    msg.push_back(0xB6);

    // Len
    const int Len = 1 + 1 + 1;
    msg.push_back((Len >> 8) & 0xFF);
    msg.push_back(Len & 0xFF);

    msg.push_back(0x88);
    msg.push_back(0x33); // 开始升级
    msg.push_back(0x01);

    msg.push_back(0xFF);
    msg.push_back(0xFC);
    msg.push_back(0xFF);
    msg.push_back(0xFF);

    // 0xEE 0xB6 Len(2) 0x88 0x33 0x01 0xFF 0xFC 0xFF 0xFF
    // Len = 1 + 1 + 1 = 3
    return msg;
}

bool OTA::CheckACK_StartDownloadMsg(const std::vector<uint8_t>& msg) {
    std::vector<uint8_t> ACK  = {0xEE, 0xA6, 0x11, 0x80, 0xFF, 0xFC, 0xFF, 0xFF};
    std::vector<uint8_t> NACK = {0xEE, 0xA6, 0x11, 0x81, 0xFF, 0xFC, 0xFF, 0xFF};

    if (msg.size()==8 && std::equal(msg.begin(), msg.end(), ACK.begin())) {
        log("[OTA][INFO] ACK_开始下载");
        return true;
    }
    
    if (std::equal(msg.begin(), msg.end(), NACK.begin())) {
        log("[OTA][ERROR] NACK_开始下载");
        return false;
    }
    
    log("[OTA][ERROR] 未知消息: %s\n", __func__);
    for (const auto& byte : msg) log("%02X ", byte);
    log("");
    return false;
}

bool OTA::CheckACK_DataMsg(const std::vector<uint8_t>& msg, uint8_t sn) {
    std::vector<uint8_t> ACK    = {0xEE, 0xA6, 0x22, 0x80, sn, 0xFF, 0xFC, 0xFF, 0xFF};
    std::vector<uint8_t> NACK_1 = {0xEE, 0xA6, 0x22, 0x82, sn, 0xFF, 0xFC, 0xFF, 0xFF};
    std::vector<uint8_t> NACK_2 = {0xEE, 0xA6, 0x22, 0x83, sn, 0xFF, 0xFC, 0xFF, 0xFF};

    if (msg.size()==9 && std::equal(msg.begin(), msg.begin()+4, ACK.begin())) {
        // log("[OTA][INFO] ACK_数据包");
        return true;
    }

    if (msg.size()==9 && std::equal(msg.begin(), msg.begin()+4, NACK_1.begin())) {
        log("[OTA][ERROR] NACK_SN_数据包, 包号: %u\n", sn);
        return false;
    }

    if (msg.size()==9 && std::equal(msg.begin(), msg.begin()+4, NACK_2.begin())) {
        log("[OTA][ERROR] NACK_CHECKSUM_数据包, 包号: %u\n", sn);
        return false;
    }

    log("[OTA][ERROR] 未知消息: %s\n", __func__);
    for (const auto& byte : msg) log("%02X ", byte);
    log("");
    return false;
}

bool OTA::CheckACK_StartUpdateMsg(const std::vector<uint8_t>& msg) {
    std::vector<uint8_t> ACK = {0xEE, 0xA6, 0x33, 0x80, 0xFF, 0xFC, 0xFF, 0xFF};

    if (msg.size()==8 && std::equal(msg.begin(), msg.end(), ACK.begin())) {
        log("[OTA][INFO] ACK_开始更新");
        return true;
    }

    log("[OTA][ERROR] 未知消息: %s\n", __func__);
    for (const auto& byte : msg) log("%02X ", byte);
    log("");
    return false;
}

bool OTA::CheckACK_VerifyMsg(const std::vector<uint8_t>& msg) {
    std::vector<uint8_t> ACK  = {0xEE, 0xA6, 0x55, 0x01, 0xFF, 0xFC, 0xFF, 0xFF};
    std::vector<uint8_t> NACK = {0xEE, 0xA6, 0x55, 0x00, 0xFF, 0xFC, 0xFF, 0xFF};

    if (msg.size()==8 && std::equal(msg.begin(), msg.end(), ACK.begin())) {
        log("[OTA][INFO] ACK_文件校验");
        return true;
    }

    if (msg.size()==8 && std::equal(msg.begin(), msg.end(), NACK.begin())) {
        log("[OTA][EEROR] NACK_文件校验");
        return false;
    }

    log("[OTA][ERROR] 未知消息: %s\n", __func__);
    for (const auto& byte : msg) log("%02X ", byte);
    log("");
    return false;
}

bool OTA::CheckACK_UNZIPMsg(const std::vector<uint8_t>& msg) {
    std::vector<uint8_t> ACK  = {0xEE, 0xA6, 0x66, 0x01, 0xFF, 0xFC, 0xFF, 0xFF};
    std::vector<uint8_t> NACK = {0xEE, 0xA6, 0x66, 0x00, 0xFF, 0xFC, 0xFF, 0xFF};

    if (msg.size()==8 && std::equal(msg.begin(), msg.end(), ACK.begin())) {
        log("[OTA][INFO] ACK_文件解压");
        return true;
    }

    if (msg.size()==8 && std::equal(msg.begin(), msg.end(), NACK.begin())) {
        log("[OTA][EEROR] NACK_文件解压");
        return false;
    }


    log("[OTA][ERROR] 未知消息: %s\n", __func__);
    for (const auto& byte : msg) log("%02X ", byte);
    log("");
    return false;
}

bool OTA::start(const uint32_t filesize) {
    log("\n[OTA][PROCESS] OTA开始: 文件大小=%u字节, 波特率=%u, 文件名=%s\n", filesize, Baudrate, filename.data());
    std::vector<uint8_t> StartDownloadMsg = Generate_StartDownloadMsg(filesize);
    log("[OTA][INFO] 已生成开始下载消息:");
    for (const auto& byte : StartDownloadMsg) log("%02X ", byte);
    log("");

    HAL::Transmit(StartDownloadMsg);
    log("[OTA][INFO] 已发送开始下载消息, 等待设备响应...");
    std::vector<uint8_t> RecvMsg = HAL::Receive(8, 6000);
    if (!RecvMsg.empty() && CheckACK_StartDownloadMsg(RecvMsg)) {
        log("[OTA][INFO] 开始下载: 设备响应正常");
        return true;
    } else {
        log("[OTA][ERROR] 开始下载: 设备响应异常");
        return false;
    }
}

bool OTA::transmit(const uint8_t sn, const uint8_t* pData, size_t len) {
    /* 构造并发送数据包 */
    std::vector<uint8_t> data(pData, pData+len);
    std::vector<uint8_t> DataMsg = Generate_DataMsg(data, sn);
    HAL::Transmit(DataMsg);
    /* 接收设备回应 */
    std::vector<uint8_t> RecvMsg = HAL::Receive(9, 8000);
    if (!RecvMsg.empty() && CheckACK_DataMsg(RecvMsg, sn)) {
        // log("[OTA][INFO] 数据包传输完成: 包号: %u, 长度: %u\n", sn, data.size());
        return true;
    } else {
        log("[OTA][ERROR] 数据包传输失败: 设备响应异常");
        return false;
    }
}

bool OTA::update() {
    log("\n[OTA][PROCESS] OTA发送更新命令");
    std::vector<uint8_t> StartUpdateMsg = Generate_StartUpdateMsg();
    HAL::Transmit(StartUpdateMsg);
    log("[OTA][INFO] 已生成并发送开始更新消息, 等待设备响应...");
    std::vector<uint8_t> RecvMsg = HAL::Receive(8, 5000);
    if (!RecvMsg.empty() && CheckACK_StartUpdateMsg(RecvMsg)) {
        log("[OTA][INFO] 开始更新: 设备响应正常");
        return true;
    } else {
        log("[OTA][ERROR] 开始更新: 设备响应异常");
        return false;
    }
}

bool OTA::verify() {
    log("\n[OTA][PROCESS] 等待设备验证...");
    std::vector<uint8_t> RecvMsg = HAL::Receive(8, 10000);
    if (!RecvMsg.empty() && CheckACK_VerifyMsg(RecvMsg)) {
        log("[OTA][INFO] 验证完成: 设备响应正常");
        return true;
    } else {
        log("[OTA][ERROR] 验证失败: 设备响应异常");
        return false;
    }
}

bool OTA::unzip() {
    log("\n[OTA][PROCESS] 等待设备解压...");
    std::vector<uint8_t> RecvMsg = HAL::Receive(8, 60000);
    if (!RecvMsg.empty() && CheckACK_UNZIPMsg(RecvMsg)) {
        log("[OTA][INFO] 解压完成: 设备响应正常");
        return true;
    } else {
        log("[OTA][ERROR] 解压失败: 设备响应异常");
        return false;
    }
}