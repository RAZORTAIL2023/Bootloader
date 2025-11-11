/* 仅被Dispatch.cpp包含 */

namespace Cmd_MAIN_Functions {
    int HELP(int) {
        puts("[BOOT][INFO] Available Commands:");
        for (const auto& pair : Cmd_MAIN_Map) {
            printf(" - %s\n", pair.first.data());
        }
        return 1;
    }

    int GET(int idx) {
        if (idx+1 < tokens.size()) { // 防止数组越界
            auto iter = Cmd_GET_Map.find(tokens[idx+1]);
            if (iter != Cmd_GET_Map.end()) {
                iter->second(idx+1);
                return 2;
            }
        }
        printf("[BOOT][ERROR] \"%s\" no matched sub command.\n", __func__);
        return 1; // 仅消耗 "GET"
    }

    int YMODEM(int idx) {
        if (idx+1 < tokens.size()) {
            auto iter = Cmd_YMODEM_Map.find(tokens[idx+1]);
            if (iter != Cmd_YMODEM_Map.end()) {
                iter->second(idx+1);
                return 2;
            }
        }
        printf("[BOOT][ERROR] \"%s\" no matched sub command.\n", __func__);
        return 1;
    }

    int JUMP(int) {
        bootloader.JumpToAPP();
        return 1;
    }

    int PRINT(int) {
        if (Logger::empty()) {
            puts("[BOOT][INFO] 没有存储的日志");   
        } else {
            puts("[BOOT][INFO] 现在打印所有存储日志:");
            Logger::printAll();
        }
        return 1;
    }
}

namespace Cmd_GET_Functions {
    int HELP(int) {
        for (const auto& pair : Cmd_GET_Map) {
            printf(" - %s\n", pair.first.data());
        }
        return 1;
    }

    int VERSION(int) {
        puts("[BOOT][REPLY] VERSION 1.0.0.0");
        return 1;
    }
}

namespace Cmd_YMODEM_Functions {
    int HELP(int) {
        for (const auto& pair : Cmd_YMODEM_Map) {
            printf(" - %s\n", pair.first.data());
        }
        return 1;
    }

    int UPDATE(int) {
        CurMode = YmodemWorkMode::FlashProgramming;
        puts("[BOOT][INFO] 在线升级模式已启用");
        Ymodem::Start();
        return 1;
    }

    int OTA(int) {
        CurMode = YmodemWorkMode::OTAUpdate;
        puts("[BOOT][INFO] LCD-OTA升级模式已启用");
        Ymodem::Start();
        return 1;
    }

    int END(int) {
        puts("[BOOT][INFO] 用户终止Ymodem传输");
        Ymodem::End();
        return 1;
    }
}

/* 仅被Dispatch.cpp包含 */