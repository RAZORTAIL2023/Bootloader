#include "Queue.h"
#include "local.h"
#include "Execute.h"

/**
 * @param  当前指令在tokens数组中的起始位置
 * @return 消耗的token数量
 */
std::map<std::string_view, std::function<int(int)>> Cmd_MAIN_Map = {
    {"HELP", Cmd_MAIN_Functions::HELP},
    {"GET", Cmd_MAIN_Functions::GET},
    {"YMODEM", Cmd_MAIN_Functions::YMODEM},
    {"JUMP", Cmd_MAIN_Functions::JUMP},
    {"PRINT", Cmd_MAIN_Functions::PRINT},
};

std::map<std::string_view, std::function<int(int)>> Cmd_GET_Map = {
    {"HELP", Cmd_GET_Functions::HELP},
    {"VERSION", Cmd_GET_Functions::VERSION},
};

std::map<std::string_view, std::function<int(int)>> Cmd_YMODEM_Map = {
    {"HELP", Cmd_YMODEM_Functions::HELP},
    {"UPDATE", Cmd_YMODEM_Functions::UPDATE},
    {"OTA", Cmd_YMODEM_Functions::OTA},
    {"END", Cmd_YMODEM_Functions::END},
};