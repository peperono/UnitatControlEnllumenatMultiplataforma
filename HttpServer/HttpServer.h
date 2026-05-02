#pragma once
#include "../qpcpp/include/qpcpp.hpp"
#include <cstdint>

namespace HttpServer {
    void start(uint16_t port, QP::QActive* edgeDetector, QP::QActive* controlRemot);
    void stop();
}
