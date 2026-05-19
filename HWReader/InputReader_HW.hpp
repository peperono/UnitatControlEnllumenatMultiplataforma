#pragma once
#include "../ControlEntrades/ControlEntrades.h"
#include "driver/gpio.h"

// GPIO34 és input-only, sense pull-up intern → requereix pull-up extern (10kΩ a 3.3V)
static const struct { int id; gpio_num_t gpio; } k_hw_inputs[] = {
    {1, GPIO_NUM_34},  // E1 — pull-up extern 10kΩ a 3.3V
    {2, GPIO_NUM_35},  // E2 — pull-up extern 10kΩ a 3.3V
};
static constexpr int k_hw_n = sizeof(k_hw_inputs) / sizeof(k_hw_inputs[0]);

inline void hw_inputs_init() {
    for (int i = 0; i < k_hw_n; ++i) {
        gpio_reset_pin(k_hw_inputs[i].gpio);
        gpio_set_direction(k_hw_inputs[i].gpio, GPIO_MODE_INPUT);
        // GPIO34-39 no tenen pull-up intern — no cridar gpio_pullup_en
    }
}

inline IOReader makeHWInputReader() {
    hw_inputs_init();
    return [](std::unordered_map<int, bool>& inputs,
              std::unordered_map<int, bool>& outputs) {
        for (int i = 0; i < k_hw_n; ++i)
            inputs[k_hw_inputs[i].id] = (gpio_get_level(k_hw_inputs[i].gpio) == 0); // pull-up: LOW=tancat
        (void)outputs;
    };
}
