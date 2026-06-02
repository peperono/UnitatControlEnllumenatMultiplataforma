#pragma once
#include "Rellotge/RellotgeState.h"
#include <cstdio>
#include <mutex>
#include <string>

inline void log_append(const char* src, const char* sig, const std::string& detail) {
    static const char* DIES[7] = {"dl","dt","dc","dj","dv","ds","dg"};
    static std::mutex log_mtx;
    char tbuf[16];
    {
        std::lock_guard<std::mutex> lk(rellotge_state.mtx);
        std::snprintf(tbuf, sizeof(tbuf), "%s %02d:%02d:%02d",
                      DIES[rellotge_state.wday],
                      rellotge_state.hour, rellotge_state.minute,
                      rellotge_state.second);
    }
    std::lock_guard<std::mutex> lk(log_mtx);
    std::printf("[%s] [%-16.16s] %s %s\n", tbuf, src, sig, detail.c_str());
}
