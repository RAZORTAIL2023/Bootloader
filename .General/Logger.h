#pragma once

#include <stdio.h>
#include <cstring>
#include <string>
#include <algorithm>
#include <array>
#include <vector>
#include <queue>
#include "monotonic_buffer_resource_ex.h"

class Logger {
private:
    static inline std::array<std::byte, 0x2000> buffer;
    static inline std::pmr::monotonic_buffer_resource pool{buffer.data(), buffer.size(), std::pmr::null_memory_resource()};
    static inline monotonic_buffer_resource_ex poolEx{&pool, buffer.size()};
    static inline std::queue<std::pmr::string, std::pmr::deque<std::pmr::string>> logQueue{std::pmr::deque<std::pmr::string>{&poolEx}};

    static std::pmr::string construct(const char* format) {
        return std::pmr::string{format, std::min(strlen(format), size_t{255}), &poolEx};
    }

    template<typename... Args>
    static std::pmr::string construct(const char* format, Args... args) {
        char temp[256];
        int len = snprintf(temp, sizeof(temp), format, args...);
        if (len > 0) {
            size_t actual_len = (len < sizeof(temp)) ? len : sizeof(temp) - 1;
            return std::pmr::string{temp, actual_len, &poolEx};
        } else {
            return std::pmr::string{};
        }
    }

    static bool save(const std::pmr::string& str) {
        const size_t required_size = (str.size()+1) + sizeof(std::pmr::string) + 100;
        if (poolEx.remain() < required_size) return false;
        logQueue.push(str);
        return true;
    }

public:
    template<typename... Args>
    static bool push_back(const char* format, Args... args) {
        auto str = construct(format, args...);
        if (!str.empty()) {
            return save(str);
        } else {
            return false;
        }
    }

    static void printAll() {
        while (!logQueue.empty()) {
            const auto& str = logQueue.front();
            printf("%s", str.c_str());
            logQueue.pop();
        }
        poolEx.release();
    }
};