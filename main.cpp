#include "qp_config.hpp"
#include "qpcpp/include/qpcpp.hpp"
#include "signals.h"
#include "DigitalEdgeDetector/DigitalEdgeDetector.h"
#include "Monitor/Monitor.h"
#include "HttpServer/HttpServer.h"
#include "DigitalEdgeDetector/SharedState.h"
#include "RemoteIO/RemoteIOState.h"
#include "Test/TestController.hpp"
#include "RemoteIO/IOReader_Remot.hpp"
#include "ControlRemot/ControlRemot.h"
#include "ControlHorari/ControlHorari.h"
#include "ControlHorari/ControlHorariState.h"
#include "ControlHorari/json_horari.h"
#include "Rellotge/Rellotge.h"
#include "ActuadorSortides/OutputWriter_Console.hpp"
#include "mongoose/mongoose.h"
#include <cstdio>
#include <cstdlib>

SharedState   se;
RemoteIOState remoteIO;

extern "C" Q_NORETURN Q_onError(char const * const module, int_t const id) {
    std::fprintf(stderr, "Q_onError: %s:%d\n", module, id);
    std::exit(1);
}

static QP::QSubscrList subscrSto[MAX_SIG];

// ── Cues d'events ─────────────────────────────────────────────────────────────
static QP::QEvtPtr edgeDetectorQSto[10];
static QP::QEvtPtr monitorQSto[10];
static QP::QEvtPtr testObserverQSto[10];
static QP::QEvtPtr controlRemotQSto[64];
static QP::QEvtPtr controlHorariQSto[32];
static QP::QEvtPtr rellotgeQSto[16];
static QP::QEvtPtr actuadorSortidesQSto[8];

// ── Callbacks del port win32-qv ───────────────────────────────────────────────
namespace QP {
namespace QF {

void onStartup() {
    setTickRate(100U, 50); // 100 ticks/s per al Rellotge simulat
}

void onCleanup() {}

void onClockTick() {
    QP::QTimeEvt::TICK_X(0U, nullptr);
}

} // namespace QF
} // namespace QP

// ── main ──────────────────────────────────────────────────────────────────────
int main() {
    std::printf("=== UnitatControlEnllumenat ===\n");
    std::printf("Selecciona reader (Control Entrades):\n");
    std::printf("  1) Test (sequencia automatica)\n");
    std::printf("  2) Control remot (navegador web)\n");
    std::printf("> ");
    int choice = 1;
    std::scanf("%d", &choice);

    std::printf("Mostrar missatges de depuracio de Mongoose? (s/n): ");
    char dbg = 'n';
    std::scanf(" %c", &dbg);
    mg_log_set(dbg == 's' ? MG_LL_DEBUG : MG_LL_NONE);

    const std::vector<InputConfig> configs = {
        InputConfig{1, /*logic_positive=*/true,  /*always=*/true,  {}   },
        InputConfig{2, /*logic_positive=*/true,  /*always=*/false, {10} },
        InputConfig{4, /*logic_positive=*/false, /*always=*/true,  {}   }
    };

    if (choice == 2) {
        std::lock_guard<std::mutex> lk(remoteIO.mtx);
        for (const auto& cfg : configs) {
            remoteIO.inputs[cfg.id] = false;
            for (int out_id : cfg.linked_outputs)
                remoteIO.outputs[out_id] = false;
        }
    }

    IOReader reader = (choice == 2) ? makeRemoteReader() : makeTestReader();

    // ── Active Object instances ───────────────────────────────────────────────
    static DigitalEdgeDetector edgeDetector{ std::move(reader), 1U };
    static Monitor             monitor;
    static TestObserver        testObserver;
    static ControlRemot        controlRemot;
    static ControlHorari       controlHorari;
    static Rellotge            rellotge;
    static ActuadorSortides    actuadorSortides{makeConsoleWriter()};

    edgeDetector.configure(configs);

    {
        std::lock_guard<std::mutex> lk(se.mtx);
        se.configs = configs;
        for (const auto& cfg : configs) {
            se.inputs[cfg.id] = false;
            for (int out_id : cfg.linked_outputs)
                se.outputs[out_id] = false;
        }
    }

    QP::QF::init();
    QP::QActive::psInit(subscrSto, Q_DIM(subscrSto));

    // Pool 1 (petit): events de comanda (OutputCmdEvt, OutputModeEvt, etc.)
    static QF_MPOOL_EL(OutputCmdEvt) smallPool[16];
    QP::QF::poolInit(smallPool, sizeof(smallPool), sizeof(smallPool[0]));

    // Pool 2 (gran): ReconfigureEvt i OutputStateEvt
    static QF_MPOOL_EL(ReconfigureEvt) largePool[8];
    QP::QF::poolInit(largePool, sizeof(largePool), sizeof(largePool[0]));

    // Prioritats (alt→baix): Rellotge > EdgeDetector > ControlRemot > ControlHorari > Monitor > TestObserver
    rellotge.start(    6U, rellotgeQSto,      Q_DIM(rellotgeQSto),      nullptr, 0U);
    edgeDetector.start(5U, edgeDetectorQSto,  Q_DIM(edgeDetectorQSto),  nullptr, 0U);
    controlRemot.start(4U, controlRemotQSto,  Q_DIM(controlRemotQSto),  nullptr, 0U);
    controlHorari.start(3U,controlHorariQSto, Q_DIM(controlHorariQSto), nullptr, 0U);
    monitor.start(     2U, monitorQSto,       Q_DIM(monitorQSto),       nullptr, 0U);
    if (choice != 2) {
        testObserver.start(   1U, testObserverQSto,      Q_DIM(testObserverQSto),      nullptr, 0U);
    } else {
        actuadorSortides.start(1U, actuadorSortidesQSto, Q_DIM(actuadorSortidesQSto), nullptr, 0U);
    }

    // Carrega l'horari per defecte
    {
        std::lock_guard<std::mutex> lk(ch_state.mtx);
        ch_state.programacioHoraria.assign(JSON_HORARI, sizeof(JSON_HORARI) - 1);
    }
    controlHorari.loadJson(JSON_HORARI, sizeof(JSON_HORARI) - 1);

    HttpServer::start(8080, &edgeDetector, &controlRemot, "Windows");

    int ret = QP::QF::run();

    HttpServer::stop();
    return ret;
}
