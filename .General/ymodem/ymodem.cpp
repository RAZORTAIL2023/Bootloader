#include "ymodem.h"
#include "Logger.h"

/**
 * @brief 接收数据包
 * 首先识别帧头, 若接收尚未开始, 则发送'C'请求开始, 否则返回失败;
 * 然后根据帧头类型判断帧长度并接收完整数据包;
 */
Ymodem::RecvResult Ymodem::RecvPacket() {

    auto RecvFirstByte = [this]() -> RecvResult {
        if (HAL::RecvByte(&packet[0]) == true) return RecvResult::RIGHT;
    
        if (CurProcess == Process::NONESTART) {
            HAL::SendByte(C);
            return RecvResult::NONE;
        } else {
            return RecvResult::FAIL;
        }
    };
    
    /* 接收帧头 */
    if (auto result = RecvFirstByte(); result != RecvResult::RIGHT) return result;
    
    /* 判断帧长度 */
    switch (packet[0]) {
    case SOH: packetLen = 128;  break;
    case STX: packetLen = 1024; break;
    case EOT: packetLen = 1; return EOT_Handle();
    case CAN: packetLen = 1; return CANCEL_Handle();
    default: log("[YMODEM][ERROR] 未定义帧头: 0X%02X\n", packet[0]); return RecvResult::FAIL;
    }

    /* 接收剩余数据 */
    auto iter = packet.begin();
    auto TO_UINT8_PTR = [](decltype(iter) it) { return &*it; };
    if (HAL::RecvMultiByte(TO_UINT8_PTR(iter+1), 2+packetLen+2) != true) {
        log("[YMODEM][ERROR] 数据包接收失败\n");
        return RecvResult::FAIL;
    }

    /* CRC校验 */
    uint16_t compute_crc = HAL::CRC_Calculate(TO_UINT8_PTR(iter+3), packetLen);
    uint16_t receive_crc = (packet[2+packetLen+1]<<8) | packet[2+packetLen+2];
    if (compute_crc != receive_crc) {
        log("[YMODEM][ERROR] CRC校验失败. 计算值: 0x%04X, 接收值: 0x%04X. \n", compute_crc, receive_crc);
        return RecvResult::FAIL;
    }

    /* 包号校验 */
    Args.PN  = *(iter+1);
    if ((Args.PN^0xFF) != *(iter+2)) {
        log("[YMODEM][ERROR] 包号不匹配. 包号: %u, 包号反码: %u\n", Args.PN, *(iter+2));
        return RecvResult::FAIL;
    }

    /* 包号顺序检查 */
    auto isPNRepeated = [this]() -> bool {
        return Args.PN == Args.PN_Prev;
    }; 
    auto isPNContinuous = [this]() -> bool {
        return CurProcess == Process::Recv_PacketTailer ? true : Args.PN == (uint8_t)(Args.PN_Prev + 1);
    };
    if (isPNRepeated()){
        if (logLevel == LogLevel::FULL) log("[YMODEM][WARN] 重复包号: %u\n", Args.PN);
        return RecvResult::DUPLICATE;
    }
    if (!isPNContinuous()) {
        log("[YMODEM][ERROR] 包号不连续. 上一个包号: %u, 当前包号: %u\n", Args.PN_Prev, Args.PN);
        return RecvResult::FAIL;
    }

    /* 接收完成 */
    Args.PN_Prev = Args.PN;
    if (logLevel == LogLevel::FULL) log("[YMODEM][INFO] 收到数据包，包号: %u, 长度: %u\n", Args.PN, packetLen);
    return RecvResult::RIGHT;
}

/**
 * @brief 起始帧解析
 * @brief SOH 0x00 0xFF filename 0x00 filesize 0x00 0x00(n) CRC(2)
 */
