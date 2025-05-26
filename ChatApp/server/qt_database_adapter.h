#ifndef QT_DATABASE_ADAPTER_H
#define QT_DATABASE_ADAPTER_H

#include "database_interface.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QSqlRecord>

class QtDatabaseAdapter : public IDatabase {
public:
    QtDatabaseAdapter(const std::string& driver = "QSQLITE");
    ~QtDatabaseAdapter() override;

    bool connect(const std::string& dbPath) override;
    void disconnect() override;
    bool execute(const std::string& query) override;
    bool execute(const std::string& query, const std::vector<std::any>& params) override;
    DbResult fetchAll(const std::string& query) override;
    DbResult fetchAll(const std::string& query, const std::vector<std::any>& params) override;
    std::optional<DbRow> fetchOne(const std::string& query) override;
    std::optional<DbRow> fetchOne(const std::string& query, const std::vector<std::any>& params) override;
    std::string lastError() const override;
private:
    QSqlDatabase m_db;
    std::string m_lastErrorText;
    std::string m_driverName;

    QVariant convertToQVariant(const std::any& val);
    std::any convertFromQVariant(const QVariant& qVal);
};

#endif // QT_DATABASE_ADAPTER_H