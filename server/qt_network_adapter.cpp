#include "qt_network_adapter.h"
#include <QDebug> 
#include <QUuid>


QtNetworkClientAdapter::QtNetworkClientAdapter(QTcpSocket* socket, QtNetworkServerAdapter* serverAdapter, QObject* parent)
    : QObject(parent), m_socket(socket), m_nextBlockSize(0), m_serverAdapter(serverAdapter) {
    if (m_socket) {
        connect(m_socket, &QTcpSocket::readyRead, this, &QtNetworkClientAdapter::onReadyRead);
        connect(m_socket, &QTcpSocket::disconnected, this, &QtNetworkClientAdapter::onSocketDisconnected);
        connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred), this, &QtNetworkClientAdapter::onSocketError);
        // Генерируем ID клиента на основе его адреса и порта для простоты
        m_clientId = m_socket->peerAddress().toString().toStdString() + ":" + std::to_string(m_socket->peerPort());
        qDebug() << "QtNetworkClientAdapter created for" << QString::fromStdString(m_clientId);
    }
}

QtNetworkClientAdapter::~QtNetworkClientAdapter() {
    qDebug() << "QtNetworkClientAdapter for" << QString::fromStdString(m_clientId) << "destroyed.";
}

void QtNetworkClientAdapter::sendMessage(const std::string& message) {
    if (m_socket && m_socket->isOpen() && m_socket->state() == QAbstractSocket::ConnectedState) {
        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_6_2);
        out << quint16(0);
        out << QString::fromStdString(message);
        out.device()->seek(0);
        out << quint16(block.size() - sizeof(quint16));
        m_socket->write(block);
        m_socket->flush(); // Убедимся, что данные отправлены немедленно
        qDebug() << "Sent to" << QString::fromStdString(m_clientId) << ":" << QString::fromStdString(message);
    } else {
        qWarning() << "Cannot send message, socket not connected or invalid for client" << QString::fromStdString(m_clientId);
    }
}

std::string QtNetworkClientAdapter::getClientId() const {
    return m_clientId;
}

bool QtNetworkClientAdapter::isConnected() const {
    return m_socket && m_socket->isOpen() && m_socket->state() == QAbstractSocket::ConnectedState;
}

void QtNetworkClientAdapter::disconnectClient() {
    if (m_socket && m_socket->isOpen()) {
        qDebug() << "Disconnecting client" << QString::fromStdString(m_clientId);
        m_socket->disconnectFromHost();
    }
}

void QtNetworkClientAdapter::onReadyRead() {
    if (!m_socket) return;

    QDataStream in(m_socket);
    in.setVersion(QDataStream::Qt_6_2);

    forever { // Читаем все доступные сообщения
        if (m_nextBlockSize == 0) {
            if (m_socket->bytesAvailable() < sizeof(quint16)) {
                break; // Недостаточно данных для размера блока
            }
            in >> m_nextBlockSize;
        }

        if (m_socket->bytesAvailable() < m_nextBlockSize) {
            break; // Недостаточно данных для полного сообщения
        }

        QString messageStr;
        in >> messageStr;

        if (in.status() != QDataStream::Ok) {
            qWarning() << "DataStream error while reading for client" << QString::fromStdString(m_clientId) << "status:" << in.status();
            m_nextBlockSize = 0; // Сбрасываем, чтобы избежать зацикливания
            break;
        }

        qDebug() << "Received from" << QString::fromStdString(m_clientId) << ":" << messageStr;
        m_nextBlockSize = 0; // Сбрасываем для следующего сообщения

        // Передаем сообщение в QtNetworkServerAdapter, который вызовет callback ChatLogicServer
        emit messageReceivedInternal(shared_from_this(), messageStr.toStdString());
    }
}

void QtNetworkClientAdapter::onSocketDisconnected() {
    qDebug() << "Socket disconnected for client" << QString::fromStdString(m_clientId);
    emit disconnectedInternal(shared_from_this());
}

void QtNetworkClientAdapter::onSocketError(QAbstractSocket::SocketError socketError) {
    qWarning() << "Socket error for client" << QString::fromStdString(m_clientId) << ":" << m_socket->errorString();

}

