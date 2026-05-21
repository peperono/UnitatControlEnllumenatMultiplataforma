#pragma once
#include "../qpcpp/include/qpcpp.hpp"
#include "../signals.h"
#include <unordered_map>

class ActuadorEntrades : public QP::QActive {
public:
    explicit ActuadorEntrades() noexcept;

private:
    Q_STATE_DECL(initial);
    Q_STATE_DECL(running);

    std::unordered_map<int, bool> m_prevInputs;
    std::unordered_map<int, bool> m_allInputs;
};
