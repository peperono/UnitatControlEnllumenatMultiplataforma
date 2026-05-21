#pragma once
#include "Rellotge/RellotgeState.h"
#include <cstdio>
#include <mutex>
#include <string>

inline void log_append(const char* src, const char* sig, const std::string& detail) {
    char tbuf[12];
    {
        std::lock_guard<std::mutex> lk(rellotge_state.mtx);
        std::snprintf(tbuf, sizeof(tbuf), "%02d:%02d:%02d",
                      rellotge_state.hour, rellotge_state.minute,
                      rellotge_state.second);
    }
    std::printf("[%s] [%-16.16s] %s %s\n", tbuf, src, sig, detail.c_str());
}
