#pragma once
#include <atomic>
#include <mutex>

struct RellotgeState {
    std::mutex        mtx;
    int               hour   = 0;
    int               minute = 0;
    int               second = 0;
    int               wday   = 0; // 0=dilluns..6=diumenge
    std::atomic<bool> push_pending{false};
};

extern RellotgeState rellotge_state;
