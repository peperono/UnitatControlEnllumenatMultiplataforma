#pragma once
#include "ControlEntrades/ControlEntrades.h"
#include "../ControlEntrades/ControlEntradesState.h"
#include "qpcpp/include/qpcpp.hpp"
#include "signals.h"
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>

static const char* TEST_LOG_FILE = "test_result.log";

// ── TestStep ──────────────────────────────────────────────────────────────────

struct TestStep {
    std::unordered_map<int, bool> inputs;
    std::unordered_map<int, bool> outputs;
    const char*                   description;
    std::vector<int>              expected_edges;  // vacío = ningún flanco
};

// ── Resultado compartido ──────────────────────────────────────────────────────

static std::vector<int>              g_detectedEdges;
static bool                          g_edgeReceived  = false;
static std::unordered_map<int, bool> g_receivedInputs;
static bool                          g_ioReceived    = false;

// ── TestObserver ──────────────────────────────────────────────────────────────

class TestObserver : public QP::QActive {
public:
    explicit TestObserver() noexcept
        : QP::QActive{Q_STATE_CAST(&TestObserver::initial)}
    {}

private:
    Q_STATE_DECL(initial);
    Q_STATE_DECL(observing);
};

Q_STATE_DEF(TestObserver, initial) {
    Q_UNUSED_PAR(e);
    subscribe(INPUT_CHANGED_SIG);
    subscribe(EDGE_DETECTED_SIG);
    return tran(&TestObserver::observing);
}

Q_STATE_DEF(TestObserver, observing) {
    QP::QState status;
    switch (e->sig) {
        case Q_ENTRY_SIG: {
            status = Q_HANDLED();
            break;
        }
        case INPUT_CHANGED_SIG: {
            auto const* evt  = Q_EVT_CAST(InputChangedEvt);
            g_receivedInputs = evt->inputs;
            g_ioReceived     = true;
            status = Q_HANDLED();
            break;
        }
        case EDGE_DETECTED_SIG: {
            auto const* evt = Q_EVT_CAST(EdgeDetectedEvt);
            g_detectedEdges = evt->input_ids;
            g_edgeReceived  = true;
            status = Q_HANDLED();
            break;
        }
        default: {
            status = super(&TestObserver::top);
            break;
        }
    }
    return status;
}

// ── Verificación ──────────────────────────────────────────────────────────────

static void verifyStep(int stepIdx, const TestStep& s,
                       const std::unordered_map<int, bool>& prevInputs)
{
    // INPUT_CHANGED_SIG s'espera si les entrades han canviat respecte al pas anterior
    bool expected_io = s.inputs != prevInputs;
    bool io_ok = (expected_io == g_ioReceived);
    if (expected_io && g_ioReceived) {
        io_ok = g_receivedInputs == s.inputs;
    }

    // EDGE_DETECTED_SIG
    bool edge_ok;
    if (s.expected_edges.empty()) {
        edge_ok = !g_edgeReceived;
    } else {
        std::vector<int> expected = s.expected_edges;
        std::vector<int> actual   = g_detectedEdges;
        std::sort(expected.begin(), expected.end());
        std::sort(actual.begin(),   actual.end());
        edge_ok = g_edgeReceived && (actual == expected);
    }

    bool ok = io_ok && edge_ok;

    // Escriure resultat al fitxer de log
    if (FILE* f = std::fopen(TEST_LOG_FILE, "a")) {
        std::fprintf(f, "%d,\"%s\",%s\n", stepIdx + 1, s.description, ok ? "OK" : "ERROR");
        std::fclose(f);
    }

    // Reset para el siguiente paso
    g_detectedEdges.clear();
    g_edgeReceived = false;
    g_receivedInputs.clear();
    g_ioReceived = false;
}

// ── makeTestReader ────────────────────────────────────────────────────────────
// Configuració: E1 (falling, always=true), E2 (falling, always=false, linked=S2),
//               E3 (rising, always=true)
//
// E1: flanc en OBERT→TANCAT (0→1)
// E2: flanc en OBERT→TANCAT (0→1) només si S2 és activa
// E3: flanc en TANCAT→OBERT (1→0)
//
//  Pas  1 — estat inicial: E1=O, E2=O, S2=O          → IO canviat, sense flanc
//  Pas  2 — sense canvis                               → sense events
//  Pas  3 — E1=TANCAT                                 → IO + flanc E1
//  Pas  4 — sense canvis                               → sense events
//  Pas  5 — E1=OBERT                                  → IO, sense flanc
//  Pas  6 — E1=TANCAT                                 → IO + flanc E1
//  Pas  7 — E1=OBERT, E2=TANCAT, S2=OBERT            → IO, sense flanc (S2 inactiva)
//  Pas  8 — E2=OBERT                                  → IO, sense flanc
//  Pas  9 — E2=TANCAT, S2=TANCAT                      → IO + flanc E2 (S2 activa)
//  Pas 10 — E2=OBERT                                  → IO, sense flanc
//  Pas 11 — S2=OBERT (sols outputs canvien)           → sense events
//  Pas 12 — E2=TANCAT, S2=OBERT                       → IO, sense flanc (S2 inactiva)
//  Pas 13 — E2=OBERT, E3=TANCAT                       → IO, sense flanc (E3: 0→1, no és rising)
//  Pas 14 — E3=OBERT                                  → IO + flanc E3 (1→0)
//  Pas 15 — E3=TANCAT                                 → IO, sense flanc
//  Pas 16 — E3=OBERT                                  → IO + flanc E3
//  Pas 17 — E1=T, E2=T, E3=T, S2=T                   → IO + flanc E1 + flanc E2
//  Pas 18 — E1=O, E2=O, E3=O, S2=O                   → IO + flanc E3
//  Pas 19 — E1=T, E2=T, E3=T, S2=T                   → IO + flanc E1 + flanc E2
//  Pas 20 — E1=O, E2=O, E3=O, S2=O                   → IO + flanc E3