QtNetworkServerAdapter::QtNetworkServerAdapter(QObject* parent) : QObject(parent) {
    connect(&m_tcpServer, &QTcpServer::newConnection, this, &QtNetworkServerAdapter::handleNewConnection);
    qDebug() << "QtNetworkServerAdapter created.";
}

QtNetworkServerAdapter::~QtNetworkServerAdapter() {
    stop();
    qDebug() << "QtNetworkServerAdapter destroyed.";
}

bool QtNetworkServerAdapter::start(int port) {
    if (m_tcpServer.listen(QHostAddress::Any, static_cast<quint16>(port))) {
        qDebug() << "QtNetworkServerAdapter started on port" << port;
        return true;
    } else {
        qWarning() << "QtNetworkServerAdapter failed to start on port" << port << ":" << m_tcpServer.errorString();
        return false;
    }
}

void QtNetworkServerAdapter::stop() {
    if (m_tcpServer.isListening()) {
        m_tcpServer.close();
        qDebug() << "QtNetworkServerAdapter stopped.";
    }
    // Закрываем все клиентские соединения
    for (const auto& client : qAsConst(m_clients)) {
        if (client) {
            client->disconnectClient();
        }
    }
    m_clients.clear(); // Очищаем список клиентов
}

void QtNetworkServerAdapter::broadcastMessage(const std::string& message) {
    qDebug() << "Broadcasting message:" << QString::fromStdString(message);
    for (const auto& client : qAsConst(m_clients)) {
        if (client && client->isConnected()) {
            client->sendMessage(message);
        }
    }
}

void QtNetworkServerAdapter::setClientConnectedCallback(ClientConnectedCallback cb) {
    m_clientConnectedCb = cb;
}

void QtNetworkServerAdapter::setClientDisconnectedCallback(ClientDisconnectedCallback cb) {
    m_clientDisconnectedCb = cb;
}

void QtNetworkServerAdapter::setMessageReceivedCallback(MessageReceivedCallback cb) {
    m_messageReceivedCb = cb;
}

void QtNetworkServerAdapter::handleNewConnection() {
    while (m_tcpServer.hasPendingConnections()) {
        QTcpSocket* socket = m_tcpServer.nextPendingConnection();
        if (socket) {
            qDebug() << "New connection from" << socket->peerAddress().toString() << ":" << socket->peerPort();
            auto clientAdapter = std::make_shared<QtNetworkClientAdapter>(socket, this);
            connect(clientAdapter.get(), &QtNetworkClientAdapter::disconnectedInternal, this, &QtNetworkServerAdapter::onClientAdapterDisconnected);
            connect(clientAdapter.get(), &QtNetworkClientAdapter::messageReceivedInternal, this, &QtNetworkServerAdapter::onClientAdapterMessageReceived);

            m_clients.append(clientAdapter);

            if (m_clientConnectedCb) {
                m_clientConnectedCb(clientAdapter);
            }
        }
    }
}

void QtNetworkServerAdapter::onClientAdapterDisconnected(std::shared_ptr<QtNetworkClientAdapter> client) {
    if (client) {
        qDebug() << "Client adapter disconnected event for" << QString::fromStdString(client->getClientId());
        if (m_clientDisconnectedCb) {
            m_clientDisconnectedCb(client);
        }
        // Удаляем клиента из списка
        removeClient(client);
    }
}

void QtNetworkServerAdapter::onClientAdapterMessageReceived(std::shared_ptr<QtNetworkClientAdapter> client, const std::string& message) {
    if (client && m_messageReceivedCb) {
        m_messageReceivedCb(client, message);
    }
}

void QtNetworkServerAdapter::removeClient(std::shared_ptr<QtNetworkClientAdapter> client) {
    if (!client) return;
    // Безопасное удаление из списка
    for (int i = 0; i < m_clients.size(); ++i) {
        if (m_clients.at(i) == client) {
            m_clients.removeAt(i);
            qDebug() << "Removed client" << QString::fromStdString(client->getClientId()) << "from list. Remaining clients:" << m_clients.size();
            break;
        }
    }
}
