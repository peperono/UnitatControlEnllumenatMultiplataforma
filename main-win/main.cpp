#include "QP/qp_config.hpp"
#include "qpcpp/include/qpcpp.hpp"
#include "QP/signals.h"
#include "ControlEntrades/ControlEntrades.h"
#include "HttpServer/HttpServer.h"
#include "ControlEntrades/ControlEntradesState.h"
#include "RemoteIO/RemoteIOState.h"
#include "Test/TestUnitariControlEntrades.hpp"
#include "RemoteIO/InputReader_WS.hpp"
#include "ControlRemot/ControlRemot.h"
#include "ControlRemot/OutputConfig.h"
#include "ControlHorari/ControlHorari.h"
#include "ControlHorari/ControlHorariState.h"
#include "ControlHorari/json_horari.h"
#include "Rellotge/Rellotge.h"
#include "ActuadorSortides/OutputWriter_Console.hpp"
#include "mongoose/mongoose.h"
#include <cstdio>
#include <cstdlib>

RemoteIOState remote_io_state;

extern "C" Q_NORETURN Q_onError(char const * const module, int_t const id) {
    std::fprintf(stderr, "Q_onError: %s:%d\n", module, id);
    std::exit(1);
}

static QP::QSubscrList subscrSto[MAX_SIG];

// ── Cues d'events ─────────────────────────────────────────────────────────────
static QP::QEvtPtr controlEntradesQSto[32];
static QP::QEvtPtr testObserverQSto[10];
static QP::QEvtPtr controlRemotQSto[64];
static QP::QEvtPtr controlHorariQSto[64];
static QP::QEvtPtr rellotgeQSto[32];
static QP::QEvtPtr actuadorSortidesQSto[32];

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
int main(int argc, char* argv[]) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("=== UnitatControlEnllumenat ===\n");
    if (argc < 2) {
        std::fprintf(stderr, "Ús: app.exe TEST_UNITARI|TEST_INTEGRACIO\n");
        return 1;
    }
    std::string mode = argv[1];
    int choice;
    if (mode == "TEST_UNITARI") {
        choice = 1;
    } else if (mode == "TEST_INTEGRACIO") {
        choice = 2;
    } else {
        std::fprintf(stderr, "Mode desconegut: %s (valors vàlids: TEST_UNITARI, TEST_INTEGRACIO)\n", argv[1]);
        return 1;
    }

    mg_log_set(MG_LL_NONE);

    const std::vector<InputConfig> configs = {
        InputConfig{1, /*detect_edge=*/EdgePolarity::falling, /*always=*/true,  {}   },
        InputConfig{2, /*detect_edge=*/EdgePolarity::falling, /*always=*/false, {2} },
        InputConfig{3, /*detect_edge=*/EdgePolarity::rising,  /*always=*/true,  {}   }
    };

    const std::vector<OutputConfig> outputConfigs = {
        OutputConfig{2},
        OutputConfig{3}
    };

    if (choice == 2) {
        std::lock_guard<std::mutex> lk(remote_io_state.mtx);
        for (const auto& cfg : configs) {
            remote_io_state.inputs[cfg.id] = false;
            for (int out_id : cfg.linked_outputs)
                remote_io_state.outputs[out_id] = false;
        }
    }

    IOReader reader = (choice == 2) ? makeWSInputReader() : makeTestReader(configs);

    // ── Active Object instances ───────────────────────────────────────────────
    static ControlEntrades controlEntrades{ std::move(reader), 1U };
    static TestObserver        testObserver;
    static ControlRemot        controlRemot;
    static ControlHorari       controlHorari;
    static Rellotge            rellotge;
    static ActuadorSortides    actuadorSortides{makeConsoleWriter()};

    controlEntrades.configure(configs);
    controlRemot.configure(outputConfigs);

    {
        std::lock_guard<std::mutex> lk(control_entrades_state.mtx);
        control_entrades_state.config_inputs = configs;
        for (const auto& cfg : configs) {
            control_entrades_state.inputs[cfg.id] = false;
            for (int out_id : cfg.linked_outputs)
                control_entrades_state.outputs[out_id] = false;
        }
    }

    QP::QF::init();
    QP::QActive::psInit(subscrSto, Q_DIM(subscrSto));

    // Pool 1 (petit): events de comanda (OutputCmdEvt, OutputModeEvt, etc.)
    static QF_MPOOL_EL(OutputCmdEvt) smallPool[16];
    QP::QF::poolInit(smallPool, sizeof(smallPool), sizeof(smallPool[0]));

    // Pool 2 (gran): RellotgeTickEvt, OutputStateEvt, ReconfigureEvt,
    //                InputChangedEvt, EdgeDetectedEvt, OutputResultEvt
    static QF_MPOOL_EL(ReconfigureEvt) largePool[32];
    QP::QF::poolInit(largePool, sizeof(largePool), sizeof(largePool[0]));

    // Prioritats (alt→baix): Rellotge > ControlEntrades > [ControlRemot] > ControlHorari > TestObserver/ActuadorSortides
    // ControlRemot no s'inicia en mode test: evita que m_commandedOutputs sobreescrigui
    // els outputs del test reader i invalidi la detecció always=false.
    rellotge.start(        7U, rellotgeQSto,         Q_DIM(rellotgeQSto),         nullptr, 0U);
    controlEntrades.start( 6U, controlEntradesQSto,  Q_DIM(controlEntradesQSto),  nullptr, 0U);
    controlHorari.start(   4U, controlHorariQSto,    Q_DIM(controlHorariQSto),    nullptr, 0U);
    if (choice != 2) {
        testObserver.start(    3U, testObserverQSto,     Q_DIM(testObserverQSto),     nullptr, 0U);
    } else {
        controlRemot.start(    5U, controlRemotQSto,     Q_DIM(controlRemotQSto),     nullptr, 0U);
        actuadorSortides.start(3U, actuadorSortidesQSto, Q_DIM(actuadorSortidesQSto), nullptr, 0U);
    }

    // Carrega l'horari per defecte
    {
        std::lock_guard<std::mutex> lk(control_horari_state.mtx);
        control_horari_state.programacioHoraria.assign(JSON_HORARI, sizeof(JSON_HORARI) - 1);
    }
    controlHorari.loadJson(JSON_HORARI, sizeof(JSON_HORARI) - 1);

    HttpServer::start(8080, &controlEntrades, &controlRemot, "Windows");

    int ret = QP::QF::run();

    HttpServer::stop();
    return ret;
}
