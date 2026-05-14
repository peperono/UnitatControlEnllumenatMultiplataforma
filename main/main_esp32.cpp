#include "qp_config.hpp"
#include "qpcpp/include/qpcpp.hpp"
#include "signals.h"
#include "DigitalEdgeDetector/DigitalEdgeDetector.h"
#include "Monitor/Monitor.h"
#include "HttpServer/HttpServer.h"
#include "DigitalEdgeDetector/DigitalEdgeDetectorState.h"
#include "RemoteIO/RemoteIOState.h"
#include "RemoteIO/IOReader_Remot.hpp"
#include "HWReader/IOReader_HW.hpp"
#include "ControlRemot/ControlRemot.h"
#include "ControlHorari/ControlHorari.h"
#include "ControlHorari/ControlHorariState.h"
#include "ControlHorari/json_horari.h"
#include "Rellotge/Rellotge.h"
#include "ActuadorSortides/OutputWriter_HW.hpp"
#include "mongoose/mongoose.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include <cstdio>
#include <cstring>
#include <mutex>

// ── Globals compartits entre threads ─────────────────────────────────────────
portMUX_TYPE  g_qp_mux = portMUX_INITIALIZER_UNLOCKED; // secció crítica QP
DigitalEdgeDetectorState   edge_detector_state;
RemoteIOState remote_io_state;

extern "C" Q_NORETURN Q_onError(char const * const module, int_t const id) {
    std::fprintf(stderr, "Q_onError: %s:%d\n", module, id);
    esp_restart();
    for (;;) {}
}

// ── Cues d'events QP ─────────────────────────────────────────────────────────
static QP::QSubscrList subscrSto[MAX_SIG];

static QP::QEvtPtr rellotgeQSto      [16];
static QP::QEvtPtr edgeDetectorQSto  [10];
static QP::QEvtPtr controlRemotQSto  [64];
static QP::QEvtPtr controlHorariQSto [32];
static QP::QEvtPtr monitorQSto          [10];
static QP::QEvtPtr actuadorSortidesQSto [ 8];

// ── Stacks estàtics per a cada tasca FreeRTOS ────────────────────────────────
// El port FreeRTOS de QP/C++ requereix configSUPPORT_STATIC_ALLOCATION=1
// NOTA: StackType_t = uint8_t en Xtensa → mides en bytes, no en paraules de 4B
static StackType_t rellotgeStk      [ 4 * 1024];  //  4 KB
static StackType_t edgeDetectorStk  [ 8 * 1024];  //  8 KB
static StackType_t controlRemotStk  [ 8 * 1024];  //  8 KB
static StackType_t controlHorariStk [16 * 1024];  // 16 KB (JSON parsing)
static StackType_t monitorStk            [ 4 * 1024];  //  4 KB
static StackType_t actuadorSortidesStk   [ 4 * 1024];  //  4 KB

// ── Tick de QP via esp_timer (10 ms = 100 Hz, igual que Win32) ───────────────
static void qp_tick_cb(void*) {
    QP::QTimeEvt::TICK_X(0U, nullptr);
}

namespace QP {
namespace QF {

void onStartup() {
    esp_timer_create_args_t args = {};
    args.callback        = qp_tick_cb;
    args.dispatch_method = ESP_TIMER_TASK;
    args.name            = "qp_tick";
    esp_timer_handle_t timer;
    esp_timer_create(&args, &timer);
    esp_timer_start_periodic(timer, 10000U); // 10 ms → 100 Hz
}

void onCleanup() {}

} // namespace QF
} // namespace QP

// ── WiFi (mode Station) ───────────────────────────────────────────────────────
static EventGroupHandle_t s_wifi_eg;
static constexpr int WIFI_CONNECTED_BIT = BIT0;

static void wifi_event_handler(void*, esp_event_base_t base, int32_t id, void* data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        auto* event = static_cast<ip_event_got_ip_t*>(data);
        std::printf("[WiFi] IP: " IPSTR "\n", IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_eg, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta() {
    s_wifi_eg = xEventGroupCreate();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,    wifi_event_handler, nullptr);
    esp_event_handler_register(IP_EVENT,   IP_EVENT_STA_GOT_IP, wifi_event_handler, nullptr);

    wifi_config_t wcfg = {};
    std::strncpy(reinterpret_cast<char*>(wcfg.sta.ssid),
                 CONFIG_WIFI_SSID, sizeof(wcfg.sta.ssid) - 1);
    std::strncpy(reinterpret_cast<char*>(wcfg.sta.password),
                 CONFIG_WIFI_PASSWORD, sizeof(wcfg.sta.password) - 1);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wcfg);
    std::printf("[WiFi] SSID: %s | PASSWORD: %s\n", CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD);
    esp_wifi_start();
    esp_wifi_connect();

    xEventGroupWaitBits(s_wifi_eg, WIFI_CONNECTED_BIT,
                        pdFALSE, pdTRUE, portMAX_DELAY);
    std::printf("[WiFi] connectat a %s\n", CONFIG_WIFI_SSID);
    std::printf("[WiFi] Obre el navegador a http://<IP>:8080\n");
}