inline IOReader makeTestReader() {
    {
        std::time_t t = std::time(nullptr);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
        if (FILE* f = std::fopen(TEST_LOG_FILE, "w")) {
            std::fprintf(f, "# Test iniciat: %s\n", buf);
            std::fclose(f);
        }
    }

    static const std::vector<TestStep> steps = {
        // ── E1: falling, always=true ──────────────────────────────────────────
        { {{1,false},{2,false}}, {{2,false}},
          "Estat inicial: E1=O, E2=O, S2=O",                          {} },

        { {{1,false},{2,false}}, {{2,false}},
          "(Sense canvis) => (sense events)",                          {} },

        { {{1,true}, {2,false}}, {{2,false}},
          "(E1=TANCAT) => (flanc E1)",                                 {1} },

        { {{1,true}, {2,false}}, {{2,false}},
          "(Sense canvis) => (sense events)",                          {} },

        { {{1,false},{2,false}}, {{2,false}},
          "(E1=OBERT) => (sense flanc)",                               {} },

        { {{1,true}, {2,false}}, {{2,false}},
          "(E1=TANCAT) => (flanc E1)",                                 {1} },

        // ── E2: falling, always=false, linked=S2 ─────────────────────────────
        { {{1,false},{2,true}},  {{2,false}},
          "(E1=O, E2=TANCAT) amb S2=O => (sense flanc)",              {} },

        { {{1,false},{2,false}}, {{2,false}},
          "(E2=OBERT) => (sense flanc)",                               {} },

        { {{1,false},{2,true}},  {{2,true}},
          "(E2=TANCAT, S2=TANCAT) => (flanc E2)",                      {2} },

        { {{1,false},{2,false}}, {{2,true}},
          "(E2=OBERT) => (sense flanc)",                               {} },

        { {{1,false},{2,false}}, {{2,false}},
          "(S2=OBERT, sols outputs) => (sense events)",                {} },

        { {{1,false},{2,true}},  {{2,false}},
          "(E2=TANCAT) amb S2=O => (sense flanc)",                    {} },

        // ── E3: rising, always=true ───────────────────────────────────────────
        { {{1,false},{2,false},{3,true}},  {{2,false}},
          "(E2=O, E3=TANCAT) => (sense flanc, E3: 0→1 no és rising)", {} },

        { {{1,false},{2,false},{3,false}}, {{2,false}},
          "(E3=OBERT) => (flanc E3)",                                  {3} },

        { {{1,false},{2,false},{3,true}},  {{2,false}},
          "(E3=TANCAT) => (sense flanc)",                              {} },

        { {{1,false},{2,false},{3,false}}, {{2,false}},
          "(E3=OBERT) => (flanc E3)",                                  {3} },

        // ── Combinat: E1+E2+E3 i S2 ──────────────────────────────────────────
        { {{1,true},{2,true},{3,true}},    {{2,true}},
          "(E1=T, E2=T, E3=T, S2=T) => (flanc E1, flanc E2)",         {1,2} },

        { {{1,false},{2,false},{3,false}}, {{2,false}},
          "(E1=O, E2=O, E3=O, S2=O) => (flanc E3)",                   {3} },

        { {{1,true},{2,true},{3,true}},    {{2,true}},
          "(E1=T, E2=T, E3=T, S2=T) => (flanc E1, flanc E2)",         {1,2} },

        { {{1,false},{2,false},{3,false}}, {{2,false}},
          "(E1=O, E2=O, E3=O, S2=O) => (flanc E3)",                   {3} },
    };

    static int step = 0;
    static std::unordered_map<int, bool> prevInputs = steps[0].inputs;

    return [](std::unordered_map<int, bool>& inputs,
              std::unordered_map<int, bool>& outputs)
    {
        static auto stepTime = std::chrono::steady_clock::now();
        static const std::chrono::milliseconds STEP_DELAY{50};
        static std::unordered_map<int, bool> lastInputs;
        static std::unordered_map<int, bool> lastOutputs;
        static bool waiting = false;
        static int  warmup  = 5; // ticks per estabilitzar m_prevInputs de ControlEntrades

        inputs  = lastInputs;
        outputs = lastOutputs;

        // ── Warm-up: aplica l'estat inicial sense verificar ───────────────────
        if (warmup > 0) {
            inputs = steps[0].inputs;
            outputs = steps[0].outputs;
            lastInputs  = steps[0].inputs;
            lastOutputs = steps[0].outputs;
            if (--warmup == 0) {
                g_detectedEdges.clear();
                g_edgeReceived  = false;
                g_receivedInputs.clear();
                g_ioReceived    = false;
            }
            return;
        }

        // ── Aplica el pas actual ──────────────────────────────────────────────
        if (!waiting) {
            if (step >= static_cast<int>(steps.size())) {
                std::printf("\n=== Test completat ===\n");
                QP::QF::stop();
                return;
            }
            inputs      = steps[step].inputs;
            outputs     = steps[step].outputs;
            lastInputs  = steps[step].inputs;
            lastOutputs = steps[step].outputs;
            waiting     = true;
            stepTime    = std::chrono::steady_clock::now();
            return;
        }

        // ── Espera que ControlEntrades processi les entrades ──────────────────
        if (std::chrono::steady_clock::now() - stepTime < STEP_DELAY) return;

        // ── Verifica el pas i avança ──────────────────────────────────────────
        verifyStep(step, steps[step], prevInputs);
        prevInputs = steps[step].inputs;
        ++step;
        waiting = false;
    };
}