void Ymodem::HeaderPacketHandle() {
    auto iter = packet.begin();
    auto find = [this, &iter]() { return std::find(iter, packet.end(), 0x00); };
    iter += 3;

    auto nameLen = std::distance(iter, find());
    Args.filename = std::string((const char*)(&*iter), nameLen);
    iter += (nameLen+1);

    auto sizeLen = std::distance(iter, find());
    Args.filesize = std::stoul(std::string((const char*)(&*iter), sizeLen));
    iter += (sizeLen+1);

    log("[YMODEM][INFO] 文件名称: %s\n", Args.filename.c_str());
    log("[YMODEM][INFO] 文件大小: %u bytes\n", Args.filesize);

    Callback::HeaderPacketCallback(Args.filename, Args.filesize);
}

/**
 * @brief 分析帧类型
 */
Ymodem::Process Ymodem::RecvParse() {
    // 对EOT/CANCEL帧透明
    if (packetLen == 1) return CurProcess;

    // 结束帧
    if (CurProcess==Process::Recv_PacketTailer) {
        // 包号反码已经验过
        bool isValid = (packetLen==128 && Args.PN==0x00 && packet[3]==0x00);
        if (!isValid) {
            log("[YMODEM][ERROR] 结束帧无效, 包号: %u, 长度: %u, 数据区首字节: 0x%02X\n", Args.PN, packetLen, packet[3]);
            return Process::END;
        }
        // 进一步检查数据区是否全为0x00(仅警告)
        for (int i=0; i<128; i++) {
            if (packet[3+i] != 0x00) {
                log("[YMODEM][WARN] 结束帧数据区非空, 位置: %d, 值: 0x%02X\n", i, packet[3+i]);
                break;
            }
        }
        log("[YMODEM][INFO] 收到结束帧");
        return Process::END;
    }

    // 起始帧
    if (Args.PN==0x00 && packetLen==128 && CurProcess == Process::NONESTART) {
        return Process::Recv_PacketHeader;
    }
    
    // 数据帧
    if (CurProcess != Process::NONESTART) {
        return Process::Recv_Data;
    } else {
        log("[YMODEM][ERROR] 无起始帧的数据帧, 包号: %u, 长度: %u\n", Args.PN, packetLen);
        return Process::END;
    }
}

void Ymodem::work()
{
    if (CurProcess == Process::END) return;

    switch (RecvPacket()) {
    case RecvResult::NONE: return;
    case RecvResult::TERMINATE: CurProcess = Process::END; return;
    case RecvResult::FAIL: HAL::SendByte(NAK); RecvFailHandle(); return;
    case RecvResult::DUPLICATE: HAL::SendByte(ACK); return;
    case RecvResult::RIGHT: Args.totalErrors = 0; break;
    }

    // 对EOT/CANCEL帧透明
    switch (CurProcess = RecvParse()) {
    case Process::Recv_PacketHeader:
        log("[YMODEM][INFO] 收到起始帧, 正在解析...\n");
        HeaderPacketHandle();
        log("[YMODEM][INFO] 解析完毕, 正在请求数据帧...");
        CurProcess = Process::Recv_Data;
        HAL::SendByte(ACK);
        HAL::SendByte(C);
        break;

    case Process::Recv_Data:
        {
            bool isLastPacket = LastPacketCheck();
            Args.processLen = isLastPacket ? remain() : packetLen;
            if (Callback::GetDataCallback(packet.data()+3, Args.processLen, isLastPacket) == true) {
                Args.receivedBytes += Args.processLen;
                if (logLevel >= LogLevel::FULL) log("[YMODEM][INFO] 已接收: %u/%u 字节\n", Args.receivedBytes, Args.filesize);
                HAL::SendByte(ACK);
            } else {
                log("[YMODEM][ERROR] 数据处理失败，包号: %u\n", Args.PN);
                Args.PN_Prev--; // 接收失败，包号不前进
                HAL::SendByte(NAK);
                RecvFailHandle();
            }
        }
        break;

    case Process::END:
        log("[YMODEM][INFO] 传输结束");
        HAL::SendByte(ACK);
        Callback::CpltCallback();
        break;

    default: break;
    }
}