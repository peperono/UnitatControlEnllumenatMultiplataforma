#include "Blink.h"

Blink::Blink(int output_id, OutputWriter writer) noexcept
    : QP::QActive{Q_STATE_CAST(&Blink::initial)},
      m_outputId{output_id},
      m_state{false},
      m_writer{std::move(writer)},
      m_timer{this, BLINK_TICK_SIG}
{}

Q_STATE_DEF(Blink, initial) {
    Q_UNUSED_PAR(e);
    return tran(&Blink::running);
}

Q_STATE_DEF(Blink, running) {
    QP::QState status;
    switch (e->sig) {

        case Q_ENTRY_SIG:
            m_timer.armX(50U, 50U); // 50 ticks @ 100 Hz = 500 ms → 1 Hz
            status = Q_HANDLED();
            break;

        case BLINK_TICK_SIG:
            m_state = !m_state;
            m_writer(m_outputId, m_state);
            status = Q_HANDLED();
            break;

        default:
            status = super(&Blink::top);
            break;
    }
    return status;
}
