#ifndef DATABASE_INTERFACE_H
#define DATABASE_INTERFACE_H

#include <string>
#include <optional>
#include <vector>
#include <map>
#include <any>

using DbRow = std::map<std::string, std::any>;
using DbResult = std::vector<DbRow>;

class IDatabase {
public:
    virtual ~IDatabase() = default;
    virtual bool connect(const std::string& dbPath) = 0;
    virtual void disconnect() = 0;
    virtual bool execute(const std::string& query) = 0;
    virtual bool execute(const std::string& query, const std::vector<std::any>& params) = 0;
    virtual DbResult fetchAll(const std::string& query) = 0;
    virtual DbResult fetchAll(const std::string& query, const std::vector<std::any>& params) = 0;
    virtual std::optional<DbRow> fetchOne(const std::string& query) = 0;
    virtual std::optional<DbRow> fetchOne(const std::string& query, const std::vector<std::any>& params) = 0;
    virtual std::string lastError() const = 0;

    // ДЛЯ ТЕСТОВ
    virtual bool initTables() = 0;
    virtual bool userExists(const std::string& username) = 0;
    virtual bool addUser(const std::string& username, const std::string& password) = 0;
    virtual bool verifyPassword(const std::string& username, const std::string& password) = 0;
    virtual long long getUserId(const std::string& username) = 0; // Assuming long long based on ChatLogicServer usage
    virtual std::vector<std::string> getAllUsernames() = 0;
    virtual std::vector<std::string> getAllGroupChatIds() = 0;
    virtual std::vector<std::string> getUserFriends(const std::string& username) = 0;
    virtual std::vector<std::pair<std::string, std::string>> getUserGroupChats(const std::string& username) = 0;
    virtual std::vector<std::tuple<std::string, std::string, std::string, std::string>> getOfflineMessages(const std::string& username) = 0;
};

#endif // DATABASE_INTERFACE_H