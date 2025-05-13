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
};

#endif // DATABASE_INTERFACE_H