#pragma once
#include "qpcpp/include/qpcpp.hpp"
#include "QP/signals.h"
#include "InputConfig.h"
#include <functional>
#include <vector>
#include <unordered_map>
#include <cstdint>

using IOReader = std::function<void(
    std::unordered_map<int, bool>& inputs,
    std::unordered_map<int, bool>& outputs)>;

// ── ControlEntrades ───────────────────────────────────────────────────────
// Active Object that periodically polls IO via an IOReader callback,
// detects configured rising/falling edges, and publishes EDGE_DETECTED_SIG.
class ControlEntrades : public QP::QActive {
public:
    explicit ControlEntrades(IOReader reader, std::uint32_t poll_ticks) noexcept;

    // Must be called before starting the AO.
    void configure(const std::vector<InputConfig>& configs);

private:
    QP::QTimeEvt  m_pollTimer;
    IOReader      m_reader;
    std::uint32_t m_pollTicks;

    std::vector<InputConfig>      m_configs;
    std::unordered_map<int, bool> m_prevStates;
    std::unordered_map<int, bool> m_prevInputs;
    std::unordered_map<int, bool> m_commandedOutputs; // estat efectiu rebut de ControlRemot

    bool detection_enabled(const InputConfig& cfg,
                            const std::unordered_map<int, bool>& outputs) const;

    Q_STATE_DECL(initial);
    Q_STATE_DECL(operating);
};
