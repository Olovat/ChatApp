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

class Server : public QTcpServer
{
    Q_OBJECT
public:
    explicit Server();
    virtual ~Server();
    bool connectDB();
        // Авторизация и регистрация пользователя
    bool authenticateUser(const QString &username, const QString &password);
    bool registerUser(const QString &username, const QString &password);
    QString getUsernameBySocket(QTcpSocket *socket);

private:
    QTcpSocket *socket;
    QSqlDatabase srv_db;
    QVector<QTcpSocket*> Sockets; // чтобы записывать сокеты в вектор
    QByteArray Data; // данные передаваемые между сервером и клиентом
    void SendToCllient(QString str); // передает данные клиенту
    quint16 nextBlockSize;

    // Структура для хранения авторизованных пользователей
    struct AuthenticatedUser {
        QString username;
        QTcpSocket* socket;
    };
    QVector<AuthenticatedUser> authenticatedUsers;
    
    // Создание таблицы пользователей
    bool initUserTable();

public slots:
    void incomingConnection(qintptr socketDescriptor); // обработчик новых подключений
    void slotReadyRead(); // слот для сигнала; обработчик полученных от клиента сообщений
    void clientDisconnected(); // обработчик отключения клиента
};

#endif // SERVER_H
