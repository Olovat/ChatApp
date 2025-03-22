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
    FRIEND_TEST(ServerTest, RegisterUser);
    FRIEND_TEST(ServerTest, AuthenticateUser);
    FRIEND_TEST(ServerTest, SendToClient);
    FRIEND_TEST(ServerTest, LogMessage);
    FRIEND_TEST(ServerTest, SaveToHistory);
public:
    explicit Server();
    virtual ~Server();
    bool connectDB();
    bool initializeDatabase();
     QSqlDatabase& getDatabase();
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
    };
    QList<AuthenticatedUser> authenticatedUsers;
    
    // Создание таблицы пользователей
    bool initUserTable();
    bool initMessageTable();  
    bool initHistoryTable();  // Новый метод для создания таблицы истории
    bool logMessage(const QString &sender, const QString &recipient, const QString &message);
    bool saveToHistory(const QString &sender, const QString &message);  // Новый метод для сохранения в историю
    void sendMessageHistory(QTcpSocket* socket);  // Новый метод для отправки истории
    void sendPrivateMessageHistory(QTcpSocket* socket, const QString &user1, const QString &user2);  // Новый метод для отправки истории личных сообщений

    void broadcastUserList();

    void sendUserList(QTcpSocket* clientSocket);

public slots:
    void incomingConnection(qintptr socketDescriptor); // обработчик новых подключений
    void slotReadyRead(); // слот для сигнала; обработчик полученных от клиента сообщений
    void clientDisconnected(); // обработчик отключения клиента
    bool sendPrivateMessage(const QString &recipientUsername, const QString &message);

};

#endif // SERVER_H
