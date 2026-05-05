#pragma once
// IOReader per a GPIO hardware (ESP32).
// Alternativa a makeRemoteReader() quan les entrades venen de pins físics.
// Ús: substituir makeRemoteReader() per makeHWReader(configs) a main_esp32.cpp.
#include "../DigitalEdgeDetector/DigitalEdgeDetector.h"
#include "../DigitalEdgeDetector/InputConfig.h"
#include "driver/gpio.h"
#include <vector>

inline IOReader makeHWReader(const std::vector<InputConfig>& configs) {
    for (const auto& cfg : configs) {
        gpio_config_t io_conf = {};
        io_conf.pin_bit_mask = 1ULL << cfg.id;
        io_conf.mode         = GPIO_MODE_INPUT;
        io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.intr_type    = GPIO_INTR_DISABLE;
        gpio_config(&io_conf);
    }

    return [](std::unordered_map<int, bool>& inputs,
              std::unordered_map<int, bool>&) {
        for (auto& [id, val] : inputs) {
            val = gpio_get_level(static_cast<gpio_num_t>(id)) != 0;
        }
    };
}
