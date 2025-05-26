#ifndef QT_NETWORK_ADAPTER_H
#define QT_NETWORK_ADAPTER_H

#include "network_interface.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QObject>
#include <QMap>
#include <QDataStream>
#include <QHostAddress>
#include <memory>

class QtNetworkServerAdapter;

class QtNetworkClientAdapter : public QObject, public INetworkClient, public std::enable_shared_from_this<QtNetworkClientAdapter> {
    Q_OBJECT
public:
    QtNetworkClientAdapter(QTcpSocket* socket, QtNetworkServerAdapter* serverAdapter, QObject* parent = nullptr);
    ~QtNetworkClientAdapter() override;

    void sendMessage(const std::string& message) override;
    std::string getClientId() const override;
    bool isConnected() const override;
    void disconnectClient() override;

    QTcpSocket* getSocket() const { return m_socket; }

signals:
    void disconnectedInternal(std::shared_ptr<QtNetworkClientAdapter> client);
    void messageReceivedInternal(std::shared_ptr<QtNetworkClientAdapter> client, const std::string& message);

private slots:
    void onReadyRead();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError socketError);

private:
    QTcpSocket* m_socket;
    quint16 m_nextBlockSize;
    std::string m_clientId;
    QtNetworkServerAdapter* m_serverAdapter;
};

class QtNetworkServerAdapter : public QObject, public INetworkServer {
    Q_OBJECT
public:
    explicit QtNetworkServerAdapter(QObject* parent = nullptr);
    ~QtNetworkServerAdapter() override;

    bool start(int port) override;
    void stop() override;
    void broadcastMessage(const std::string& message) override;

    void setClientConnectedCallback(ClientConnectedCallback cb) override;
    void setClientDisconnectedCallback(ClientDisconnectedCallback cb) override;
    void setMessageReceivedCallback(MessageReceivedCallback cb) override;

    void removeClient(std::shared_ptr<QtNetworkClientAdapter> client);

private slots:
    void handleNewConnection();
    void onClientAdapterDisconnected(std::shared_ptr<QtNetworkClientAdapter> client);
    void onClientAdapterMessageReceived(std::shared_ptr<QtNetworkClientAdapter> client, const std::string& message);


private:
    QTcpServer m_tcpServer;
    QList<std::shared_ptr<QtNetworkClientAdapter>> m_clients;

    ClientConnectedCallback m_clientConnectedCb;
    ClientDisconnectedCallback m_clientDisconnectedCb;
    MessageReceivedCallback m_messageReceivedCb;
};

#endif // QT_NETWORK_ADAPTER_H