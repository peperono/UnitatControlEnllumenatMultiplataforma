#include "ControlRemot.h"
#include "ControlRemotState.h"
#include "../LogState.h"
#include "../mongoose/mongoose.h"
#include <cstdio>
#include <mutex>

ControlRemotState control_remot_state;

ControlRemot::ControlRemot() noexcept
    : QP::QActive{Q_STATE_CAST(&ControlRemot::initial)},
      m_resultEvt{}
{}

void ControlRemot::configure(const std::vector<OutputConfig>& configs) {
    for (auto const& cfg : configs)
        m_outputs.emplace(cfg.id, OutputEntry{});
}

Q_STATE_DEF(ControlRemot, initial) {
    Q_UNUSED_PAR(e);
    subscribe(OUTPUT_STATE_SIG);
    return tran(&ControlRemot::operating);
}

Q_STATE_DEF(ControlRemot, operating) {
    QP::QState status;
    switch (e->sig) {

        case Q_ENTRY_SIG:
            status = Q_HANDLED();
            break;

        case OUTPUT_STATE_SIG: {
            auto const* ev = Q_EVT_CAST(OutputStateEvt);
            std::string detail;
            for (int i = 0; i < ev->n_outputs; ++i) {
                int id = ev->outputs[i].id;
                auto it = m_outputs.find(id);
                if (it == m_outputs.end()) {
                    log_append("ControlRemot", "WARN OUTPUT_STATE_SIG",
                               "S" + std::to_string(id) + " no declarada, ignorada");
                    continue;
                }
                it->second.state = ev->outputs[i].state;
                if (it->second.mode == OutputEntry::Mode::AUTO)
                    it->second.result = ev->outputs[i].state;
                if (!detail.empty()) detail += ", ";
                detail += "S" + std::to_string(id)
                        + "=" + (ev->outputs[i].state ? "ON" : "OFF");
            }
            log_append("ControlRemot", "<< OUTPUT_STATE_SIG", detail);
            publishResult();
            status = Q_HANDLED();
            break;
        }

        case CTRL_OUTPUT_CMD_SIG: {
            auto const* ev = Q_EVT_CAST(OutputCmdEvt);
            auto it = m_outputs.find(ev->output_id);
            if (it == m_outputs.end()) {
                log_append("ControlRemot", "WARN CTRL_OUTPUT_CMD_SIG",
                           "S" + std::to_string(ev->output_id) + " no declarada, ignorada");
                status = Q_HANDLED();
                break;
            }
            it->second.commanded = ev->activate;
            it->second.result    = ev->activate;
            log_append("ControlRemot", "<< CTRL_OUTPUT_CMD_SIG",
                       "S" + std::to_string(ev->output_id) + "=" + (ev->activate ? "ON" : "OFF"));
            publishResult();
            status = Q_HANDLED();
            break;
        }

        case CTRL_OUTPUT_MODE_SIG: {
            auto const* ev = Q_EVT_CAST(OutputModeEvt);
            auto it = m_outputs.find(ev->output_id);
            if (it == m_outputs.end()) {
                log_append("ControlRemot", "WARN CTRL_OUTPUT_MODE_SIG",
                           "S" + std::to_string(ev->output_id) + " no declarada, ignorada");
                status = Q_HANDLED();
                break;
            }
            it->second.mode = ev->remote ? OutputEntry::Mode::REMOTE
                                         : OutputEntry::Mode::AUTO;
            it->second.result = (it->second.mode == OutputEntry::Mode::REMOTE)
                              ? it->second.commanded : it->second.state;
            log_append("ControlRemot", "<< CTRL_OUTPUT_MODE_SIG",
                       "S" + std::to_string(ev->output_id) + "=" + (ev->remote ? "REMOTE" : "AUTO"));
            publishResult();
            status = Q_HANDLED();
            break;
        }

        case CTRL_OUTPUT_DELETE_SIG: {
            auto const* ev = Q_EVT_CAST(OutputDeleteEvt);
            if (m_outputs.find(ev->output_id) == m_outputs.end()) {
                log_append("ControlRemot", "WARN CTRL_OUTPUT_DELETE_SIG",
                           "S" + std::to_string(ev->output_id) + " no declarada, ignorada");
                status = Q_HANDLED();
                break;
            }
            m_outputs.erase(ev->output_id);
            log_append("ControlRemot", "<< CTRL_OUTPUT_DELETE_SIG",
                       "S" + std::to_string(ev->output_id));
            publishResult();
            status = Q_HANDLED();
            break;
        }

        case CTRL_OUTPUT_RETURN_AUTO_SIG: {
            auto const* ev = Q_EVT_CAST(OutputReturnAutoEvt);
            if (ev->output_id == -1) {
                for (auto& [id, out] : m_outputs) {
                    out.mode   = OutputEntry::Mode::AUTO;
                    out.result = out.state;
                }
            } else {
                auto it = m_outputs.find(ev->output_id);
                if (it == m_outputs.end()) {
                    log_append("ControlRemot", "WARN CTRL_OUTPUT_RETURN_AUTO_SIG",
                               "S" + std::to_string(ev->output_id) + " no declarada, ignorada");
                    status = Q_HANDLED();
                    break;
                }
                it->second.mode   = OutputEntry::Mode::AUTO;
                it->second.result = it->second.state;
            }
            log_append("ControlRemot", "<< CTRL_OUTPUT_RETURN_AUTO_SIG",
                       ev->output_id == -1 ? "all" : "S" + std::to_string(ev->output_id));
            publishResult();
            status = Q_HANDLED();
            break;
        }

        default:
            status = super(&ControlRemot::top);
            break;
    }
    return status;
}

