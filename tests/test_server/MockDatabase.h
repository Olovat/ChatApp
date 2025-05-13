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

    // ТЕСТ
    MOCK_METHOD(bool, initTables, (), (override));
    MOCK_METHOD(bool, userExists, (const std::string& username), (override));
    MOCK_METHOD(bool, addUser, (const std::string& username, const std::string& password), (override));
    MOCK_METHOD(bool, verifyPassword, (const std::string& username, const std::string& password), (override));
    MOCK_METHOD(long long, getUserId, (const std::string& username), (override));
    MOCK_METHOD(std::vector<std::string>, getAllUsernames, (), (override));
    MOCK_METHOD(std::vector<std::string>, getAllGroupChatIds, (), (override));
    MOCK_METHOD(std::vector<std::string>, getUserFriends, (const std::string& username), (override));
    MOCK_METHOD(std::vector<std::pair<std::string, std::string>>, getUserGroupChats, (const std::string& username), (override));
    MOCK_METHOD(std::vector<std::tuple<std::string, std::string, std::string, std::string>>, getOfflineMessages, (const std::string& username), (override));
};

#endif // MOCK_DATABASE_H
