#include "message_database.h"
#include <QDir>
#include <QStandardPaths>
#include <QDebug>
#include <QSqlQuery>
#include <QSqlError>
#include <QApplication>

MessageDatabase::MessageDatabase(QObject *parent) : QObject(parent), isInitialized(false)
{
    // Подписываемся на сигнал завершения приложения для правильного закрытия базы
    connect(qApp, &QApplication::aboutToQuit, this, [this](){
        qDebug() << "Application is closing, explicitly committing transactions and closing DB";
        if (db.isOpen()) {
            // Явно коммитим все транзакции перед закрытием
            QSqlQuery query(db);
            query.exec("COMMIT");
            db.commit();
            db.close();
            qDebug() << "Database closed successfully";
        }
    });
}

MessageDatabase::~MessageDatabase()
{
    if (db.isOpen()) {
        db.close();
    }
}

MessageDatabase& MessageDatabase::instance()
{
    static MessageDatabase instance;
    if (!instance.isInitialized) {
        instance.init();
    }
    return instance;
}

bool MessageDatabase::init()
{
    if (isInitialized) {
        return true;
    }

    // Определяем путь к папке данных приложения
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(dataPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // Создаем или открываем базу данных
    db = QSqlDatabase::addDatabase("QSQLITE", "chathistory");
    db.setDatabaseName(dataPath + "/chat_history.db");

    if (!db.open()) {
        qDebug() << "Failed to open database:" << db.lastError().text();
        return false;
    }

    isInitialized = createTables();
    qDebug() << "Message database initialized: " << isInitialized;
    return isInitialized;
}

bool MessageDatabase::createTables()
{
    QSqlQuery query(db);
    bool success = query.exec(
        "CREATE TABLE IF NOT EXISTS private_messages ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "current_user TEXT NOT NULL,"
        "peer_user TEXT NOT NULL,"
        "sender TEXT NOT NULL,"
        "content TEXT NOT NULL,"
        "timestamp TEXT NOT NULL,"
        "is_read INTEGER NOT NULL DEFAULT 0,"
        "UNIQUE(current_user, peer_user, sender, timestamp, content) ON CONFLICT IGNORE"
        ")"
    );

    if (!success) {
        qDebug() << "Failed to create tables:" << query.lastError().text();
        return false;
    }

    return true;
}

bool MessageDatabase::storeMessage(const QString &currentUser, const QString &peerUser, 
                                 const QString &sender, const QString &content, 
                                 const QString &timestamp, bool isRead)
{
    if (!isInitialized && !init()) {
        qDebug() << "Failed to initialize database in storeMessage";
        return false;
    }

    // Начинаем транзакцию для ускорения вставки
    db.transaction();
    
    QSqlQuery query(db);
    query.prepare("INSERT INTO private_messages (current_user, peer_user, sender, content, timestamp, is_read) "
                  "VALUES (?, ?, ?, ?, ?, ?)");
    query.bindValue(0, currentUser);
    query.bindValue(1, peerUser);
    query.bindValue(2, sender);
    query.bindValue(3, content);
    query.bindValue(4, timestamp);
    query.bindValue(5, isRead ? 1 : 0);

    bool result = query.exec();
    if (!result) {
        qDebug() << "Failed to store message:" << query.lastError().text() 
                 << " for user:" << currentUser << ", peer:" << peerUser;
        db.rollback(); // Откатываем транзакцию при ошибке
    } else {
        // Явно завершаем транзакцию, чтобы убедиться, что данные записываются на диск
        bool commitSuccess = db.commit();
        qDebug() << "Successfully stored message for user:" << currentUser 
                 << ", peer:" << peerUser << ", timestamp:" << timestamp
                 << ", commit success:" << commitSuccess;
                 
        // Дополнительно проверяем, что сообщение действительно было сохранено
        verifyMessageSaved(currentUser, peerUser, sender, content, timestamp);
    }
    return result;
}

// Добавляем новый метод для проверки, что сообщение сохранено
void MessageDatabase::verifyMessageSaved(const QString &currentUser, const QString &peerUser,
                                       const QString &sender, const QString &content, 
                                       const QString &timestamp)
{
    QSqlQuery verifyQuery(db);
    verifyQuery.prepare("SELECT COUNT(*) FROM private_messages WHERE "
                      "current_user = ? AND peer_user = ? AND sender = ? AND content = ? AND timestamp = ?");
    verifyQuery.bindValue(0, currentUser);
    verifyQuery.bindValue(1, peerUser);
    verifyQuery.bindValue(2, sender);
    verifyQuery.bindValue(3, content);
    verifyQuery.bindValue(4, timestamp);
    
    if (verifyQuery.exec() && verifyQuery.next()) {
        int count = verifyQuery.value(0).toInt();
        if (count > 0) {
            qDebug() << "Verified: Message was successfully saved to database";
        } else {
            qDebug() << "WARNING: Message was NOT saved to database!";
        }
    } else {
        qDebug() << "Error during verification query:" << verifyQuery.lastError().text();
    }
}

QList<PrivateMessage> MessageDatabase::loadMessages(const QString &currentUser, const QString &peerUser)
{
    QList<PrivateMessage> messages;
    
    if (!isInitialized && !init()) {
        qDebug() << "Failed to initialize database in loadMessages";
        return messages;
    }

    // Загружаем сообщения из базы данных, где current_user это либо текущий пользователь,
    // либо собеседник, и peer_user это либо собеседник, либо текущий пользователь
    // Это позволяет получить сообщения в обоих направлениях
    QSqlQuery query(db);
    query.prepare(
        "SELECT sender, content, timestamp, is_read FROM private_messages "
        "WHERE (current_user = ? AND peer_user = ?) OR (current_user = ? AND peer_user = ?) "
        "ORDER BY timestamp ASC"
    );
    query.bindValue(0, currentUser);
    query.bindValue(1, peerUser);
    query.bindValue(2, peerUser);
    query.bindValue(3, currentUser);

    if (query.exec()) {
        qDebug() << "Executed query to load messages for user:" << currentUser 
                 << ", peer:" << peerUser;
        while (query.next()) {
            QString sender = query.value(0).toString();
            QString content = query.value(1).toString();
            QString timestamp = query.value(2).toString();
            bool isRead = query.value(3).toBool();
            
            messages.append(PrivateMessage(sender, content, timestamp, isRead));
            qDebug() << "Loaded message: sender=" << sender << ", content=" << content.left(20) + "...";
        }
        qDebug() << "Total messages loaded from DB:" << messages.size();
    } else {
        qDebug() << "Failed to load messages:" << query.lastError().text();
    }

    return messages;
}

bool MessageDatabase::clearHistory(const QString &currentUser, const QString &peerUser)
{
    if (!isInitialized && !init()) {
        return false;
    }

    QSqlQuery query(db);
    query.prepare(
        "DELETE FROM private_messages WHERE "
        "(current_user = ? AND peer_user = ?) OR (current_user = ? AND peer_user = ?)"
    );
    query.bindValue(0, currentUser);
    query.bindValue(1, peerUser);
    query.bindValue(2, peerUser);
    query.bindValue(3, currentUser);

    bool result = query.exec();
    if (!result) {
        qDebug() << "Failed to clear history:" << query.lastError().text();
    }
    return result;
}
