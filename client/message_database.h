#ifndef MESSAGE_DATABASE_H
#define MESSAGE_DATABASE_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QList>
#include "privatechat_model.h"

class MessageDatabase : public QObject
{
    Q_OBJECT

public:
    explicit MessageDatabase(QObject *parent = nullptr);
    ~MessageDatabase();
    
    bool init();
    bool storeMessage(const QString &currentUser, const QString &peerUser, const QString &sender, 
                     const QString &content, const QString &timestamp, bool isRead = false);
    QList<PrivateMessage> loadMessages(const QString &currentUser, const QString &peerUser);
    bool clearHistory(const QString &currentUser, const QString &peerUser);
    
    static MessageDatabase& instance();

private:
    QSqlDatabase db;
    bool createTables();
    bool isInitialized;
    void verifyMessageSaved(const QString &currentUser, const QString &peerUser,
                          const QString &sender, const QString &content, 
                          const QString &timestamp);
};

#endif // MESSAGE_DATABASE_H
