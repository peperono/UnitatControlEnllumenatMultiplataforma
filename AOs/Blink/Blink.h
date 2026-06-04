#pragma once
#include "qpcpp/include/qpcpp.hpp"
#include "Integracio/signals.h"
#include <functional>

class Blink : public QP::QActive {
public:
    using OutputWriter = std::function<void(int id, bool actiu)>;

    explicit Blink(int output_id, OutputWriter writer) noexcept;

private:
    int           m_outputId;
    bool          m_state = false;
    OutputWriter  m_writer;
    QP::QTimeEvt  m_timer;

    Q_STATE_DECL(initial);
    Q_STATE_DECL(running);
};
