#pragma once
#include <array>
#include <queue>
#include <map>
#include <string_view>
#include <functional>

struct UARTCmd {
    uint16_t len = 0;
    std::array<uint8_t, 128> data = {};
} extern saveCmd;

extern std::queue<UARTCmd> UARTCmdQueue;
extern std::array<std::string_view, 8> tokens;

void UARTCmdQueue_Work();