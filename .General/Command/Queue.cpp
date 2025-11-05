#include "Queue.h"

UARTCmd saveCmd;
std::queue<UARTCmd> UARTCmdQueue;
std::array<std::string_view, 8> tokens;

static char* strupr(char* string) {
    char* origin = string;
    while (*string) {
        if(*string >= 'a' && *string <= 'z') *string = *string - 'a' + 'A';
        string++;
    }
    return origin;
}

static void strupr(uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (data[i] >= 'a' && data[i] <= 'z') {
            data[i] = data[i] - 'a' + 'A';
        }
    }
}

static std::string_view trim(std::string_view str) {
    // 去掉尾部的 \r 和 \n
    while (!str.empty() && (str.back() == '\r' || str.back() == '\n' || str.back() == ' ')) str.remove_suffix(1);
    // 去掉头部空格
    while (!str.empty() && str.front() == ' ') str.remove_prefix(1);
    return str;
}

static size_t split(std::string_view str, std::string_view out[], int max_tokens) {
    int i = 0, tokenCount = 0;

    while (i < str.size() && tokenCount < max_tokens) {
        // 跳过连续空格
        while (i < str.size() && str[i] == ' ') ++i;
        if (i >= str.size()) break;
        // 遍历非空字符
        int j = i;
        while (j < str.size() && str[j] != ' ') ++j;
        out[tokenCount++] = str.substr(i, j - i);
        i = j;
    }
    return tokenCount;
}

static void Process(UARTCmd& cmd) {
    std::string_view str = trim(std::string_view(reinterpret_cast<const char*>(cmd.data.data()), cmd.len));
    int tokenCount = split(str, tokens.data(), tokens.size());

    for (int i=0; i<tokenCount; ) {
        auto iterator = Cmd_MAIN_Map.find(tokens[i]);
        if (iterator != Cmd_MAIN_Map.end()) {
            i += iterator->second(i);
        } else {
            ++i; // 未知指令，跳过
        }
    }
}

void UARTCmdQueue_Work() {
    if (!UARTCmdQueue.empty()) {
        auto cmd = UARTCmdQueue.front();
        strupr(cmd.data.data(), cmd.len);
        Process(cmd);
        UARTCmdQueue.pop();
    }
}