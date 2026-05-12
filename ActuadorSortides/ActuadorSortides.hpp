#pragma once
#include "../qpcpp/include/qpcpp.hpp"
#include "../signals.h"
#include <functional>
#include <unordered_map>

using OutputWriter = std::function<void(int id, bool actiu)>;

class ActuadorSortides : public QP::QActive {
public:
    explicit ActuadorSortides(OutputWriter writer) noexcept;

private:
    Q_STATE_DECL(initial);
    Q_STATE_DECL(running);

    OutputWriter                  m_writer;
    std::unordered_map<int, bool> m_prevOutputs;
};
