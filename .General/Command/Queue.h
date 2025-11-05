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
extern std::map<std::string_view, std::function<int(int)>> Cmd_MAIN_Map;
extern std::map<std::string_view, std::function<int(int)>> Cmd_GET_Map;
extern std::map<std::string_view, std::function<int(int)>> Cmd_YMODEM_Map;

void UARTCmdQueue_Work();