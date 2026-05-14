#pragma once
#include <vector>

enum class EdgePolarity { rising, falling };

struct InputConfig {
    int          id;
    EdgePolarity detect_edge;     // rising → flanc pujada (0→1); falling → flanc baixada (1→0)
    bool         detection_always; // true → sempre actiu; false → només si una sortida vinculada és ON
    std::vector<int> linked_outputs;
};
