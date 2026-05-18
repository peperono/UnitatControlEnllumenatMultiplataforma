#include "DigitalEdgeDetector.h"
#include "DigitalEdgeDetectorState.h"
#include <mutex>

DigitalEdgeDetectorState edge_detector_state;

// ── Constructor ───────────────────────────────────────────────────────────────

DigitalEdgeDetector::DigitalEdgeDetector(IOReader reader,
                                         std::uint32_t poll_ticks) noexcept
    : QP::QActive{Q_STATE_CAST(&DigitalEdgeDetector::initial)},
      m_pollTimer{this, EDGE_DETECTOR_POLL_SIG},
      m_reader{std::move(reader)},
      m_pollTicks{poll_ticks},
      m_ioEvt{},
      m_edgeEvt{}
{}

// ── Public API ────────────────────────────────────────────────────────────────

void DigitalEdgeDetector::configure(const std::vector<InputConfig>& configs) {
    m_configs = configs;
    m_prevStates.clear();
}

// ── State: initial (pseudo-state) ─────────────────────────────────────────────

Q_STATE_DEF(DigitalEdgeDetector, initial) {
    Q_UNUSED_PAR(e);
    subscribe(OUTPUT_RESULT_SIG);
    m_pollTimer.armX(m_pollTicks, m_pollTicks);
    return tran(&DigitalEdgeDetector::operating);
}

// ── State: operating ──────────────────────────────────────────────────────────

Q_STATE_DEF(DigitalEdgeDetector, operating) {
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

            if ((inputs != m_prevInputs) || (outputs != m_prevOutputs)) {
                m_prevInputs  = inputs;
                m_prevOutputs = outputs;

                m_ioEvt.inputs  = inputs;
                m_ioEvt.outputs = outputs;
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
                    PUBLISH(&m_edgeEvt, this);
                }

                {
                    std::lock_guard<std::mutex> lk(edge_detector_state.mtx);
                    edge_detector_state.inputs  = inputs;
                    edge_detector_state.outputs = outputs;
                    if (!m_edgeEvt.input_ids.empty()) {
                        edge_detector_state.last_edges = m_edgeEvt.input_ids;
                        for (int id : m_edgeEvt.input_ids)
                            ++edge_detector_state.edge_counts[id];
                    } else {
                        edge_detector_state.last_edges.clear();
                    }
                }
                edge_detector_state.push_pending.store(true);
            }

            status = Q_HANDLED();
            break;
        }

        case OUTPUT_RESULT_SIG: {
            auto const* ev = Q_EVT_CAST(OutputResultEvt);
            m_commandedOutputs = ev->outputs;
            status = Q_HANDLED();
            break;
        }

        case RECONFIGURE_SIG: {
            auto const* evt = Q_EVT_CAST(ReconfigureEvt);
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
            m_prevOutputs.clear();
            {
                std::lock_guard<std::mutex> lk(edge_detector_state.mtx);
                edge_detector_state.configs = m_configs;
                edge_detector_state.inputs.clear();
                edge_detector_state.outputs.clear();
                edge_detector_state.edge_counts.clear();
                edge_detector_state.last_edges.clear();
                for (const auto& cfg : m_configs) {
                    edge_detector_state.inputs[cfg.id] = false;
                    for (int out_id : cfg.linked_outputs)
                        edge_detector_state.outputs[out_id] = false;
                }
            }
            edge_detector_state.push_pending.store(true);
            status = Q_HANDLED();
            break;
        }

        default: {
            status = super(&DigitalEdgeDetector::top);
            break;
        }
    }
    return status;
}

// ── Private helpers ───────────────────────────────────────────────────────────

bool DigitalEdgeDetector::detection_enabled(
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
