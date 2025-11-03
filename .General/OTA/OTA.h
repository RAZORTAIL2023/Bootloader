#pragma once

#include <cstdint>
#include <vector>
#include <string_view>

class OTA {
public:
    static bool start(const uint32_t filesize);
    static bool transmit(const uint8_t sn, const uint8_t* pData, size_t len);
    static bool update();
    static bool verify();
    static bool unzip();
    enum class LogLevel { None, Light, Full } static inline logLevel = LogLevel::Full;

private:
    static constexpr std::string_view filename = "ota.bin";
    static inline uint32_t Baudrate = 921600;

    static std::vector<uint8_t> Generate_StartDownloadMsg(const uint32_t filesize);
    static std::vector<uint8_t> Generate_DataMsg(const std::vector<uint8_t>& data, uint8_t sn);
    static std::vector<uint8_t> Generate_StartUpdateMsg();

    static bool CheckACK_StartDownloadMsg(const std::vector<uint8_t>& msg);
    static bool CheckACK_DataMsg(const std::vector<uint8_t>& msg, uint8_t sn);
    static bool CheckACK_StartUpdateMsg(const std::vector<uint8_t>& msg);
    static bool CheckACK_VerifyMsg(const std::vector<uint8_t>& msg);
    static bool CheckACK_UNZIPMsg(const std::vector<uint8_t>& msg);

    class HAL {
    public:
        static std::vector<uint8_t> Receive(uint8_t Size, uint32_t timeout_ms);
        static void Transmit(std::vector<uint8_t>& msg);
    };
};