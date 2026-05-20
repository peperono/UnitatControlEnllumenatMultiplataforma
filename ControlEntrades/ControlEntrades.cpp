#include "ControlEntrades.h"
#include "ControlEntradesState.h"
#include "../LogState.h"
#include <mutex>
#include <string>

ControlEntradesState control_entrades_state;

// ── Constructor ───────────────────────────────────────────────────────────────

ControlEntrades::ControlEntrades(IOReader reader,
                                         std::uint32_t poll_ticks) noexcept
    : QP::QActive{Q_STATE_CAST(&ControlEntrades::initial)},
      m_pollTimer{this, EDGE_DETECTOR_POLL_SIG},
      m_reader{std::move(reader)},
      m_pollTicks{poll_ticks},
      m_ioEvt{},
      m_edgeEvt{}
{}

// ── Public API ────────────────────────────────────────────────────────────────

void ControlEntrades::configure(const std::vector<InputConfig>& configs) {
    m_configs = configs;
    m_prevStates.clear();
}

// ── State: initial (pseudo-state) ─────────────────────────────────────────────

Q_STATE_DEF(ControlEntrades, initial) {
    Q_UNUSED_PAR(e);
    subscribe(OUTPUT_RESULT_SIG);
    m_pollTimer.armX(m_pollTicks, m_pollTicks);
    return tran(&ControlEntrades::operating);
}

// ── State: operating ──────────────────────────────────────────────────────────

Q_STATE_DEF(ControlEntrades, operating) {
    QP::QState status;

    switch (e->sig) {

        case Q_ENTRY_SIG: {
            status = Q_HANDLED();
            break;
        }

        case EDGE_DETECTOR_POLL_SIG: {
            std::unordered_map<int, bool> inputs;
            std::unordered_map<int, bool> outputs;
            m_reader(inputs, outputs);

            if (inputs != m_prevInputs) {
                m_prevInputs = inputs;

                m_ioEvt.inputs = inputs;
                {
                    std::string detail;
                    for (auto const& [id, state] : inputs) {
                        if (!detail.empty()) detail += ", ";
                        detail += "E" + std::to_string(id) + "=" + (state ? "ON" : "OFF");
                    }
                    log_append("ControlEntrades", ">> INPUT_CHANGED_SIG", detail);
                }
                PUBLISH(&m_ioEvt, this);

                m_edgeEvt.input_ids.clear();

                for (const auto& cfg : m_configs) {
                    auto it = inputs.find(cfg.id);
                    if (it == inputs.end()) continue;

                    bool current = it->second;
                    bool prev    = m_prevStates.count(cfg.id)
                                       ? m_prevStates.at(cfg.id)
                                       : current; // first scan: no edge

                    bool rising_edge   = !prev && current;
                    bool falling_edge  =  prev && !current;
                    // falling = tancament (false→true); rising = obertura (true→false)
                    bool edge_detected = (cfg.detect_edge == EdgePolarity::falling) ? rising_edge : falling_edge;

                    if (edge_detected && detection_enabled(cfg, outputs)) {
                        m_edgeEvt.input_ids.push_back(cfg.id);
                    }

                    m_prevStates[cfg.id] = current;
                }

                if (!m_edgeEvt.input_ids.empty()) {
                    std::string detail;
                    for (int id : m_edgeEvt.input_ids) {
                        if (!detail.empty()) detail += ", ";
                        detail += "E" + std::to_string(id);
                    }
                    log_append("ControlEntrades", ">> EDGE_DETECTED_SIG", detail);
                    PUBLISH(&m_edgeEvt, this);
                }

                {
                    std::lock_guard<std::mutex> lk(control_entrades_state.mtx);
                    control_entrades_state.inputs  = inputs;
                    control_entrades_state.outputs = outputs;
                    if (!m_edgeEvt.input_ids.empty()) {
                        control_entrades_state.last_edges = m_edgeEvt.input_ids;
                        for (int id : m_edgeEvt.input_ids)
                            ++control_entrades_state.edge_counts[id];
                    } else {
                        control_entrades_state.last_edges.clear();
                    }
                }
                control_entrades_state.push_pending.store(true);
            }

            status = Q_HANDLED();
            break;
        }

        case OUTPUT_RESULT_SIG: {
            auto const* ev = Q_EVT_CAST(OutputResultEvt);
            m_commandedOutputs = ev->outputs;
            {
                std::string detail;
                for (auto const& [id, state] : ev->outputs) {
                    if (!detail.empty()) detail += ", ";
                    detail += "S" + std::to_string(id) + "=" + (state ? "ON" : "OFF");
                }
                log_append("ControlEntrades", "<< OUTPUT_RESULT_SIG", detail);
            }
            status = Q_HANDLED();
            break;
        }

        case RECONFIGURE_SIG: {
            auto const* evt = Q_EVT_CAST(ReconfigureEvt);
            log_append("ControlEntrades", "<< RECONFIGURE_SIG",
                       std::to_string(evt->n_configs) + " entrades");
            std::vector<InputConfig> newConfigs;
            for (int i = 0; i < evt->n_configs; ++i) {
                auto const& e = evt->entries[i];
                InputConfig cfg;
                cfg.id               = e.id;
                cfg.detect_edge      = e.detect_edge;
                cfg.detection_always = e.detection_always;
                for (int j = 0; j < e.n_linked; ++j)
                    cfg.linked_outputs.push_back(e.linked_outputs[j]);
                newConfigs.push_back(std::move(cfg));
            }
            configure(newConfigs);
            m_prevInputs.clear();
            {
                std::lock_guard<std::mutex> lk(control_entrades_state.mtx);
                control_entrades_state.config_inputs = m_configs;
                control_entrades_state.inputs.clear();
                control_entrades_state.outputs.clear();
                control_entrades_state.edge_counts.clear();
                control_entrades_state.last_edges.clear();
                for (const auto& cfg : m_configs) {
                    control_entrades_state.inputs[cfg.id] = false;
                    for (int out_id : cfg.linked_outputs)
                        control_entrades_state.outputs[out_id] = false;
                }
            }
            control_entrades_state.push_pending.store(true);
            status = Q_HANDLED();
            break;
        }

        default: {
            status = super(&ControlEntrades::top);
            break;
        }
    }
    return status;
}

// ── Private helpers ───────────────────────────────────────────────────────────

bool ControlEntrades::detection_enabled(
    const InputConfig& cfg,
    const std::unordered_map<int, bool>& outputs) const
{
    if (cfg.detection_always) return true;

    for (int out_id : cfg.linked_outputs) {
        // Estat comandat per ControlRemot té prioritat sobre l'IOReader
        auto it = m_commandedOutputs.find(out_id);
        if (it != m_commandedOutputs.end()) {
            if (it->second) return true;
            continue;
        }
        auto it2 = outputs.find(out_id);
        if (it2 != outputs.end() && it2->second) return true;
    }
    return false;
}
