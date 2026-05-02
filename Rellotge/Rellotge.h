#pragma once
#include "../qpcpp/include/qpcpp.hpp"
#include "../signals.h"

class Rellotge : public QP::QActive {
public:
    explicit Rellotge() noexcept;

private:
    QP::QTimeEvt m_tick;
    int          m_wday   = 0;
    int          m_hour   = 0;
    int          m_minute = 0;

    Q_STATE_DECL(initial);
    Q_STATE_DECL(operating);
};
