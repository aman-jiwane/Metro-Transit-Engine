#  Pune Metro Transit Engine

A highly concurrent, enterprise-grade Low-Level Design (LLD) C++ system modeling the Pune Metro network. This project features a real-time graph routing engine, an event-driven incident management system, atomic wallet transactions backed by SQLite, and a dual CLI/REST API architecture.

##  Key Features

* **Advanced Graph Routing:** Implements the Strategy Pattern to allow commuters to switch between **Shortest Time** (Dijkstra's Algorithm - $O(E \log V)$) and **Fewest Interchanges** (Breadth-First Search - $O(V + E)$).
* **Thread-Safe Concurrency:** Utilizes C++17 `std::shared_mutex` (Reader-Writer locks) to safely resolve simultaneous commuter requests, route calculations, and administrative database updates.
* **ACID-Compliant Ticketing:** Integrates a normalized (3NF) SQLite database. Turnstile checkouts are protected by C++ RAII scope guards, ensuring atomic wallet deductions and trip logging with automated rollbacks on failure.
* **Event-Driven Delay System:** Employs the Observer Pattern to broadcast real-time network incidents (track closures, signal failures), dynamically invalidating cached routes and forcing on-the-fly path recalculation.
* **Turnstile State Machine:** Models physical gate hardware using the State Pattern (`Locked`, `Unlocked`, `OutOfOrder`), preventing unauthorized entry and gracefully handling tampered states.
* **Dual-Interface Architecture:** Features a completely decoupled Service Layer injected via Dependency Inversion. The system runs both an interactive terminal CLI and a REST API (via `cpp-httplib`) driving a modern Vanilla JS/CSS Glassmorphism frontend.

##  Tech Stack & Architecture

* **Language:** C++17
* **Database:** SQLite3 (Repository Pattern, Automated SQL Triggers)
* **Build System:** CMake (with FetchContent for dependency management)
* **Testing:** Catch2 (Mocked Repositories & In-Memory Databases)
* **API / Networking:** `cpp-httplib`, `nlohmann/json`
* **Frontend:** Vanilla JavaScript, HTML5, CSS3

###  Design Patterns Implemented
1. **Strategy:** Interchangeable routing algorithms.
2. **State:** Turnstile gate physical mechanics.
3. **Observer:** Real-time delay and track closure broadcasting.
4. **Repository:** Database abstraction (SQLite vs. In-Memory Catch2 Testing).
5. **Dependency Injection:** Injecting network and database singletons into service classes.
6. **Chain of Responsibility / Factory:** Dynamic fare calculation (Student, Senior, Daily Pass).

##  Getting Started

### Prerequisites
* **C++17 Compiler** (GCC/MinGW-w64 on Windows, Clang on macOS, GCC on Linux)
* **CMake** (v3.15+)
