#include <iostream>
#include <memory>
#include <limits>
#include <cstdlib>

#include "../include/db/Database.h"
#include "../include/db/SchemaInitializer.h"
#include "../include/network/MetroNetwork.h"
#include "../include/network/DelayService.h"
#include "../include/network/SqliteNetworkRepository.h"
#include "../include/ticketing/SqliteCardRepository.h"
#include "../include/ticketing/SqliteTripRepository.h"
#include "../include/ticketing/SqliteFareRepository.h"
#include "../include/ticketing/Turnstile.h"
#include "../include/routing/CachedShortestTimeStrategy.h"
#include "../include/service/MetroService.h"

using namespace std;

// Builds the in-memory routing graph by reading it out of SQLite.
void loadNetworkFromDb(INetworkRepository& repo, MetroNetwork& network) {
    for (const auto& l : repo.getAllLines()) {
        network.addLine(l.id, l.name);
    }
    for (const auto& s : repo.getAllStations()) {
        network.addStation(s.id, s.name, s.isInterchange);
    }
    for (const auto& r : repo.getAllRoutes()) {
        network.addRoute(r.stationA, r.stationB, r.timeMinutes, r.distanceKm, r.lineId);
    }
}

string readString() {
    string input;
    cin >> ws;
    getline(cin, input);
    return input;
}

