#include "ymodem.h"

Ymodem::RecvResult Ymodem::RecvPacket() {

    /* 帧头识别 */
    auto RecvFirstByte = [this]() -> RecvResult {
        if (HAL::RecvByte(&packet[0]) == true) {
            // 接收成功
            return RecvResult::RIGHT;
        } else if (CurProcess == Process::NONESTART) {
            // 尚未握手
            HAL::SendByte(C);
            return RecvResult::NONE;
        } else {
            // 接收失败
            log("[YMODEM][ERROR] 发送端无响应\n");
            return RecvResult::FAIL;
        }
    };

    if (auto result = RecvFirstByte(); result != RecvResult::RIGHT) return result;

    switch (packet[0]) {
        case SOH: packetLen = 128;
            break;
        case STX: packetLen = 1024;
            break;
        case EOT: packetLen = 1;
            return EOT_Handle();
        case CAN: packetLen = 1;
            return CANCEL_Handle();
        default:
            log("[YMODEM][ERROR] 未定义帧头: 0X%02X\n", packet[0]);
            return RecvResult::FAIL;
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
    Args.PN = *(iter+1);
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

Ymodem::Process Ymodem::RecvParse() {

    if (packetLen == 1) return CurProcess; // 对EOT/CANCEL帧透明

    /* 结束帧 */
    if (CurProcess==Process::Recv_PacketTailer) {
        log("[YMODEM][INFO] 收到结束帧");
        if (!PacketTailerCheck()) log("[YMODEM][ERROR] 结束帧无效, 包号: %u, 长度: %u, 数据区首字节: 0x%02X\n", Args.PN, packetLen, packet[3]);
        return Process::END;
    }

    /* 起始帧 */
    if (CurProcess == Process::NONESTART && Args.PN==0x00 && packetLen==128) {
        return Process::Recv_PacketHeader;
    }

    /* 数据帧 */
    if (CurProcess != Process::NONESTART) {
        return Process::Recv_Data;
    } else {
        log("[YMODEM][ERROR] 无起始帧的数据帧, 包号: %u, 长度: %u\n", Args.PN, packetLen);
        return Process::END;
    }
}

void Ymodem::work() {

    if (CurProcess == Process::END) return;

    switch (RecvPacket()) {
        case RecvResult::NONE: return;

        case RecvResult::FAIL:
            HAL::SendByte(NAK);
            RecvFailHandle();
            return;

        case RecvResult::DUPLICATE:
            HAL::SendByte(ACK);
            return;

        case RecvResult::RIGHT:
            Args.totalErrors = 0;
            break; // 只有RecvResult::RIGHT可以继续工作
    }

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
            Args.isLastPacket = LastPacketCheck();
            Args.processLen = Args.isLastPacket ? remain() : packetLen;
            if (Callback::GetDataCallback(packet.data()+3, Args.processLen, Args.isLastPacket) == true) {
                Args.receivedBytes += Args.processLen;
                if (logLevel >= LogLevel::FULL) log("[YMODEM][INFO] 已接收: %u/%u 字节\n", Args.receivedBytes, Args.filesize);
                HAL::SendByte(ACK);
            } else {
                log("[YMODEM][ERROR] 数据处理失败, 包号: %u\n", Args.PN);
                Args.PN_Prev--; // 接收失败，包号不前进
                HAL::SendByte(NAK);
                RecvFailHandle();
            }
            break;

        case Process::END:
            log("[YMODEM][INFO] 传输结束");
            HAL::SendByte(ACK);
            Callback::CpltCallback();
            break;

        default: break; // 对EOT/CANCEL帧透明
    }
}