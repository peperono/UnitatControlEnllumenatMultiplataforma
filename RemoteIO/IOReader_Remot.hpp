#pragma once
#include "../DigitalEdgeDetector/DigitalEdgeDetector.h"
#include "RemoteIOState.h"
#include <mutex>

inline IOReader makeRemoteReader() {
    return [](std::unordered_map<int, bool>& inputs,
              std::unordered_map<int, bool>& outputs) {
        std::lock_guard<std::mutex> lk(remote_io_state.mtx);
        inputs  = remote_io_state.inputs;
        outputs = remote_io_state.outputs;
    };
}
