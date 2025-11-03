#pragma once

#include <stdint.h>
#include <stdio.h>
#include <functional>
#include "bootloader.h"
#include "ymodem.h"

void NON_BLOCKING_Delay(uint32_t* static_timeout, uint32_t time, const std::function<void(void)>& func);
// 此宏不可在成员方法中调用，因为静态局部变量不属于独立对象
#define NON_BLOCKING_DELAY(time, func) do { \
    static uint32_t timeout_##__COUNTER__ = 0; \
    NON_BLOCKING_Delay(&timeout_##__COUNTER__, time, func); \
} while(0)
void task_run();