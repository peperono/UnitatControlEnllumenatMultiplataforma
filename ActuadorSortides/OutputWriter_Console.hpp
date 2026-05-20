#pragma once
#include "ActuadorSortides.hpp"

inline OutputWriter makeConsoleWriter() {
    return [](int, bool) {};
}
