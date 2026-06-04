#pragma once
#include "qpcpp/include/qpcpp.hpp"
#include "Integracio/signals.h"
#include <array>
#include <vector>
#include <cstdint>
#include <cstddef>

class ControlHorari : public QP::QActive {
public:
    explicit ControlHorari() noexcept;

    void loadJson(const char* buf, std::size_t len);

private:
    struct Maniobra {
        int     id;
        bool    on;
        uint8_t hour;
        uint8_t minute;
    };

    std::array<std::vector<Maniobra>, 7> m_schedule; // 0=dilluns..6=diumenge

    Q_STATE_DECL(initial);
    Q_STATE_DECL(operating);
};
