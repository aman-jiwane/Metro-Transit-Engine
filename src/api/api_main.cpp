#include <iostream>
#include <memory>

#include "../../include/db/Database.h"
#include "../../include/db/SchemaInitializer.h"
#include "../../include/network/MetroNetwork.h"
#include "../../include/network/DelayService.h"
#include "../../include/network/SqliteNetworkRepository.h"
#include "../../include/ticketing/SqliteCardRepository.h"
#include "../../include/ticketing/SqliteTripRepository.h"
#include "../../include/ticketing/SqliteFareRepository.h"
#include "../../include/ticketing/Turnstile.h"
#include "../../include/routing/CachedShortestTimeStrategy.h"
#include "../../include/service/MetroService.h"

#include <httplib.h>
#include <nlohmann/json.hpp>


using json = nlohmann::json;
using namespace std;

// ---------------------------------------------------------------------------
// Same network-loading helper as main.cpp - both entry points build their
// own MetroService over the same metro_core components, but neither
// duplicates the business logic itself.
// ---------------------------------------------------------------------------
namespace {

void loadNetworkFromDb(INetworkRepository& repo, MetroNetwork& network) {
    for (const auto& l : repo.getAllLines()) network.addLine(l.id, l.name);
    for (const auto& s : repo.getAllStations()) network.addStation(s.id, s.name, s.isInterchange);
    for (const auto& r : repo.getAllRoutes()) network.addRoute(r.stationA, r.stationB, r.timeMinutes, r.distanceKm, r.lineId);
}

json toJson(const CardInfoResult& r) {
    return json{
        {"success", r.success}, {"message", r.message}, {"created", r.created},
        {"cardId", r.cardId}, {"cardType", r.cardType}, {"balance", r.balance},
        {"checkedIn", r.checkedIn}, {"checkedInStation", r.checkedInStation}
    };
}

json toJson(const RouteSearchResult& r) {
    return json{
        {"success", r.success}, {"message", r.message}, {"stations", r.stationNames},
        {"metric", r.metric}, {"metricType", r.isTimeMetric ? "minutes" : "stops"}
    };
}

json toJson(const TapOutcome& r) {
    return json{
        {"success", r.success}, {"message", r.message}, {"isCheckout", r.isCheckout},
        {"entryStation", r.entryStationName}, {"exitStation", r.exitStationName},
        {"fareType", r.fareType}, {"fareCharged", r.fareCharged}, {"remainingBalance", r.remainingBalance}
    };
}

json toJson(const SimpleResult& r) {
    return json{{"success", r.success}, {"message", r.message}};
}

json toJson(const IncidentActionResult& r) {
    return json{{"success", r.success}, {"message", r.message}, {"eventId", r.eventId}, {"lineName", r.lineName}};
}

json toJson(const IncidentSummary& r) {
    return json{
        {"eventId", r.eventId}, {"fromStation", r.fromStationName}, {"toStation", r.toStationName},
        {"line", r.lineName}, {"incidentType", r.incidentType}, {"severity", r.severity},
        {"delayMinutes", r.delayMinutes}, {"trackClosed", r.trackClosed}, {"reportedBy", r.reportedBy}
    };
}

// Parses the request body as JSON. Returns false (and writes a 400 response)
// if the body isn't valid JSON - every handler that needs a body starts
// with this so malformed input never reaches MetroService.
bool parseBody(const httplib::Request& req, httplib::Response& res, json& out) {
    try {
        out = json::parse(req.body);
        return true;
    } catch (const json::parse_error& e) {
        res.status = 400;
        res.set_content(json{{"success", false}, {"message", string("Malformed JSON body: ") + e.what()}}.dump(), "application/json");
        return false;
    }
}

void sendJson(httplib::Response& res, const json& body, int status = 200) {
    res.status = status;
    res.set_content(body.dump(), "application/json");
}

} // namespace

