#pragma once
#include "../qpcpp/include/qpcpp.hpp"
#include "../signals.h"
#include "OutputConfig.h"
#include <unordered_map>
#include <vector>
#include <cstddef>

class ControlRemot : public QP::QActive {
public:
    explicit ControlRemot() noexcept;

    void configure(const std::vector<OutputConfig>& configs);

    // Cridat des del thread Mongoose. Thread-safe: Q_NEW + POST a this.
    void handleJson(const char* buf, std::size_t len);

private:
    struct OutputEntry {
        enum class Mode : uint8_t { AUTO, REMOTE } mode = Mode::AUTO;
        bool state     = false;
        bool commanded = false;
        bool result    = false;
    };

    std::unordered_map<int, OutputEntry> m_outputs;
    OutputResultEvt                      m_resultEvt; // static event (poolNum_=0)

    void publishResult();

    Q_STATE_DECL(initial);
    Q_STATE_DECL(operating);
};
