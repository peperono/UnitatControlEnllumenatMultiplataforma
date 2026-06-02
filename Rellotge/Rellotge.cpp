#include "Rellotge.h"
#include "RellotgeState.h"
#include <ctime>
#include <mutex>

RellotgeState rellotge_state;

static void localtime_safe(std::time_t t, std::tm& lt) {
#ifdef _WIN32
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &lt);
#endif
}

Rellotge::Rellotge() noexcept
    : QP::QActive{Q_STATE_CAST(&Rellotge::initial)},
      m_tick{this, RELLOTGE_TICK_INTERNAL_SIG, 0U}
{}

Q_STATE_DEF(Rellotge, initial) {
    Q_UNUSED_PAR(e);

    std::time_t now = std::time(nullptr);
    std::tm lt;
    localtime_safe(now, lt);
    m_wday   = (lt.tm_wday + 6) % 7;
    m_hour   = lt.tm_hour;
    m_minute = lt.tm_min;

    {
        std::lock_guard<std::mutex> lk(rellotge_state.mtx);
        rellotge_state.hour   = m_hour;
        rellotge_state.minute = m_minute;
        rellotge_state.second = 0;
        rellotge_state.wday   = m_wday;
    }

    // 100 ticks/s, 5 ticks = 50ms → 1 minut simulat cada 50ms
    m_tick.armX(5U, 5U);
    return tran(&Rellotge::operating);
}

Q_STATE_DEF(Rellotge, operating) {
    QP::QState status;
    switch (e->sig) {

        case Q_ENTRY_SIG:
            status = Q_HANDLED();
            break;

        case RELLOTGE_TICK_INTERNAL_SIG: {
            if (++m_minute >= 60) {
                m_minute = 0;
                if (++m_hour >= 24) {
                    m_hour = 0;
                    m_wday = (m_wday + 1) % 7;
                }
            }
            {
                std::lock_guard<std::mutex> lk(rellotge_state.mtx);
                rellotge_state.hour   = m_hour;
                rellotge_state.minute = m_minute;
                rellotge_state.second = 0;
                rellotge_state.wday   = m_wday;
            }
            rellotge_state.push_pending.store(true);

            auto* ev   = Q_NEW(RellotgeTickEvt, RELLOTGE_TICK_SIG);
            ev->hour   = m_hour;
            ev->minute = m_minute;
            ev->wday   = m_wday;
            PUBLISH(ev, this);

            status = Q_HANDLED();
            break;
        }

        default:
            status = super(&Rellotge::top);
            break;
    }
    return status;
}