int main() {
    // ---- Composition root - identical shape to main.cpp's ----
    Database db("metro.db");
    SchemaInitializer::ensureSchema(db);

    MetroNetwork& network = MetroNetwork::getInstance();
    DelayService& delayService = DelayService::getInstance();

    SqliteNetworkRepository networkRepo(db);
    loadNetworkFromDb(networkRepo, network);

    SqliteCardRepository cardRepo(db);
    SqliteTripRepository tripRepo(db);
    SqliteFareRepository fareRepo(db);

    CachedShortestTimeStrategy cachedRouter;
    delayService.subscribeGlobal(&cachedRouter);

    Turnstile gate(network, delayService, cachedRouter, cardRepo, tripRepo, fareRepo);
    MetroService service(network, delayService, cardRepo, cachedRouter, gate);

    httplib::Server svr;

    // Serves the static demo page (static/index.html, etc.) from the same
    // origin as the API, so the page's fetch() calls need no CORS setup.
    svr.set_mount_point("/", "./static");

    svr.Get("/api/health", [&](const httplib::Request&, httplib::Response& res) {
        sendJson(res, json{{"status", "ok"}, {"stations", network.getTotalStations()}});
    });

    // ---- Stations (Added for Dropdowns) ----
    svr.Get("/api/stations", [&](const httplib::Request&, httplib::Response& res) {
        json stationsArray = json::array();
        
        // Fetch all stations securely from the database repository
        for (const auto& s : networkRepo.getAllStations()) {
            stationsArray.push_back({
                {"id", s.id},
                {"name", s.name}
            });
        }

        json response;
        response["success"] = true;
        response["data"] = stationsArray;

        sendJson(res, response);
    });

    // ---- Ticketing ----

    svr.Post("/api/cards", [&](const httplib::Request& req, httplib::Response& res) {
        json body;
        if (!parseBody(req, res, body)) return;
        if (!body.contains("cardId") || !body.contains("amount")) {
            sendJson(res, json{{"success", false}, {"message", "cardId and amount are required."}}, 400);
            return;
        }
        string cardId = body["cardId"].get<string>();
        double amount = body["amount"].get<double>();
        string cardType = body.value("cardType", "Regular");

        CardInfoResult result = service.createOrRechargeCard(cardId, cardType, amount);
        sendJson(res, toJson(result), result.success ? (result.created ? 201 : 200) : 400);
    });

    svr.Get(R"(/api/cards/([^/]+))", [&](const httplib::Request& req, httplib::Response& res) {
        string cardId = req.matches[1];
        CardInfoResult result = service.getCardInfo(cardId);
        sendJson(res, toJson(result), result.success ? 200 : 404);
    });

    // ---- Routing ----

    svr.Get("/api/routes", [&](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_param("from") || !req.has_param("to")) {
            sendJson(res, json{{"success", false}, {"message", "'from' and 'to' query params are required."}}, 400);
            return;
        }
        string from = req.get_param_value("from");
        string to = req.get_param_value("to");
        string strategy = req.has_param("strategy") ? req.get_param_value("strategy") : "fastest";

        RouteSearchResult result = service.searchRoute(from, to, strategy);
        sendJson(res, toJson(result), result.success ? 200 : 404);
    });

    // ---- Turnstile ----

    svr.Post("/api/turnstile/tap-in", [&](const httplib::Request& req, httplib::Response& res) {
        json body;
        if (!parseBody(req, res, body)) return;
        if (!body.contains("cardId") || !body.contains("station")) {
            sendJson(res, json{{"success", false}, {"message", "cardId and station are required."}}, 400);
            return;
        }
        TapOutcome result = service.tapIn(body["cardId"].get<string>(), body["station"].get<string>());
        sendJson(res, toJson(result));
    });

    svr.Post("/api/turnstile/tap-out", [&](const httplib::Request& req, httplib::Response& res) {
        json body;
        if (!parseBody(req, res, body)) return;
        if (!body.contains("cardId") || !body.contains("station")) {
            sendJson(res, json{{"success", false}, {"message", "cardId and station are required."}}, 400);
            return;
        }
        TapOutcome result = service.tapOut(body["cardId"].get<string>(), body["station"].get<string>());
        sendJson(res, toJson(result));
    });

    svr.Get("/api/turnstile/status", [&](const httplib::Request&, httplib::Response& res) {
        sendJson(res, json{{"state", service.getTurnstileStatus()}});
    });

    // ---- Admin: incidents & tracks ----

    svr.Post("/api/admin/incidents", [&](const httplib::Request& req, httplib::Response& res) {
        json body;
        if (!parseBody(req, res, body)) return;
        for (const char* field : {"from", "to", "type", "severity", "durationMinutes"}) {
            if (!body.contains(field)) {
                sendJson(res, json{{"success", false}, {"message", string("Missing field: ") + field}}, 400);
                return;
            }
        }
        IncidentActionResult result = service.reportIncident(
            body["from"].get<string>(), body["to"].get<string>(),
            body["type"].get<string>(), body["severity"].get<string>(),
            body["durationMinutes"].get<int>(), body.value("reportedBy", "API")
        );
        sendJson(res, toJson(result), result.success ? 201 : 400);
    });

    svr.Delete(R"(/api/admin/incidents/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        int eventId = std::stoi(req.matches[1]);
        SimpleResult result = service.removeIncident(eventId);
        sendJson(res, toJson(result), result.success ? 200 : 404);
    });

    svr.Get("/api/admin/incidents", [&](const httplib::Request&, httplib::Response& res) {
        json arr = json::array();
        for (const auto& incident : service.getActiveIncidents()) arr.push_back(toJson(incident));
        sendJson(res, arr);
    });

    svr.Post("/api/admin/tracks/close", [&](const httplib::Request& req, httplib::Response& res) {
        json body;
        if (!parseBody(req, res, body)) return;
        if (!body.contains("from") || !body.contains("to") || !body.contains("durationMinutes")) {
            sendJson(res, json{{"success", false}, {"message", "from, to, and durationMinutes are required."}}, 400);
            return;
        }
        IncidentActionResult result = service.closeTrack(body["from"].get<string>(), body["to"].get<string>(), body["durationMinutes"].get<int>());
        sendJson(res, toJson(result), result.success ? 200 : 400);
    });

    svr.Post("/api/admin/tracks/reopen", [&](const httplib::Request& req, httplib::Response& res) {
        json body;
        if (!parseBody(req, res, body)) return;
        if (!body.contains("from") || !body.contains("to")) {
            sendJson(res, json{{"success", false}, {"message", "from and to are required."}}, 400);
            return;
        }
        SimpleResult result = service.reopenTrack(body["from"].get<string>(), body["to"].get<string>());
        sendJson(res, toJson(result), result.success ? 200 : 400);
    });

    svr.Post("/api/admin/turnstile/maintenance", [&](const httplib::Request&, httplib::Response& res) {
        SimpleResult result = service.setTurnstileOutOfOrder();
        sendJson(res, toJson(result));
    });

    svr.Post("/api/admin/turnstile/repair", [&](const httplib::Request&, httplib::Response& res) {
        SimpleResult result = service.repairTurnstile();
        sendJson(res, toJson(result));
    });

    // ---- Subscriptions ----

    svr.Post("/api/subscriptions", [&](const httplib::Request& req, httplib::Response& res) {
        json body;
        if (!parseBody(req, res, body)) return;
        if (!body.contains("cardId") || !body.contains("line")) {
            sendJson(res, json{{"success", false}, {"message", "cardId and line are required."}}, 400);
            return;
        }
        SimpleResult result = service.subscribeToLine(body["cardId"].get<string>(), body["line"].get<string>());
        sendJson(res, toJson(result), result.success ? 200 : 400);
    });

    const int port = 9090;
    cout << "Pune Metro API server listening on http://localhost:" << port << "\n";
    cout << "Demo page: http://localhost:" << port << "/\n";
    svr.listen("0.0.0.0", port);

    return 0;
}