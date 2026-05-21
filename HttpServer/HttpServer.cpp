#include "HttpServer.h"
#include "../ControlEntrades/ControlEntradesState.h"
#include "../RemoteIO/RemoteIOState.h"
#include "../ControlRemot/ControlRemot.h"
#include "../ControlRemot/ControlRemotState.h"
#include "../ControlHorari/ControlHorariState.h"
#include "../Rellotge/RellotgeState.h"
#include "../LogState.h"
#include "../signals.h"
#include <thread>
#include <atomic>
#include <string>
#include <cstdio>
#include <cstring>
#include <mutex>

extern "C" {
#include "../mongoose/mongoose.h"
}

#include "../web/index_html.h"

// ── JSON helpers ──────────────────────────────────────────────────────────────

static std::string bool_map_to_json(const std::unordered_map<int, bool>& m) {
    std::string s = "{";
    bool first = true;
    for (auto const& [k, v] : m) {
        if (!first) s += ",";
        s += "\"" + std::to_string(k) + "\":" + (v ? "true" : "false");
        first = false;
    }
    return s + "}";
}

static std::string int_map_to_json(const std::unordered_map<int, int>& m) {
    std::string s = "{";
    bool first = true;
    for (auto const& [k, v] : m) {
        if (!first) s += ",";
        s += "\"" + std::to_string(k) + "\":" + std::to_string(v);
        first = false;
    }
    return s + "}";
}

static std::string int_vec_to_json(const std::vector<int>& v) {
    std::string s = "[";
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i > 0) s += ",";
        s += std::to_string(v[i]);
    }
    return s + "]";
}

static std::string json_str(const std::string& v) {
    std::string out = "\"";
    for (char c : v) {
        if      (c == '"')  out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else                out += c;
    }
    return out + "\"";
}

// ── Missatge WS unificat (Control Entrades + Control Sortides) ────────────────

static const char* s_platform = "Windows";

static std::string build_ws_msg(
        const std::unordered_map<int, bool>& inputs,
        const std::vector<int>&              edges,
        const std::unordered_map<int, int>&  counts,
        const std::unordered_map<int, OutputInfo>& cs_outputs,
        int hour, int minute, int wday)
{
    static const char* DAYS[7] = {
        "dilluns","dimarts","dimecres","dijous","divendres","dissabte","diumenge"};
    char tbuf[8];
    std::snprintf(tbuf, sizeof(tbuf), "%02d:%02d", hour, minute);

    std::string s;
    s += "{\"inputs\":"      + bool_map_to_json(inputs);
    s += ",\"last_edges\":"  + int_vec_to_json(edges);
    s += ",\"edge_counts\":" + int_map_to_json(counts);
    s += ",\"time\":\""      + std::string(tbuf) + "\"";
    s += ",\"day\":\""       + std::string((wday >= 0 && wday < 7) ? DAYS[wday] : "") + "\"";
    s += ",\"cs_outputs\":{";
    bool first = true;
    for (auto const& [k, v] : cs_outputs) {
        if (!first) s += ",";
        s += "\"" + std::to_string(k) + "\":"
             "{\"state\":"     + (v.state     ? "true" : "false") + ","
             "\"commanded\":"  + (v.commanded ? "true" : "false") + ","
             "\"result\":"     + (v.result    ? "true" : "false") + ","
             "\"mode\":\""     + (v.remote    ? "REMOTE" : "AUTO") + "\"}";
        first = false;
    }
    s += "},\"platform\":\"";
    s += s_platform;
    s += "\"}";
    return s;
}

// ── Push WS si hi ha dades pendents ──────────────────────────────────────────

