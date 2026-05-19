#pragma once
#include "ActuadorSortides.hpp"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstdio>

static const struct { int id; gpio_num_t gpio; } k_mapa[] = {
    {1, GPIO_NUM_4 },  // S1 — pin 26 — LED blau
    {2, GPIO_NUM_0 },  // S2 — pin 25 — LED vermell
    {3, GPIO_NUM_2 },  // S3 — pin 24 — LED verd
};
static constexpr int k_n = sizeof(k_mapa) / sizeof(k_mapa[0]);

static gpio_num_t gpio_per_id(int id) {
    for (int i = 0; i < k_n; ++i)
        if (k_mapa[i].id == id) return k_mapa[i].gpio;
    return GPIO_NUM_NC;
}

inline OutputWriter makeGPIOWriter() {
    for (int i = 0; i < k_n; ++i) {
        gpio_reset_pin(k_mapa[i].gpio);
        gpio_set_direction(k_mapa[i].gpio, GPIO_MODE_OUTPUT);
        gpio_set_level(k_mapa[i].gpio, 0);
        std::printf("[ActuadorSortides] GPIO %d configurat com a sortida\n", (int)k_mapa[i].gpio);
    }
    for (int i = 0; i < k_n; ++i) {
        std::printf("[ActuadorSortides] test GPIO %d ON\n", (int)k_mapa[i].gpio);
        gpio_set_level(k_mapa[i].gpio, 1);
        vTaskDelay(pdMS_TO_TICKS(200));
        gpio_set_level(k_mapa[i].gpio, 0);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    return [](int id, bool actiu) {
        gpio_num_t pin = gpio_per_id(id);
        if (pin != GPIO_NUM_NC)
            gpio_set_level(pin, actiu ? 1 : 0);
    };
}
