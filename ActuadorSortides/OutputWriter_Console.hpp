#pragma once
#include "ActuadorSortides.hpp"
#include <cstdio>

inline OutputWriter makeConsoleWriter() {
    return [](int id, bool actiu) {
        std::printf("[ActuadorSortides] S%d: %s\n", id, actiu ? "ON" : "OFF");
    };
}