static void push_if_pending(struct mg_mgr* mgr) {
    bool pending = control_entrades_state.push_pending.exchange(false)
                 | control_remot_state.push_pending.exchange(false)
                 | rellotge_state.push_pending.exchange(false);
    if (!pending) return;

    std::unordered_map<int, bool>       inputs;
    std::unordered_map<int, int>        counts;
    std::vector<int>                    edges;
    std::unordered_map<int, OutputInfo> cs_outputs;
    int hh, mm, wd;

    {
        std::lock_guard<std::mutex> lk(control_entrades_state.mtx);
        inputs  = control_entrades_state.inputs;
        edges   = std::move(control_entrades_state.last_edges);
        counts  = control_entrades_state.edge_counts;
    }
    {
        std::lock_guard<std::mutex> lk(control_remot_state.mtx);
        cs_outputs = control_remot_state.outputsResult;
    }
    {
        std::lock_guard<std::mutex> lk(rellotge_state.mtx);
        hh = rellotge_state.hour;
        mm = rellotge_state.minute;
        wd = rellotge_state.wday;
    }

    std::string msg = build_ws_msg(inputs, edges, counts,
                                   cs_outputs, hh, mm, wd);
    for (struct mg_connection* c = mgr->conns; c != nullptr; c = c->next) {
        if (c->is_websocket)
            mg_ws_send(c, msg.c_str(), msg.size(), WEBSOCKET_OP_TEXT);
    }
}

// ── Parser JSON {"1":true} → unordered_map ───────────────────────────────────

static void parse_bool_object(struct mg_str s, std::unordered_map<int, bool>& result) {
    const char* p   = s.buf;
    const char* end = s.buf + s.len;
    while (p < end && *p != '{') ++p;
    if (p >= end) return;
    ++p;
    while (p < end) {
        while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) ++p;
        if (p >= end || *p == '}') break;
        if (*p != '"') { ++p; continue; }
        ++p;
        const char* ks = p;
        while (p < end && *p != '"') ++p;
        int key = std::atoi(std::string(ks, p - ks).c_str());
        if (p < end) ++p;
        while (p < end && *p != ':') ++p;
        if (p < end) ++p;
        while (p < end && (*p == ' ' || *p == '\t')) ++p;
        if (p + 4 <= end && std::strncmp(p, "true",  4) == 0) { result[key] = true;  p += 4; }
        else if (p + 5 <= end && std::strncmp(p, "false", 5) == 0) { result[key] = false; p += 5; }
        while (p < end && *p != ',' && *p != '}') ++p;
        if (p < end && *p == ',') ++p;
    }
}

// ── Handlers ─────────────────────────────────────────────────────────────────

static QP::QActive* s_edgeDetector = nullptr;
static ControlRemot* s_controlRemot = nullptr;

static void post_reconfigure(const std::vector<InputConfig>& configs);

