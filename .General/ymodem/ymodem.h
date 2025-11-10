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
        static bool Init();
        static bool RecvByte(uint8_t* pData);
        static void SendByte(uint8_t data);
        static bool RecvMultiByte(uint8_t* pData, uint16_t Size);
        static uint32_t EOT_Delay(); // 防止NAK回复过快
        static uint16_t CRC_Calculate(const uint8_t* pData, uint32_t Size);
    };

    class Callback {
    public:
        static void HeaderPacketCallback(std::string filename, uint32_t filesize);
        static bool GetDataCallback(const uint8_t* data, size_t len, bool isLastPacket);
        static void EndCallback();
        static void CpltCallback();
        static void ErrorCallback();
    };

private:
    static constexpr uint8_t SOH=0x01, STX=0x02, EOT=0x04, ACK=0x06, NAK=0x15, CAN=0x18, C=0x43;
    enum class Process { NONESTART, Recv_PacketHeader, Recv_Data, Recv_PacketTailer, END } CurProcess = Process::END;;
    enum class RecvResult { NONE, RIGHT, DUPLICATE, FAIL };
    
    std::array<uint8_t, 1029> packet;
    uint16_t packetLen;

    struct Args_structure {
        std::string filename;
        uint32_t filesize;
        bool GetFirstEOT = false;
        bool GetFirstCAN = false;
        int  totalErrors = 0;
        uint16_t processLen;
        uint32_t receivedBytes;
        uint8_t  PN; // packet number
        uint8_t  PN_Prev = 0xFF;
        bool isLastPacket = false;
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

    bool reset() {
        CurProcess = Process::NONESTART;
        Args.reset();
        packet.fill(0x00);
        return HAL::Init();
    }

    void start() {
        if (reset() == true) {
            HAL::SendByte(C);
            log("[YMODEM][INFO] 接收器启动. 请发送文件...");
        }
    }

    void end() {
        log("[YMODEM][INFO] 接收器强制停止.");
        CurProcess = Process::END;
        Callback::EndCallback();
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

    bool PacketTailerCheck() {
        if (packetLen==128 && Args.PN==0x00 && packet[3]==0x00) {
            // 检查数据区是否全为0x00(仅警告)
            for (int i=0; i<128; i++) {
                if (packet[3+i] != 0x00) {
                    log("[YMODEM][WARN] 结束帧数据区非空, 位置: %d, 值: 0x%02X\n", i, packet[3+i]);
                    break;
                }
            }
            return true;
        } else {
            return false;
        }
    }

    RecvResult EOT_Handle() {
        if (Args.GetFirstEOT == false) {
            Args.GetFirstEOT = true;
            uint32_t ms = HAL::EOT_Delay();
            if (ms > 0) log("[YMODEM][INFO] 第一个EOT回复延时 %u ms\n", ms);
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
            end();
            return RecvResult::NONE;
        }
    }

    void RecvFailHandle()  {
        log("[YMODEM][ERROR] 累计失败: %u/%u\n", Args.totalErrors+1, 5);

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