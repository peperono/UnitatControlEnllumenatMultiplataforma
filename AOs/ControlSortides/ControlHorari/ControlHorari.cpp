#include "ControlHorari.h"
#include "ControlHorariState.h"
#include "Log/LogState.h"
#include "mongoose/mongoose.h"
#include <cstdio>
#include <mutex>

ControlHorariState control_horari_state;

static const char* DAY_KEYS[7] = {
    "$.dilluns", "$.dimarts", "$.dimecres", "$.dijous",
    "$.divendres", "$.dissabte", "$.diumenge"
};

ControlHorari::ControlHorari() noexcept
    : QP::QActive{Q_STATE_CAST(&ControlHorari::initial)}
{}

Q_STATE_DEF(ControlHorari, initial) {
    Q_UNUSED_PAR(e);
    subscribe(RELLOTGE_TICK_SIG);
    return tran(&ControlHorari::operating);
}

Q_STATE_DEF(ControlHorari, operating) {
    QP::QState status;
    switch (e->sig) {

        case Q_ENTRY_SIG:
            status = Q_HANDLED();
            break;

        case RELLOTGE_TICK_SIG: {
            auto const* tick = Q_EVT_CAST(RellotgeTickEvt);
            int hh   = tick->hour;
            int mm   = tick->minute;
            int wday = tick->wday;

            if (control_horari_state.load_pending.exchange(false)) {
                std::string copy;
                {
                    std::lock_guard<std::mutex> lk(control_horari_state.mtx);
                    copy = control_horari_state.programacioHoraria;
                }
                loadJson(copy.c_str(), copy.size());
            }

            int matches = 0;
            for (auto const& m : m_schedule[wday])
                if (m.hour == hh && m.minute == mm) ++matches;

            if (matches > 0) {
                auto* ev = Q_NEW(OutputStateEvt, OUTPUT_STATE_SIG);
                ev->n_outputs = 0;
                for (auto const& m : m_schedule[wday]) {
                    if (m.hour == hh && m.minute == mm
                        && ev->n_outputs < OutputStateEvt::MAX_OUTPUTS) {
                        ev->outputs[ev->n_outputs++] = { m.id, m.on };
                    }
                }
                std::string detail;
                for (int i = 0; i < ev->n_outputs; ++i) {
                    if (i > 0) detail += ", ";
                    detail += "S" + std::to_string(ev->outputs[i].id);
                    detail += ev->outputs[i].state ? "=ON" : "=OFF";
                }
                log_append("ControlHorari", ">> OUTPUT_STATE_SIG", detail);
                PUBLISH(ev, this);
            }
            status = Q_HANDLED();
            break;
        }

        default:
            status = super(&ControlHorari::top);
            break;
    }
    return status;
}

void ControlHorari::loadJson(const char* buf, std::size_t len) {
    for (auto& v : m_schedule) v.clear();

    struct mg_str json = mg_str_n(buf, len);

    for (int d = 0; d < 7; ++d) {
        int alen = 0;
        int aoff = mg_json_get(json, DAY_KEYS[d], &alen);
        if (aoff < 0) continue;
        struct mg_str arr = { json.buf + aoff, (std::size_t)alen };

        for (int i = 0; ; ++i) {
            char path[32];
            std::snprintf(path, sizeof(path), "$[%d]", i);
            int elen = 0;
            int eoff = mg_json_get(arr, path, &elen);
            if (eoff < 0) break;
            struct mg_str elem = { arr.buf + eoff, (std::size_t)elen };

            long id = mg_json_get_long(elem, "$.id", -1);
            if (id < 0) continue;

            int act_len = 0;
            int act_off = mg_json_get(elem, "$.act", &act_len);
            if (act_off < 0) continue;
            struct mg_str act = { elem.buf + act_off, (std::size_t)act_len };
            bool on = (mg_strcmp(act, mg_str("\"on\"")) == 0);

            int tlen = 0;
            int toff = mg_json_get(elem, "$.time", &tlen);
            if (toff < 0 || tlen < 7) continue;
            const char* t = elem.buf + toff + 1; // salta la cometa inicial
            int hour   = (t[0] - '0') * 10 + (t[1] - '0');
            int minute = (t[3] - '0') * 10 + (t[4] - '0');

            m_schedule[d].push_back({ (int)id, on, (uint8_t)hour, (uint8_t)minute });
        }
    }

    int total = 0;
    for (auto const& v : m_schedule) total += (int)v.size();
    log_append("ControlHorari", "calendari carregat", std::to_string(total) + " maniobres");
}
