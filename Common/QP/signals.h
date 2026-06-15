#pragma once
#include "qpcpp/include/qpcpp.hpp"
#include "ControlEntrades/InputConfig.h"
#include <unordered_map>
#include <vector>

// ── Signals ───────────────────────────────────────────────────────────────────

enum Signals : QP::QSignal {
    // Control Entrades
    INPUT_CHANGED_SIG = QP::Q_USER_SIG,
    EDGE_DETECTED_SIG,
    EDGE_DETECTOR_POLL_SIG,
    RECONFIGURE_SIG,
    // Control Sortides
    OUTPUT_STATE_SIG,
    CTRL_OUTPUT_CMD_SIG,
    CTRL_OUTPUT_MODE_SIG,
    CTRL_OUTPUT_RETURN_AUTO_SIG,
    CTRL_OUTPUT_DELETE_SIG,
    CTRL_REMOT_INIT_SIG,
    OUTPUT_RESULT_SIG,
    RELLOTGE_TICK_INTERNAL_SIG,
    RELLOTGE_TICK_SIG,
    BLINK_TICK_SIG,
    MAX_SIG
};

// ── Events Control Entrades ───────────────────────────────────────────────────

struct InputChangedEvt : public QP::QEvt {
    std::unordered_map<int, bool> inputs;
    explicit InputChangedEvt() noexcept : QP::QEvt{INPUT_CHANGED_SIG} {}
};

struct ReconfigureEvt : public QP::QEvt {
    static constexpr int MAX_CONFIGS = 16;
    static constexpr int MAX_LINKED  = 8;
    struct Entry {
        int          id;
        EdgePolarity detect_edge;
        bool         detection_always;
        int          linked_outputs[MAX_LINKED];
        int          n_linked;
    };
    Entry entries[MAX_CONFIGS];
    int   n_configs;
};

struct EdgeDetectedEvt : public QP::QEvt {
    std::vector<int> edges;
    explicit EdgeDetectedEvt() noexcept : QP::QEvt{EDGE_DETECTED_SIG} {}
};

// ── Events Control Sortides ───────────────────────────────────────────────────

struct RellotgeTickEvt : public QP::QEvt {
    int hour;
    int minute;
    int wday; // 0=dilluns..6=diumenge
};

struct OutputCmdEvt : public QP::QEvt {
    int  output_id;
    bool activate;
};

struct OutputModeEvt : public QP::QEvt {
    int  output_id;
    bool remote;
};

struct OutputReturnAutoEvt : public QP::QEvt {
    int output_id; // -1 → totes les sortides
};

struct OutputDeleteEvt : public QP::QEvt {
    int output_id;
};

struct OutputStateEvt : public QP::QEvt {
    static constexpr int MAX_OUTPUTS = 32;
    struct Entry { int id; bool state; };
    Entry outputs[MAX_OUTPUTS];
    int   n_outputs = 0;
};

struct OutputResultEvt : public QP::QEvt {
    std::unordered_map<int, bool> outputs;
    explicit OutputResultEvt() noexcept : QP::QEvt{OUTPUT_RESULT_SIG} {}
};
