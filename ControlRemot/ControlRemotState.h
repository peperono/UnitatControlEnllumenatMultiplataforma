#pragma once
#include <atomic>
#include <mutex>
#include <unordered_map>

struct OutputInfo {
    bool state     = false;
    bool commanded = false;
    bool result    = false;
    bool remote    = false; // true = REMOTE, false = AUTO
};

struct ControlRemotState {
    std::mutex                          mtx;
    std::unordered_map<int, OutputInfo> outputsResult;
    std::atomic<bool>                   push_pending{false};
};

extern ControlRemotState cr_state;