void clearBadInput() {
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

bool readInt(int& out) {
    if (cin >> out) return true;
    if (cin.eof()) {
        cout << "\nInput stream closed. Shutting down.\n";
        std::exit(0);
    }
    clearBadInput();
    return false;
}

bool readDouble(double& out) {
    if (cin >> out) return true;
    if (cin.eof()) {
        cout << "\nInput stream closed. Shutting down.\n";
        std::exit(0);
    }
    clearBadInput();
    return false;
}

int main() {
    // ---- Composition root ----
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

    // Every operation goes through this. main.cpp's only job from here is
    // reading input and printing MetroService's results - it contains no
    // business logic of its own. api_main.cpp builds an equivalent
    // MetroService over the same components and exposes it as JSON instead.
    MetroService service(network, delayService, cardRepo, cachedRouter, gate);

    cout << "\n======================================================\n";
    cout << "    WELCOME TO THE PUNE METRO TRANSIT SYSTEM    \n";
    cout << "======================================================\n";
    cout << "Successfully loaded " << network.getTotalStations() << " stations from metro.db.\n";

    while (true) {
        cout << "\n-------------------- MAIN MENU --------------------\n";
        cout << "[1] Buy or Recharge Smart Card\n";
        cout << "[2] Search Route (Plan Journey)\n";
        cout << "[3] Tap IN at Turnstile\n";
        cout << "[4] Tap OUT at Turnstile\n";
        cout << "[5] [Admin] Operations Menu\n";
        cout << "[6] Subscribe to Line Alerts\n";
        cout << "[0] Exit System\n";
        cout << "Enter your choice: ";

        int choice;
        if (!readInt(choice)) {
            cout << "Invalid input. Please enter a number.\n";
            continue;
        }

        if (choice == 0) {
            cout << "Shutting down system. Goodbye!\n";
            break;
        }

        switch (choice) {
            case 1: { // BUY / RECHARGE CARD
                cout << "Enter Card ID (e.g., USER123): ";
                string cardId = readString();

                cout << "Enter amount to add (Rs): ";
                double amount;
                if (!readDouble(amount)) {
                    cout << "Invalid amount.\n";
                    break;
                }

                string cardType = "Regular";
                // Only ask for a type if this looks like a new card - the
                // service layer itself will confirm, but asking up front
                // keeps the CLI flow matching what a first-time user expects.
                CardInfoResult existing = service.getCardInfo(cardId);
                if (!existing.success) {
                    cout << "Select Card Type:\n"
                         << "  [1] Regular\n  [2] Student\n  [3] Senior Citizen\n  [4] Daily Pass\n"
                         << "Choice: ";
                    int typeChoice;
                    if (readInt(typeChoice)) {
                        switch (typeChoice) {
                            case 2: cardType = "Student"; break;
                            case 3: cardType = "SeniorCitizen"; break;
                            case 4: cardType = "DailyPass"; break;
                            default: cardType = "Regular"; break;
                        }
                    }
                }

                CardInfoResult result = service.createOrRechargeCard(cardId, cardType, amount);
                cout << result.message;
                if (result.success) {
                    cout << " Type: " << result.cardType << ". Balance: " << result.balance << " INR.\n";
                } else {
                    cout << "\n";
                }
                break;
            }

            case 2: { // SEARCH ROUTE
                cout << "Enter Start Station Name (e.g., Vanaz): ";
                string startStr = readString();
                cout << "Enter Destination Station Name (e.g., Katraj): ";
                string endStr = readString();

                cout << "Select Routing Strategy:\n  [1] Fastest Time (Dijkstra, cached, includes interchange penalty)\n  [2] Fewest Stations (BFS)\nChoice: ";
                int strat;
                if (!readInt(strat) || (strat != 1 && strat != 2)) {
                    cout << "Invalid selection. Defaulting to Fastest Time.\n";
                    strat = 1;
                }

                cout << "\n[App] Calculating optimal route...\n";
                RouteSearchResult result = service.searchRoute(startStr, endStr, strat == 2 ? "fewest" : "fastest");

                if (!result.success) {
                    cout << result.message << "\n";
                } else {
                    cout << "\nPath:\n";
                    for (size_t i = 0; i < result.stationNames.size(); ++i) {
                        cout << result.stationNames[i];
                        if (i < result.stationNames.size() - 1) cout << " -> ";
                        if ((i + 1) % 4 == 0) cout << "\n";
                    }
                    if (result.isTimeMetric) cout << "\n\nEstimated Time: " << result.metric << " minutes (includes any interchange penalties).\n";
                    else cout << "\n\nStations Traversed: " << result.metric << " stops.\n";
                }
                break;
            }

            case 3: { // TAP IN
                cout << "Enter Card ID: ";
                string cardId = readString();
                cout << "Enter Station Name to Tap In: ";
                string stName = readString();

                TapOutcome outcome = service.tapIn(cardId, stName);
                cout << (outcome.success ? "[Turnstile] " : "[Turnstile] Denied: ") << outcome.message << "\n";
                break;
            }

            case 4: { // TAP OUT
                cout << "Enter Card ID: ";
                string cardId = readString();
                cout << "Enter Station Name to Tap Out: ";
                string stName = readString();

                TapOutcome outcome = service.tapOut(cardId, stName);
                cout << (outcome.success ? "[Turnstile] " : "[Turnstile] Denied: ") << outcome.message << "\n";
                break;
            }

            case 5: { // ADMIN OPERATIONS MENU
                bool adminBack = false;
                while (!adminBack) {
                    cout << "\n========== Metro Operations ==========\n";
                    cout << "1. Report Incident\n";
                    cout << "2. Remove Incident\n";
                    cout << "3. Close Track\n";
                    cout << "4. Reopen Track\n";
                    cout << "5. View Active Incidents\n";
                    cout << "6. Set Turnstile Out of Order\n";
                    cout << "7. Repair Turnstile\n";
                    cout << "8. View Turnstile Status\n";
                    cout << "0. Back to Main Menu\n";
                    cout << "Choice: ";

                    int adminChoice;
                    if (!readInt(adminChoice)) {
                        cout << "Invalid input.\n";
                        continue;
                    }

                    switch (adminChoice) {
                        case 0:
                            adminBack = true;
                            break;

                        case 1: { // REPORT INCIDENT
                            cout << "Start Station: "; string s1 = readString();
                            cout << "End Station: "; string s2 = readString();

                            cout << "Type (Maintenance, SignalFailure, TrainBreakdown, HeavyLoad, Emergency, Weather, Other): ";
                            string typeStr = readString();
                            cout << "Severity (Minor, Moderate, Major, Critical): ";
                            string severityStr = readString();
                            cout << "Duration (mins): ";
                            int d;
                            if (!readInt(d)) { cout << "Invalid duration.\n"; break; }
                            cout << "Reported by: "; string rep = readString();

                            IncidentActionResult result = service.reportIncident(s1, s2, typeStr, severityStr, d, rep);
                            cout << result.message << "\n";
                            break;
                        }

                        case 2: { // REMOVE INCIDENT
                            cout << "Enter Incident ID to remove: ";
                            int evId;
                            if (!readInt(evId)) { cout << "Invalid incident ID.\n"; break; }

                            SimpleResult result = service.removeIncident(evId);
                            cout << result.message << "\n";
                            break;
                        }

                        case 3: { // CLOSE TRACK
                            cout << "Start Station: "; string s1 = readString();
                            cout << "End Station: "; string s2 = readString();
                            cout << "Duration to close (mins): ";
                            int d;
                            if (!readInt(d)) { cout << "Invalid duration.\n"; break; }

                            IncidentActionResult result = service.closeTrack(s1, s2, d);
                            cout << result.message << "\n";
                            break;
                        }

                        case 4: { // REOPEN TRACK
                            cout << "Start Station: "; string s1 = readString();
                            cout << "End Station: "; string s2 = readString();

                            SimpleResult result = service.reopenTrack(s1, s2);
                            cout << result.message << "\n";
                            break;
                        }

                        case 5: { // VIEW INCIDENTS
                            auto incidents = service.getActiveIncidents();
                            cout << "\n--- ACTIVE INCIDENTS (" << incidents.size() << ") ---\n";
                            for (const auto& ev : incidents) {
                                cout << "[ID: " << ev.eventId << "] "
                                     << ev.fromStationName << " <-> " << ev.toStationName
                                     << " (" << ev.lineName << " Line)\n"
                                     << "    Type: " << ev.incidentType << " | Sev: " << ev.severity;
                                if (!ev.trackClosed) cout << " (+" << ev.delayMinutes << " mins)";
                                cout << "\n    Reporter: " << ev.reportedBy << "\n\n";
                            }
                            break;
                        }

                        case 6: {
                            SimpleResult result = service.setTurnstileOutOfOrder();
                            cout << result.message << "\n";
                            break;
                        }

                        case 7: {
                            SimpleResult result = service.repairTurnstile();
                            cout << result.message << "\n";
                            break;
                        }

                        case 8: {
                            cout << "Current Turnstile State: " << service.getTurnstileStatus() << "\n";
                            break;
                        }

                        default:
                            cout << "Invalid option.\n";
                    }
                }
                break;
            } // END CASE 5

            case 6: { // SUBSCRIBE TO LINE ALERTS
                cout << "Enter Card ID: ";
                string cardId = readString();
                cout << "Enter Line Name to subscribe to (e.g., Purple): ";
                string lineName = readString();

                SimpleResult result = service.subscribeToLine(cardId, lineName);
                cout << result.message << "\n";
                break;
            }

            default:
                cout << "Invalid choice. Try again.\n";
                break;
        } // END MAIN SWITCH
    } // END WHILE LOOP

    return 0;
}