// ── app_main ──────────────────────────────────────────────────────────────────
extern "C" void app_main() {
    std::printf("=== UnitatControlEnllumenat (ESP32) ===\n");
    mg_log_set(MG_LL_NONE);

    // NVS: requerit per WiFi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_netif_init();
    esp_event_loop_create_default();
    wifi_init_sta();

    // Configuració d'entrades per defecte (editable via PUT /config_inputs)
    const std::vector<InputConfig> configs = {
        InputConfig{1, /*detect_edge=*/EdgePolarity::falling, /*always=*/true,  {}   },
        InputConfig{2, /*detect_edge=*/EdgePolarity::rising,  /*always=*/false, {1} },
    };

    {
        std::lock_guard<std::mutex> lk(remote_io_state.mtx);
        for (const auto& cfg : configs) {
            remote_io_state.inputs[cfg.id] = false;
            for (int out_id : cfg.linked_outputs)
                remote_io_state.outputs[out_id] = false;
        }
    }

    {
        std::lock_guard<std::mutex> lk(edge_detector_state.mtx);
        edge_detector_state.configs = configs;
        for (const auto& cfg : configs) {
            edge_detector_state.inputs[cfg.id] = false;
            for (int out_id : cfg.linked_outputs)
                edge_detector_state.outputs[out_id] = false;
        }
    }

    // ── Active Objects ────────────────────────────────────────────────────────
    // NOTA: IOStateEvt/EdgeDetectedEvt usen semàntiques zero-pool (sense còpia).
    // Sota QV (cooperatiu, Windows) és segur. Sota FreeRTOS (preemptiu), si
    // DigitalEdgeDetector publica un nou event abans que tots els subscriptors
    // hagin processat l'anterior, hi ha una condició de cursa. A 100 Hz de
    // polling i subscriptors lleugers, el risc pràctic és baix, però s'ha de
    // refactoritzar si es requereix garantia estricta.
    static DigitalEdgeDetector edgeDetector{makeHWReader(), 1U};
    static Monitor             monitor;
    static ControlRemot        controlRemot;
    static ControlHorari       controlHorari;
    static Rellotge            rellotge;
    static ActuadorSortides    actuadorSortides{makeGPIOWriter()};

    edgeDetector.configure(configs);

    QP::QF::init();
    QP::QActive::psInit(subscrSto, Q_DIM(subscrSto));

    static QF_MPOOL_EL(OutputCmdEvt)   smallPool[16];
    QP::QF::poolInit(smallPool, sizeof(smallPool), sizeof(smallPool[0]));

    static QF_MPOOL_EL(ReconfigureEvt) largePool[8];
    QP::QF::poolInit(largePool, sizeof(largePool), sizeof(largePool[0]));

    // Prioritats i stacks (FreeRTOS requereix stack explícit per static allocation)
    rellotge.start    (6U, rellotgeQSto,      Q_DIM(rellotgeQSto),
                       rellotgeStk,      sizeof(rellotgeStk));
    edgeDetector.start(5U, edgeDetectorQSto,  Q_DIM(edgeDetectorQSto),
                       edgeDetectorStk,  sizeof(edgeDetectorStk));
    controlRemot.start(4U, controlRemotQSto,  Q_DIM(controlRemotQSto),
                       controlRemotStk,  sizeof(controlRemotStk));
    controlHorari.start(3U,controlHorariQSto, Q_DIM(controlHorariQSto),
                       controlHorariStk, sizeof(controlHorariStk));
    monitor.start     (2U, monitorQSto,          Q_DIM(monitorQSto),
                       monitorStk,          sizeof(monitorStk));
    actuadorSortides.start(1U, actuadorSortidesQSto, Q_DIM(actuadorSortidesQSto),
                       actuadorSortidesStk, sizeof(actuadorSortidesStk));

    {
        std::lock_guard<std::mutex> lk(control_horari_state.mtx);
        control_horari_state.programacioHoraria.assign(JSON_HORARI, sizeof(JSON_HORARI) - 1);
    }
    controlHorari.loadJson(JSON_HORARI, sizeof(JSON_HORARI) - 1);

    HttpServer::start(8080, &edgeDetector, &controlRemot, "ESP32");

    QP::QF::run(); // inicia el scheduler FreeRTOS — no retorna
}
