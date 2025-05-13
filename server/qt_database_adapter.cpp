#include "qt_database_adapter.h"
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QVariant>
#include <QDebug>
#include <QDate>
#include <QDateTime>

QtDatabaseAdapter::QtDatabaseAdapter(const std::string& driver) : m_driverName(driver) {
    if (QSqlDatabase::isDriverAvailable(QString::fromStdString(m_driverName))) {
        m_db = QSqlDatabase::addDatabase(QString::fromStdString(m_driverName));
    } else {
        m_lastErrorText = "Driver " + m_driverName + " not available.";
        qWarning() << QString::fromStdString(m_lastErrorText);
    }
}

QtDatabaseAdapter::~QtDatabaseAdapter() {
    disconnect();
}

bool QtDatabaseAdapter::connect(const std::string& dbPath) {
    if (!m_db.isValid()) { // Flfgnth yt gjluhe;tyy
         qWarning() << "Database driver" << QString::fromStdString(m_driverName) << "is not valid or available.";
        return false;
    }
    m_db.setDatabaseName(QString::fromStdString(dbPath));
    if (!m_db.open()) {
        m_lastErrorText = m_db.lastError().text().toStdString();
        qWarning() << "Failed to connect to database:" << QString::fromStdString(m_lastErrorText);
        return false;
    }
    m_lastErrorText.clear();
    qDebug() << "Successfully connected to database:" << QString::fromStdString(dbPath);
    return true;
}

void QtDatabaseAdapter::disconnect() {
    if (m_db.isOpen()) {
        m_db.close();
        qDebug() << "Database disconnected.";
    }
}

bool QtDatabaseAdapter::execute(const std::string& query_str) {
    if (!m_db.isOpen()) {
        m_lastErrorText = "Database is not open.";
        qWarning() << QString::fromStdString(m_lastErrorText);
        return false;
    }
    QSqlQuery query(m_db);
    if (!query.exec(QString::fromStdString(query_str))) {
        m_lastErrorText = query.lastError().text().toStdString();
        qWarning() << "Execute failed:" << QString::fromStdString(m_lastErrorText) << "Query:" << QString::fromStdString(query_str);
        return false;
    }
    m_lastErrorText.clear();
    return true;
}

bool QtDatabaseAdapter::execute(const std::string& query_str, const std::vector<std::any>& params) {
    if (!m_db.isOpen()) {
        m_lastErrorText = "Database is not open.";
        qWarning() << QString::fromStdString(m_lastErrorText);
        return false;
    }
    QSqlQuery query(m_db);
    query.prepare(QString::fromStdString(query_str));
    for (const auto& param : params) {
        query.addBindValue(convertToQVariant(param));
    }
    if (!query.exec()) {
        m_lastErrorText = query.lastError().text().toStdString();
        qWarning() << "Execute with params failed:" << QString::fromStdString(m_lastErrorText) << "Query:" << QString::fromStdString(query_str);
        return false;
    }
    m_lastErrorText.clear();
    return true;
}

DbResult QtDatabaseAdapter::fetchAll(const std::string& query_str) {
    DbResult result_set;
    if (!m_db.isOpen()) {
        m_lastErrorText = "Database is not open.";
        qWarning() << QString::fromStdString(m_lastErrorText);
        return result_set;
    }
    QSqlQuery query(m_db);
    if (query.exec(QString::fromStdString(query_str))) {
        QSqlRecord record = query.record();
        while (query.next()) {
            DbRow row;
            for (int i = 0; i < record.count(); ++i) {
                row[record.fieldName(i).toStdString()] = convertFromQVariant(query.value(i));
            }
            result_set.push_back(row);
        }
        m_lastErrorText.clear();
    } else {
        m_lastErrorText = query.lastError().text().toStdString();
        qWarning() << "FetchAll failed:" << QString::fromStdString(m_lastErrorText) << "Query:" << QString::fromStdString(query_str);
    }
    return result_set;
}

DbResult QtDatabaseAdapter::fetchAll(const std::string& query_str, const std::vector<std::any>& params) {
    DbResult result_set;
    if (!m_db.isOpen()) {
        m_lastErrorText = "Database is not open.";
        qWarning() << QString::fromStdString(m_lastErrorText);
        return result_set;
    }
    QSqlQuery query(m_db);
    query.prepare(QString::fromStdString(query_str));
    for (const auto& param : params) {
        query.addBindValue(convertToQVariant(param));
    }
    if (query.exec()) {
        QSqlRecord record = query.record();
        while (query.next()) {
            DbRow row;
            for (int i = 0; i < record.count(); ++i) {
                row[record.fieldName(i).toStdString()] = convertFromQVariant(query.value(i));
            }
            result_set.push_back(row);
        }
        m_lastErrorText.clear();
    } else {
        m_lastErrorText = query.lastError().text().toStdString();
        qWarning() << "FetchAll with params failed:" << QString::fromStdString(m_lastErrorText) << "Query:" << QString::fromStdString(query_str);
    }
    return result_set;
}

