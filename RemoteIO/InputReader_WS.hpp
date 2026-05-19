#pragma once
#include "../ControlEntrades/ControlEntrades.h"
#include "RemoteIOState.h"
#include <mutex>

inline IOReader makeWSInputReader() {
    return [](std::unordered_map<int, bool>& inputs,
              std::unordered_map<int, bool>&) {
        std::lock_guard<std::mutex> lk(remote_io_state.mtx);
        inputs = remote_io_state.inputs;
    };
}
