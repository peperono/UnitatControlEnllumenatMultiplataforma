#include "ActuadorSortides.hpp"
#include <cstdio>

ActuadorSortides::ActuadorSortides(OutputWriter writer) noexcept
    : QP::QActive{Q_STATE_CAST(&ActuadorSortides::initial)},
      m_writer{std::move(writer)}
{}

Q_STATE_DEF(ActuadorSortides, initial) {
    Q_UNUSED_PAR(e);
    subscribe(OUTPUT_RESULT_SIG);
    return tran(&ActuadorSortides::running);
}

Q_STATE_DEF(ActuadorSortides, running) {
    QP::QState status;

    switch (e->sig) {

        case Q_ENTRY_SIG: {
            status = Q_HANDLED();
            break;
        }

        case OUTPUT_RESULT_SIG: {
            auto const* evt = Q_EVT_CAST(OutputResultEvt);
            for (auto const& [id, actiu] : evt->outputs) {
                std::printf("[ActuadorSortides] id=%d %s\n", id, actiu ? "ON" : "OFF");
                auto it = m_prevOutputs.find(id);
                if (it != m_prevOutputs.end() && it->second == actiu)
                    continue;
                m_writer(id, actiu);
            }
            m_prevOutputs = evt->outputs;
            status = Q_HANDLED();
            break;
        }

        default: {
            status = super(&ActuadorSortides::top);
            break;
        }
    }
    return status;
}
