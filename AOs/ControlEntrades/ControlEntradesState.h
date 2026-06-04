#pragma once
#include "ControlEntrades/InputConfig.h"
#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

// ── ControlEntradesState ───────────────────────────────────────────────────────────────
// Shared between the QV thread (ControlEntrades writes) and the Mongoose HTTP thread
// (HttpServer reads). Access must be guarded by mtx.

struct ControlEntradesState {
    std::mutex                    mtx;
    std::unordered_map<int, bool> inputs;       // true = interruptor tancat
    std::unordered_map<int, bool> outputs;      // true = sortida activa
    std::vector<int>              last_edges;   // IDs amb flanc detectat en el darrer poll
    std::unordered_map<int, int>  edge_counts;  // nombre acumulat de flancs per ID d'entrada
    std::atomic<bool>             push_pending{false}; // activat per ControlEntrades, esborrat pel fil Mongoose
    std::vector<InputConfig>      config_inputs; // configurat a l'inici i en cada RECONFIGURE_SIG
};

extern ControlEntradesState control_entrades_state;