static void http_fn(struct mg_connection* c, int ev, void* ev_data) {
    if (ev == MG_EV_HTTP_MSG) {
        auto* hm = static_cast<struct mg_http_message*>(ev_data);

        // ── WS upgrade ────────────────────────────────────────────────────────
        if (mg_match(hm->uri, mg_str("/ws"), NULL)) {
            mg_ws_upgrade(c, hm, NULL);
            std::unordered_map<int, bool>       inputs;
            std::unordered_map<int, int>        counts;
            std::unordered_map<int, OutputInfo> cs_outputs;
            int hh, mm, wd;
            {
                std::lock_guard<std::mutex> lk(control_entrades_state.mtx);
                inputs = control_entrades_state.inputs;  counts = control_entrades_state.edge_counts;
            }
            {
                std::lock_guard<std::mutex> lk(control_remot_state.mtx);
                cs_outputs = control_remot_state.outputsResult;
            }
            {
                std::lock_guard<std::mutex> lk(rellotge_state.mtx);
                hh = rellotge_state.hour;  mm = rellotge_state.minute;  wd = rellotge_state.wday;
            }
            std::string msg = build_ws_msg(inputs, {}, counts, cs_outputs, hh, mm, wd);
            mg_ws_send(c, msg.c_str(), msg.size(), WEBSOCKET_OP_TEXT);

        // ── GET /config_inputs ────────────────────────────────────────────────
        } else if (mg_match(hm->uri, mg_str("/config_inputs"), NULL)
                   && mg_match(hm->method, mg_str("GET"), NULL)) {
            std::string body;
            {
                std::lock_guard<std::mutex> lk(control_entrades_state.mtx);
                body = "[";
                for (std::size_t i = 0; i < control_entrades_state.config_inputs.size(); ++i) {
                    const auto& cfg = control_entrades_state.config_inputs[i];
                    if (i > 0) body += ",";
                    body += "{\"id\":"               + std::to_string(cfg.id);
                    body += ",\"detect_edge\":\""     + std::string(cfg.detect_edge == EdgePolarity::rising ? "rising" : "falling") + "\"";
                    body += ",\"detection_always\":" + std::string(cfg.detection_always ? "true" : "false");
                    body += ",\"linked_outputs\":"   + int_vec_to_json(cfg.linked_outputs) + "}";
                }
                body += "]";
            }
            mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s", body.c_str());

        // ── PUT /config_inputs ────────────────────────────────────────────────
        } else if (mg_match(hm->uri, mg_str("/config_inputs"), NULL)
                   && mg_match(hm->method, mg_str("PUT"), NULL)) {
            std::vector<InputConfig> allConfigs;
            for (int i = 0; i < ReconfigureEvt::MAX_CONFIGS; ++i) {
                char arrpath[16];
                std::snprintf(arrpath, sizeof(arrpath), "$[%d]", i);
                int elen = 0;
                int eoff = mg_json_get(hm->body, arrpath, &elen);
                if (eoff < 0) break;
                struct mg_str elem = { hm->body.buf + eoff, (size_t)elen };
                long id = mg_json_get_long(elem, "$.id", -1);
                if (id < 0) break;
                InputConfig cfg;
                cfg.id = (int)id;
                { int slen = 0; int soff = mg_json_get(elem, "$.detect_edge", &slen);
                  cfg.detect_edge = (soff >= 0 && slen >= 6 && std::strncmp(elem.buf + soff + 1, "rising", 6) == 0)
                                    ? EdgePolarity::rising : EdgePolarity::falling; }
                { bool v = false; mg_json_get_bool(elem, "$.detection_always", &v); cfg.detection_always = v; }
                { int llen = 0;
                  int loff = mg_json_get(elem, "$.linked_outputs", &llen);
                  if (loff >= 0) {
                      struct mg_str linked = { elem.buf + loff, (size_t)llen };
                      for (int k = 0; k < ReconfigureEvt::MAX_LINKED; ++k) {
                          char kpath[8];
                          std::snprintf(kpath, sizeof(kpath), "$[%d]", k);
                          long out_id = mg_json_get_long(linked, kpath, -1);
                          if (out_id < 0) break;
                          cfg.linked_outputs.push_back((int)out_id);
                      }
                  }
                }
                allConfigs.push_back(cfg);
            }
            if (allConfigs.empty()) { mg_http_reply(c, 400, "", "empty or invalid array\n"); return; }
            post_reconfigure(allConfigs);
            mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{}");

        // ── POST /control_outputs ─────────────────────────────────────────────
        } else if (mg_match(hm->uri, mg_str("/control_outputs"), NULL)
                   && mg_match(hm->method, mg_str("POST"), NULL)) {
            if (s_controlRemot && hm->body.len > 0)
                s_controlRemot->handleJson(hm->body.buf, hm->body.len);
            mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{}");

        // ── GET /programacio_horaria ──────────────────────────────────────────
        } else if (mg_match(hm->uri, mg_str("/programacio_horaria"), NULL)
                   && mg_match(hm->method, mg_str("GET"), NULL)) {
            std::string body;
            {
                std::lock_guard<std::mutex> lk(control_horari_state.mtx);
                body = control_horari_state.programacioHoraria;
            }
            mg_http_reply(c, 200, "Content-Type: application/json\r\n",
                          "%.*s", (int)body.size(), body.c_str());

        // ── POST /programacio_horaria ─────────────────────────────────────────
        } else if (mg_match(hm->uri, mg_str("/programacio_horaria"), NULL)
                   && mg_match(hm->method, mg_str("POST"), NULL)) {
            if (hm->body.len > 0) {
                {
                    std::lock_guard<std::mutex> lk(control_horari_state.mtx);
                    control_horari_state.programacioHoraria.assign(hm->body.buf, hm->body.len);
                }
                control_horari_state.load_pending.store(true);
            }
            mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{}");

        // ── GET / ─────────────────────────────────────────────────────────────
        } else if (mg_match(hm->uri, mg_str("/"), NULL)) {
            mg_printf(c, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nCache-Control: no-cache\r\nContent-Length: %u\r\n\r\n",
                      (unsigned)std::strlen(s_html));
            mg_send(c, s_html, std::strlen(s_html));

        } else {
            mg_http_reply(c, 404, "", "Not found\n");
        }

    } else if (ev == MG_EV_WS_MSG) {
        auto* wm = static_cast<struct mg_ws_message*>(ev_data);
        if ((wm->flags & 0xF) != WEBSOCKET_OP_TEXT) return;

        std::unordered_map<int, bool> inputs;
        int ilen = 0;
        int ioff = mg_json_get(wm->data, "$.inputs", &ilen);
        if (ioff > 0) parse_bool_object({wm->data.buf + ioff, (size_t)ilen}, inputs);
        if (!inputs.empty()) {
            std::lock_guard<std::mutex> lk(remote_io_state.mtx);
            for (auto const& [id, v] : inputs) remote_io_state.inputs[id] = v;
        }
    }
}

