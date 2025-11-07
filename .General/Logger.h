#pragma once

#include <string>
#include <array>
#include <algorithm>

class Logger {
public:
    template<typename... Args>
    static std::string construct(const char* format, Args... args) {
        char temp[256]; // // 可以根据snprintf返回值创建std::array或std::vector等, 这里是一个简单实现
        int len = snprintf(temp, sizeof(temp), format, args...);
        if (len > 0 && len < sizeof(temp)) {
            return std::string(temp, len);
        }
        return {};
    }

    static void save(const std::string& message) {

    }

    
};

class RingBuffer {
    public:
        bool write(const uint8_t* data, size_t len) {
            if (len > free()) return false;

            const int Part1_Len = std::min((int)len, writeTailSpace());
            const int Part2_Len = len - Part1_Len;
            
            const uint8_t* Part1_Ptr = data;
            const uint8_t* Part2_Ptr = data + Part1_Len;
            
            memcpy(&buf[writeIdx], Part1_Ptr, Part1_Len);
            memcpy(&buf[0],        Part2_Ptr, Part2_Len);

            writeIdx = (writeIdx+len) % buf.size();
            used += len;

            return true;
        }

        bool read(uint8_t* out, size_t len) {
            if (len > used) return false;

            const int Part1_Len = std::min((int)len, readTailSpace());
            const int Part2_Len = len - Part1_Len;

            uint8_t* Part1_Ptr = out;
            uint8_t* Part2_Ptr = out + Part1_Len;

            memcpy(Part1_Ptr, &buf[readIdx], Part1_Len);
            memcpy(Part2_Ptr, &buf[0],       Part2_Len);

            readIdx = (readIdx + len) % buf.size();
            used -= len;
            return true;
        }

    private:
        std::array <uint8_t, 4096> buf;
        int writeIdx=0, readIdx=0, used=0;
        int free() { return buf.size() - used; }
        int writeTailSpace() { return buf.size() - writeIdx; }
        int readTailSpace () { return buf.size() - readIdx;  }

};