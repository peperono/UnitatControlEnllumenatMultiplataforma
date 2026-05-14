#pragma once
#include <atomic>
#include <mutex>
#include <string>

struct ControlHorariState {
    std::mutex        mtx;
    std::string       programacioHoraria;
    std::atomic<bool> load_pending{false};
};

extern ControlHorariState control_horari_state;
