#pragma once
#include "ICardRepository.h"

class Database;

class SqliteCardRepository : public ICardRepository {
private:
    Database& db;

public:
    explicit SqliteCardRepository(Database& database);

    bool exists(const std::string& cardId) override;
    void create(const std::string& cardId, CardType type, double initialBalance) override;
    bool find(const std::string& cardId, CardRecord& out) override;
    void recharge(const std::string& cardId, double amount) override;
    void setEntryStation(const std::string& cardId, int stationId) override;
};