static void post_reconfigure(const std::vector<InputConfig>& configs) {
    if (!s_edgeDetector) return;
    {
        std::lock_guard<std::mutex> lk(remote_io_state.mtx);
        remote_io_state.inputs.clear();
        remote_io_state.outputs.clear();
        for (const auto& cfg : configs) {
            remote_io_state.inputs[cfg.id] = false;
            for (int out_id : cfg.linked_outputs)
                remote_io_state.outputs[out_id] = false;
        }
    }
    auto* evt = Q_NEW(ReconfigureEvt, RECONFIGURE_SIG);
    evt->n_configs = 0;
    for (const auto& cfg : configs) {
        if (evt->n_configs >= ReconfigureEvt::MAX_CONFIGS) break;
        auto& e          = evt->entries[evt->n_configs++];
        e.id               = cfg.id;
        e.detect_edge      = cfg.detect_edge;
        e.detection_always = cfg.detection_always;
        e.n_linked = 0;
        for (int out : cfg.linked_outputs) {
            if (e.n_linked < ReconfigureEvt::MAX_LINKED)
                e.linked_outputs[e.n_linked++] = out;
        }
    }
    s_edgeDetector->post_(evt, nullptr);
}

// ── Server thread ─────────────────────────────────────────────────────────────

static std::atomic<bool> s_running{false};
static std::thread       s_thread;

static void server_loop(uint16_t port) {
    char addr[32];
    std::snprintf(addr, sizeof(addr), "http://0.0.0.0:%u", port);
    struct mg_mgr mgr;
    mg_mgr_init(&mgr);
    mg_http_listen(&mgr, addr, http_fn, NULL);
    log_append("HttpServer", "listening on", addr);
    while (s_running.load()) {
        mg_mgr_poll(&mgr, 100);
        push_if_pending(&mgr);
    }
    mg_mgr_free(&mgr);
}

void HttpServer::start(uint16_t port, QP::QActive* edgeDetector, QP::QActive* controlRemot,
                       const char* platform) {
    s_platform     = platform;
    s_edgeDetector = edgeDetector;
    s_controlRemot = static_cast<ControlRemot*>(controlRemot);
    s_running = true;
    s_thread  = std::thread(server_loop, port);
}

void HttpServer::stop() {
    s_running = false;
    if (s_thread.joinable()) s_thread.join();
}
