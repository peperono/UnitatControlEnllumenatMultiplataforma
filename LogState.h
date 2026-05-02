#pragma once
#include "Rellotge/RellotgeState.h"
#include <atomic>
#include <mutex>
#include <string>
#include <vector>
#include <cstdio>

struct LogEntry {
    std::string time;
    std::string src;
    std::string sig;
    std::string detail;
};

struct LogState {
    std::mutex            mtx;
    std::vector<LogEntry> pending;
    std::atomic<bool>     push_pending{false};
};

extern LogState log_state;

inline void log_append(const char* src, const char* sig, std::string detail) {
    char tbuf[12];
    {
        std::lock_guard<std::mutex> lk(rellotge_state.mtx);
        std::snprintf(tbuf, sizeof(tbuf), "%02d:%02d:%02d",
                      rellotge_state.hour, rellotge_state.minute,
                      rellotge_state.second);
    }
    LogEntry entry{ tbuf, src, sig, std::move(detail) };
    {
        std::lock_guard<std::mutex> lk(log_state.mtx);
        log_state.pending.push_back(std::move(entry));
    }
    log_state.push_pending.store(true);
}
