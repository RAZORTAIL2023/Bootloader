#pragma once

#include <cstdint>
#include <array>
#include <string>
#include <functional>
#include "Logger.h"

class Ymodem {
public:
    static void Work()  { GetInstance()->work(); }
    static void Start() { GetInstance()->start(); }
    static void End()   { GetInstance()->end(); }
    enum class LogLevel { NONE, LIGHT,  FULL } static constexpr logLevel = LogLevel::LIGHT;
    enum class LogMode  { NONE, PRINTF, USER } static inline    logMode  = LogMode::PRINTF;

    template<typename... Args>
    static bool log(const char* format, Args... args) {
        if constexpr (logLevel == LogLevel::NONE) return true;

        if (logMode == LogMode::PRINTF) {
            if constexpr (sizeof...(args) == 0) puts(format);
            else printf(format, args...);
            return true;
        } else if (logMode == LogMode::USER) {
            return Logger::push_back<Args...>(format, args...);
        } else {
            return true;
        }
    };

    class HAL {
    public:
        static void Init();
        static bool RecvByte(uint8_t* pData);
        static void SendByte(uint8_t data);
        static bool RecvMultiByte(uint8_t* pData, uint16_t Size);
        static uint16_t CRC_Calculate(const uint8_t* pData, uint32_t Size);
    };

    class Callback {
    public:
        static void HeaderPacketCallback(std::string filename, uint32_t filesize);
        static bool GetDataCallback(const uint8_t* data, size_t len, bool isLastPacket);
        static void CpltCallback();
        static void ErrorCallback();
    };

private:
    static constexpr uint8_t SOH=0x01, STX=0x02, EOT=0x04, ACK=0x06, NAK=0x15, CAN=0x18, C=0x43;
    enum class Process { NONESTART, Recv_PacketHeader, Recv_Data, Recv_PacketTailer, END } CurProcess = Process::END;;
    enum class RecvResult { NONE, RIGHT, DUPLICATE, FAIL, TERMINATE };
    
    std::array<uint8_t, 1029> packet;
    uint16_t packetLen;

    struct Args_structure {
        std::string filename;
        uint32_t filesize;
        bool GetFirstEOT;
        bool GetFirstCAN;
        int  totalErrors;
        uint16_t processLen;
        uint32_t receivedBytes; // 已接收的数据字节数
        uint8_t  PN; // packet number
        uint8_t  PN_Prev = 0xFF; // 必须初始化为0xFF，防止第一个包号为0时误判为重复包
        void reset() { *this = Args_structure(); }
    } Args;

    void HeaderPacketHandle();
    RecvResult RecvPacket();
    Process RecvParse();
    void work();
    static Ymodem* GetInstance() { static Ymodem Instance; return &Instance; }

    [[nodiscard]] uint32_t remain() {
        return Args.filesize - Args.receivedBytes;
    }

    void reset() {
        CurProcess = Process::NONESTART;
        Args.reset();
        packet.fill(0x00);
        HAL::Init();
    }

    void start() {
        reset();
        HAL::SendByte(C);
        log("[YMODEM][INFO] 接收器启动. 请发送文件...");
    }

    void end() {
        CurProcess = Process::END;
        log("[YMODEM][INFO] 接收器强制停止.");
    }

    bool LastPacketCheck() {
        uint32_t remainBytes = remain();
        
        bool result = (remainBytes < packetLen) && (remainBytes != 0);
        
        if (result) {
            log("[YMODEM][INFO] 最后一个数据包，实际长度: %u\n", remainBytes);

            if (packet[3+remainBytes] != 0x1A) {
                log("[YMODEM][WARN] 填充字节不是0x1A, 位置: %u, 值: 0x%02X\n", remainBytes, packet[3+remainBytes]);
            }
        }

        return result;
    }

    RecvResult EOT_Handle() {
        if (Args.GetFirstEOT == false) {
            Args.GetFirstEOT = true;
            HAL::SendByte(NAK);
            return RecvResult::NONE;
        } else {
            log("\n[YMODEM][INFO] 收到EOT, 正在请求结束帧...");
            HAL::SendByte(ACK);
            HAL::SendByte(C);
            CurProcess = Process::Recv_PacketTailer;
            return RecvResult::RIGHT;
        }
    }

    RecvResult CANCEL_Handle() {
        if (Args.GetFirstCAN == false) {
            Args.GetFirstCAN = true;
            return RecvResult::NONE;
        } else {
            log("\n[YMODEM][WARN] 用户取消传输");
            return RecvResult::TERMINATE;
        }
    }

    void RecvFailHandle()  {
        if (++Args.totalErrors >= 5) {
            log("\n[YMODEM][ERROR] 错误次数太多, 传输失败, 正在取消...");
            HAL::SendByte(CAN);
            HAL::SendByte(CAN);
            CurProcess = Process::END;
            log("[YMODEM][INFO] Ymodem已关闭.");
            Callback::ErrorCallback();
        }
    }
};