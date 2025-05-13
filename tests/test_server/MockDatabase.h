#ifndef MOCK_DATABASE_H
#define MOCK_DATABASE_H

#include "gmock/gmock.h"
#include "../../server/database_interface.h"
#include <string>
#include <vector>
#include <tuple>
#include <set>

class MockDatabase : public IDatabase {
public:
    MOCK_METHOD(bool, connect, (const std::string& dbPath), (override));
    MOCK_METHOD(void, disconnect, (), (override));
    MOCK_METHOD(bool, execute, (const std::string& query), (override));
    MOCK_METHOD(bool, execute, (const std::string& query, const std::vector<std::any>& params), (override));
    MOCK_METHOD(DbResult, fetchAll, (const std::string& query), (override));
    MOCK_METHOD(DbResult, fetchAll, (const std::string& query, const std::vector<std::any>& params), (override));
    MOCK_METHOD(std::optional<DbRow>, fetchOne, (const std::string& query), (override));
    MOCK_METHOD(std::optional<DbRow>, fetchOne, (const std::string& query, const std::vector<std::any>& params), (override));
    MOCK_METHOD(std::string, lastError, (), (const, override));
};

#endif // MOCK_DATABASE_H