void ControlRemot::publishResult() {
    m_resultEvt.outputs.clear();
    for (auto const& [id, out] : m_outputs)
        m_resultEvt.outputs[id] = out.result;

    {
        std::lock_guard<std::mutex> lk(control_remot_state.mtx);
        control_remot_state.outputsResult.clear();
        for (auto const& [id, out] : m_outputs) {
            control_remot_state.outputsResult[id] = {
                out.state, out.commanded, out.result,
                out.mode == OutputEntry::Mode::REMOTE
            };
        }
    }
    control_remot_state.push_pending.store(true);

    std::string detail;
    for (auto const& [id, out] : m_outputs) {
        if (!detail.empty()) detail += ", ";
        detail += std::to_string(id) + "=";
        detail += out.result ? "ON" : "OFF";
        detail += out.mode == OutputEntry::Mode::REMOTE ? "(REM)" : "(AUTO)";
    }
    log_append("ControlRemot", ">> OUTPUT_RESULT_SIG", detail);

    PUBLISH(&m_resultEvt, this);
}

void ControlRemot::handleJson(const char* buf, std::size_t len) {
    struct mg_str json = mg_str_n(buf, len);

    for (int i = 0; ; ++i) {
        char path[32];
        std::snprintf(path, sizeof(path), "$[%d]", i);
        int elen = 0;
        int eoff = mg_json_get(json, path, &elen);
        if (eoff < 0) break;
        struct mg_str elem = { json.buf + eoff, (std::size_t)elen };

        long id = mg_json_get_long(elem, "$.id", -1);
        if (id < 0) continue;

        int alen = 0;
        int aoff = mg_json_get(elem, "$.action", &alen);
        if (aoff < 0) continue;
        struct mg_str action = { elem.buf + aoff, (std::size_t)alen };

        if (mg_strcmp(action, mg_str("\"activate\"")) == 0 ||
            mg_strcmp(action, mg_str("\"deactivate\"")) == 0) {
            auto* ev      = Q_NEW(OutputCmdEvt, CTRL_OUTPUT_CMD_SIG);
            ev->output_id = (int)id;
            ev->activate  = (mg_strcmp(action, mg_str("\"activate\"")) == 0);
            POST(ev, this);
        } else if (mg_strcmp(action, mg_str("\"set_remote\"")) == 0 ||
                   mg_strcmp(action, mg_str("\"set_auto\"")) == 0) {
            auto* ev      = Q_NEW(OutputModeEvt, CTRL_OUTPUT_MODE_SIG);
            ev->output_id = (int)id;
            ev->remote    = (mg_strcmp(action, mg_str("\"set_remote\"")) == 0);
            POST(ev, this);
        } else if (mg_strcmp(action, mg_str("\"return_auto\"")) == 0) {
            auto* ev      = Q_NEW(OutputReturnAutoEvt, CTRL_OUTPUT_RETURN_AUTO_SIG);
            ev->output_id = (int)id;
            POST(ev, this);
        } else if (mg_strcmp(action, mg_str("\"delete\"")) == 0) {
            auto* ev      = Q_NEW(OutputDeleteEvt, CTRL_OUTPUT_DELETE_SIG);
            ev->output_id = (int)id;
            POST(ev, this);
        }
    }
}
