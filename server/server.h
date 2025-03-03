#ifndef SERVER_H
#define SERVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QSqlDatabase>
#include <QSqlError> 
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

private:
    QTcpSocket *socket;
    QSqlDatabase srv_db;
    QVector<QTcpSocket*> Sockets;//чтобы записывать сокеты в вектор
    QByteArray Data;// данные передаваемые между сервером и клиентом
    void SendToCllient(QString str);//передает данные клиенту
    quint16 nextBlockSize;

public slots:
    void incomingConnection(qintptr socketDescriptor);//обработчик новых подключений
    void slotReadyRead();//слот для сигнала; обработчик полученных от клиента сообщений
};

#endif // SERVER_H
