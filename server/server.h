#ifndef SERVER_H
#define SERVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QSqlDatabase>
#include <QSqlError> 
#include <QSqlQuery>
#include <QDir>      
#include <QCoreApplication> 
#include <QVector>
#include <QList>
#include <gtest/gtest.h>


class Server : public QTcpServer
{
    Q_OBJECT

public:
    explicit Server();
    virtual ~Server();
    bool connectDB();
    bool initializeDatabase();

    QSqlDatabase& getDatabase();

    bool testRegisterUser(const QString &username, const QString &password);
    bool testAuthenticateUser(const QString &username, const QString &password);
    void testSendToClient(const QString &str);
    bool testLogMessage(const QString &sender, const QString &recipient, const QString &message);
    bool testSaveToHistory(const QString &sender, const QString &message);

    void searchUsers(const QString &query, QTcpSocket* socket);
    bool addFriend(const QString &username, const QString &friendName);
    bool removeFriend(const QString &username, const QString &friendName);
    QStringList getUserFriends(const QString &username);
    bool isFriend(const QString &username, const QString &friendName);
    bool initFriendshipsTable();

private:
    QTcpSocket *socket;
    QSqlDatabase srv_db;
    QVector<QTcpSocket*> Sockets; // чтобы записывать сокеты в вектор
    QByteArray Data; // данные передаваемые между сервером и клиентом
    void SendToCllient(QString str); // передает данные клиенту
    quint16 nextBlockSize;
    
    // Авторизация и регистрация пользователя
    bool authenticateUser(const QString &username, const QString &password);
    bool registerUser(const QString &username, const QString &password);
    QString getUsernameBySocket(QTcpSocket *socket);
    
    // Структура для хранения авторизованных пользователей
    struct AuthenticatedUser {
        QString username;
        QTcpSocket* socket;
        bool isOnline; // Флаг онлайн-статуса
    };
    QList<AuthenticatedUser> authenticatedUsers;
    
    // Создание таблицы пользователей
    bool initUserTable();
    bool initMessageTable();  
    bool initHistoryTable();  // Новый метод для создания таблицы истории
    bool initGroupChatTables(); // Метод для создания таблиц групповых чатов
    bool initReadMessageTable(); // Метод для создания таблицы прочитанных сообщений
    bool logMessage(const QString &sender, const QString &recipient, const QString &message);
    bool saveToHistory(const QString &sender, const QString &message);  // Новый метод для сохранения в историю
    void sendMessageHistory(QTcpSocket* socket);  // Новый метод для отправки истории
    void sendPrivateMessageHistory(QTcpSocket* socket, const QString &user1, const QString &user2);  // Новый метод для отправки истории личных сообщений

    void broadcastUserList();

    void sendUserList(QTcpSocket* clientSocket);
    
    // Метод для хранения сообщений, отправленных оффлайн-пользователям
    bool storeOfflineMessage(const QString &sender, const QString &recipient, const QString &message);
    
    // Метод для отправки сохраненных оффлайн-сообщений
    void sendStoredOfflineMessages(const QString &username, QTcpSocket* socket);

    // Методы для работы с групповыми чатами
    bool createGroupChat(const QString &chatId, const QString &chatName, const QString &creator);
    bool addUserToGroupChat(const QString &chatId, const QString &username);
    bool removeUserFromGroupChat(const QString &chatId, const QString &username); // Новый метод для удаления пользователя

    void sendGroupChatInfo(const QString &chatId, QTcpSocket *socket);
    bool saveGroupChatMessage(const QString &chatId, const QString &sender, const QString &message);
    void sendGroupChatMessage(const QString &chatId, const QString &sender, const QString &message);
    void sendGroupChatHistory(const QString &chatId, QTcpSocket *socket);
    void sendUserGroupChats(const QString &username, QTcpSocket *socket);

    // Отправка информации о чате всем его участникам
    void broadcastGroupChatInfo(const QString &chatId);

    // Методы для отслеживания прочитанных сообщений
    bool updateLastReadMessage(const QString &username, const QString &chatPartner, qint64 messageId);
    qint64 getLastReadMessageId(const QString &username, const QString &chatPartner);
    int getUnreadMessageCount(const QString &username, const QString &chatPartner);
    bool markAllMessagesAsRead(const QString &username, const QString &chatPartner);
    void sendUnreadMessagesCount(QTcpSocket* socket, const QString &username, const QString &chatPartner);

public slots:
    void incomingConnection(qintptr socketDescriptor); // обработчик новых подключений
    void slotReadyRead(); // слот для сигнала; обработчик полученных от клиента сообщений
    void clientDisconnected(); // обработчик отключения клиента
    bool sendPrivateMessage(const QString &recipientUsername, const QString &message);
};

#endif // SERVER_H