std::optional<DbRow> QtDatabaseAdapter::fetchOne(const std::string& query_str) {
    if (!m_db.isOpen()) {
        m_lastErrorText = "Database is not open.";
        qWarning() << QString::fromStdString(m_lastErrorText);
        return std::nullopt;
    }
    QSqlQuery query(m_db);
    if (query.exec(QString::fromStdString(query_str))) {
        if (query.next()) {
            DbRow row;
            QSqlRecord record = query.record();
            for (int i = 0; i < record.count(); ++i) {
                row[record.fieldName(i).toStdString()] = convertFromQVariant(query.value(i));
            }
            m_lastErrorText.clear();
            return row;
        }
        m_lastErrorText.clear();
    } else {
        m_lastErrorText = query.lastError().text().toStdString();
        qWarning() << "FetchOne failed:" << QString::fromStdString(m_lastErrorText) << "Query:" << QString::fromStdString(query_str);
    }
    return std::nullopt;
}

std::optional<DbRow> QtDatabaseAdapter::fetchOne(const std::string& query_str, const std::vector<std::any>& params) {
    if (!m_db.isOpen()) {
        m_lastErrorText = "Database is not open.";
        qWarning() << QString::fromStdString(m_lastErrorText);
        return std::nullopt;
    }
    QSqlQuery query(m_db);
    query.prepare(QString::fromStdString(query_str));
    for (const auto& param : params) {
        query.addBindValue(convertToQVariant(param));
    }
    if (query.exec()) {
        if (query.next()) {
            DbRow row;
            QSqlRecord record = query.record();
            for (int i = 0; i < record.count(); ++i) {
                row[record.fieldName(i).toStdString()] = convertFromQVariant(query.value(i));
            }
            m_lastErrorText.clear();
            return row;
        }
        m_lastErrorText.clear();
    } else {
        m_lastErrorText = query.lastError().text().toStdString();
        qWarning() << "FetchOne with params failed:" << QString::fromStdString(m_lastErrorText) << "Query:" << QString::fromStdString(query_str);
    }
    return std::nullopt;
}

std::string QtDatabaseAdapter::lastError() const {
    return m_lastErrorText;
}

QVariant QtDatabaseAdapter::convertToQVariant(const std::any& val) {
    if (!val.has_value()) {
        return QVariant();
    }
    const auto& type = val.type();
    if (type == typeid(int)) {
        return QVariant(std::any_cast<int>(val));
    } else if (type == typeid(long)) {
        return QVariant(static_cast<qlonglong>(std::any_cast<long>(val)));
    } else if (type == typeid(long long)) {
        return QVariant(static_cast<qlonglong>(std::any_cast<long long>(val)));
    } else if (type == typeid(unsigned int)) {
        return QVariant(std::any_cast<unsigned int>(val));
    } else if (type == typeid(unsigned long)) {
        return QVariant(static_cast<qulonglong>(std::any_cast<unsigned long>(val)));
    } else if (type == typeid(unsigned long long)) {
        return QVariant(static_cast<qulonglong>(std::any_cast<unsigned long long>(val)));
    } else if (type == typeid(float)) {
        return QVariant(static_cast<double>(std::any_cast<float>(val)));
    } else if (type == typeid(double)) {
        return QVariant(std::any_cast<double>(val));
    } else if (type == typeid(bool)) {
        return QVariant(std::any_cast<bool>(val));
    } else if (type == typeid(std::string)) {
        return QVariant(QString::fromStdString(std::any_cast<std::string>(val)));
    } else if (type == typeid(const char*)) {
        return QVariant(QString::fromUtf8(std::any_cast<const char*>(val)));
    }
    qWarning() << "convertToQVariant: Unsupported std::any type:" << val.type().name();
    return QVariant();
}

std::any QtDatabaseAdapter::convertFromQVariant(const QVariant& qVal) {
    if (qVal.isNull() || !qVal.isValid()) {
        return std::any();
    }

    switch (static_cast<QMetaType::Type>(qVal.typeId())) {
        case QMetaType::Bool:
            return qVal.toBool();
        case QMetaType::Int:
            return qVal.toInt();
        case QMetaType::UInt:
            return qVal.toUInt();
        case QMetaType::LongLong:
            return qVal.toLongLong();
        case QMetaType::ULongLong:
            return qVal.toULongLong();
        case QMetaType::Float:
            return qVal.toFloat();
        case QMetaType::Double:
            return qVal.toDouble();
        case QMetaType::QString:
            return qVal.toString().toStdString();
        case QMetaType::QByteArray:
            return qVal.toByteArray().toStdString();
        case QMetaType::QDate:
            return qVal.toDate().toString(Qt::ISODate).toStdString();
        case QMetaType::QTime:
            return qVal.toTime().toString(Qt::ISODate).toStdString();
        case QMetaType::QDateTime:
            return qVal.toDateTime().toString(Qt::ISODate).toStdString();
        default:
            qWarning() << "convertFromQVariant: Unsupported QVariant type:" << qVal.typeName() << "Value:" << qVal.toString();
            if (qVal.canConvert<QString>()) {
                return qVal.toString().toStdString();
            }
            return std::any();
    }
}