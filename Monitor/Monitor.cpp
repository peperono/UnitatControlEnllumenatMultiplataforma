#include "Monitor.h"
#include "../LogState.h"
#include <string>

// ── Constructor ───────────────────────────────────────────────────────────────

Monitor::Monitor() noexcept
    : QP::QActive{Q_STATE_CAST(&Monitor::initial)}
{}

// ── State: initial ────────────────────────────────────────────────────────────

Q_STATE_DEF(Monitor, initial) {
    Q_UNUSED_PAR(e);
    subscribe(INPUT_CHANGED_SIG);
    subscribe(EDGE_DETECTED_SIG);
    return tran(&Monitor::running);
}

// ── State: running ────────────────────────────────────────────────────────────

Q_STATE_DEF(Monitor, running) {
    QP::QState status;

    switch (e->sig) {

        case Q_ENTRY_SIG: {
            status = Q_HANDLED();
            break;
        }

        case INPUT_CHANGED_SIG: {
            auto const* evt = Q_EVT_CAST(InputChangedEvt);
            std::string detail;
            for (auto const& [id, state] : evt->inputs) {
                auto it = m_prevInputs.find(id);
                if (it == m_prevInputs.end() || it->second != state) {
                    if (!detail.empty()) detail += ", ";
                    detail += "E" + std::to_string(id) + "=" + (state ? "ON" : "OFF");
                }
            }
            if (!detail.empty())
                log_append("Monitor", "INPUT_CHANGED_SIG", detail);
            m_prevInputs = evt->inputs;
            status = Q_HANDLED();
            break;
        }

        case EDGE_DETECTED_SIG: {
            auto const* evt = Q_EVT_CAST(EdgeDetectedEvt);
            std::string detail;
            for (int id : evt->input_ids) {
                if (!detail.empty()) detail += ", ";
                detail += "E" + std::to_string(id);
            }
            log_append("Monitor", "EDGE_DETECTED_SIG", detail);
            status = Q_HANDLED();
            break;
        }

        default: {
            status = super(&Monitor::top);
            break;
        }
    }
    return status;
}
