#include "ActuadorSortides.hpp"
#include <cstdio>

#ifdef ESP_PLATFORM
#include "driver/gpio.h"

// Mapeig: id de sortida → GPIO físic de l'ESP-WROVER-KIT V4.1
// Correspondència GPIO → pin físic del mòdul ESP32:
//   GPIO_4 = pin 26 | GPIO_0 = pin 25 | GPIO_2 = pin 24
// NOTA: GPIO_16/17 reservats per PSRAM (mòdul WROVER) — no usar
static const struct { int id; gpio_num_t gpio; } k_mapa[] = {
    {1, GPIO_NUM_4 },  // S1 — pin 26 — LED blau
    {2, GPIO_NUM_0 },  // S2 — pin 25 — LED vermell (compartit amb BOOT; no baixar durant el reset)
    {3, GPIO_NUM_2 },  // S3 — pin 24 — LED verd
};
static constexpr int k_n = sizeof(k_mapa) / sizeof(k_mapa[0]);

static gpio_num_t gpio_per_id(int id) {
    for (int i = 0; i < k_n; ++i)
        if (k_mapa[i].id == id) return k_mapa[i].gpio;
    return GPIO_NUM_NC;
}

static void init_gpios() {
    for (int i = 0; i < k_n; ++i) {
        gpio_reset_pin(k_mapa[i].gpio);
        gpio_set_direction(k_mapa[i].gpio, GPIO_MODE_OUTPUT);
        gpio_set_level(k_mapa[i].gpio, 0);
        std::printf("[ActuadorSortides] GPIO %d configurat com a sortida\n", (int)k_mapa[i].gpio);
    }

    // Prova d'arrencada: encén cada sortida 200 ms per verificar el GPIO
    for (int i = 0; i < k_n; ++i) {
        std::printf("[ActuadorSortides] test GPIO %d ON\n", (int)k_mapa[i].gpio);
        gpio_set_level(k_mapa[i].gpio, 1);
        vTaskDelay(pdMS_TO_TICKS(200));
        gpio_set_level(k_mapa[i].gpio, 0);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
#endif

// ── Constructor ───────────────────────────────────────────────────────────────

ActuadorSortides::ActuadorSortides() noexcept
    : QP::QActive{Q_STATE_CAST(&ActuadorSortides::initial)}
{}

// ── State: initial ────────────────────────────────────────────────────────────

Q_STATE_DEF(ActuadorSortides, initial) {
    Q_UNUSED_PAR(e);
#ifdef ESP_PLATFORM
    init_gpios();
#endif
    subscribe(OUTPUT_RESULT_SIG);
    return tran(&ActuadorSortides::running);
}

// ── State: running ────────────────────────────────────────────────────────────

Q_STATE_DEF(ActuadorSortides, running) {
    QP::QState status;

    switch (e->sig) {

        case Q_ENTRY_SIG: {
            status = Q_HANDLED();
            break;
        }

        case OUTPUT_RESULT_SIG: {
            auto const* evt = Q_EVT_CAST(OutputResultEvt);
            for (auto const& [id, actiu] : evt->outputs) {
                std::printf("[ActuadorSortides] id=%d %s\n", id, actiu ? "ON" : "OFF");
                auto it = m_prevOutputs.find(id);
                if (it != m_prevOutputs.end() && it->second == actiu)
                    continue;
#ifdef ESP_PLATFORM
                gpio_num_t pin = gpio_per_id(id);
                if (pin != GPIO_NUM_NC) {
                    std::printf("[ActuadorSortides] gpio_set_level(%d, %d)\n", (int)pin, actiu ? 1 : 0);
                    gpio_set_level(pin, actiu ? 1 : 0);
                } else {
                    std::printf("[ActuadorSortides] id=%d sense GPIO assignat\n", id);
                }
#else
                std::printf("[ActuadorSortides] S%d: %s\n", id, actiu ? "ON" : "OFF");
#endif
            }
            m_prevOutputs = evt->outputs;
            status = Q_HANDLED();
            break;
        }

        default: {
            status = super(&ActuadorSortides::top);
            break;
        }
    }
    return status;
}
