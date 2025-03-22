#include "server.h"
#include <QCoreApplication>
#include <QDir>
#include <QSqlQuery>
#include <QSqlError>
#include <QDataStream>
#include <QDebug>
#include <QThread>  

Server::Server(){
    if(this->listen(QHostAddress::Any, 5402))
    {
        qDebug() << "Server started on port 5402";

        if (!connectDB()) {
            qDebug() << "Failed to connect to database";
        } else {
            qDebug() << "Database connection established";
        }
    }
    else
    {
        qDebug() << "Error starting server";
    }
    nextBlockSize = 0;
}

Server::~Server()
{
    for (QTcpSocket* socket : Sockets) {
        if (socket && socket->isOpen()) {
            socket->close();
            socket->deleteLater();
        }
    }
    Sockets.clear();

    if (srv_db.isOpen()) {
        srv_db.close();
    }

    qDebug() << "Server destroyed";
}

bool Server::initializeDatabase() {
    if (!initUserTable()) {
        qDebug() << "Failed to initialize user table.";
        return false;
    }
    if (!initMessageTable()) {
        qDebug() << "Failed to initialize message table.";
        return false;
    }
    if (!initHistoryTable()) {
        qDebug() << "Failed to initialize history table.";
        return false;
    }
    qDebug() << "Database initialized successfully.";
    return true;
}

QSqlDatabase& Server::getDatabase() {
    return srv_db;
}

void Server::incomingConnection(qintptr socketDescriptor){
    socket = new QTcpSocket;
    socket->setSocketDescriptor(socketDescriptor);
    connect(socket, &QTcpSocket::readyRead, this, &Server::slotReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &Server::clientDisconnected);

    Sockets.push_back(socket);
    qDebug() << "Client connected with descriptor" << socketDescriptor;
}

void Server::slotReadyRead()
{
    socket = (QTcpSocket*)sender();
    QDataStream in(socket);
    in.setVersion(QDataStream::Qt_6_2);

    if(in.status() == QDataStream::Ok){
        while(socket->bytesAvailable() > 0){
            if(nextBlockSize == 0){
                if(socket->bytesAvailable() < 2){
                    break;
                }
                in >> nextBlockSize;
            }
            if(socket->bytesAvailable() < nextBlockSize){
                break;
            }

            QString message;
            in >> message;
            nextBlockSize = 0;

            qDebug() << "Received message from client:" << message;

            if (message.startsWith("AUTH:")) {
                QStringList parts = message.split(":", Qt::SkipEmptyParts);
                if (parts.size() >= 3) {
                    QString username = parts[1];
                    QString password = parts[2];

                    bool success = authenticateUser(username, password);
                    QString response = success ? "AUTH_SUCCESS" : "AUTH_FAILED";

                    if (success) {
                        bool userExists = false;
                        for (const AuthenticatedUser& user : authenticatedUsers) {
                            if (user.username == username) {
                                userExists = true;
                                break;
                            }
                        }
                        if (!userExists) {
                            AuthenticatedUser user;
                            user.username = username;
                            user.socket = socket;
                            authenticatedUsers.append(user);

                            // Обновляем список пользователей для всех после успешной авторизации
                            broadcastUserList();
                            
                            // Отправляем историю сообщений после успешной авторизации
                            sendMessageHistory(socket);
                        }
                    }

                    Data.clear();
                    QDataStream out(&Data, QIODevice::WriteOnly);
                    out.setVersion(QDataStream::Qt_6_2);
                    out << quint16(0) << response;
                    out.device()->seek(0);
                    out << quint16(Data.size() - sizeof(quint16));
                    socket->write(Data);
                    qDebug() << "Sent response to client:" << response;
                }
            }
            else if (message.startsWith("REGISTER:")) {
                QStringList parts = message.split(":", Qt::SkipEmptyParts);
                if (parts.size() >= 3) {
                    QString username = parts[1];
                    QString password = parts[2];

                    bool success = registerUser(username, password);
                    QString response = success ? "REGISTER_SUCCESS" : "REGISTER_FAILED";

                    Data.clear();
                    QDataStream out(&Data, QIODevice::WriteOnly);
                    out.setVersion(QDataStream::Qt_6_2);
                    out << quint16(0) << response;
                    out.device()->seek(0);
                    out << quint16(Data.size() - sizeof(quint16));
                    socket->write(Data);
                    qDebug() << "Sent response to client:" << response;
                }
            }
            else if (message == "GET_USERLIST") {
                sendUserList(socket);
            }
            else if (message.startsWith("PRIVATE:")) {
                QStringList parts = message.split(":", Qt::SkipEmptyParts);
                if (parts.size() >= 3) {
                    QString recipientUsername = parts[1];

                    // Объединяем все части сообщения после второго двоеточия
                    // чтобы правильно обрабатывать случаи с несколькими двоеточиями в сообщении
                    QString privateMessage = parts.mid(2).join(":");

                    QString senderUsername = "Unknown";
                    for (const AuthenticatedUser& user : authenticatedUsers) {
                        if (user.socket == socket) {
                            senderUsername = user.username;
                            break;
                        }
                    }

                    // Логируем приватное сообщение
                    logMessage(senderUsername, recipientUsername, privateMessage);

                    QString formattedMessage = "PRIVATE:" + senderUsername + ":" + privateMessage;
                    bool sent = sendPrivateMessage(recipientUsername, formattedMessage);

                    if (sent) {
                        qDebug() << "Sent private message from" << senderUsername << "to" << recipientUsername << ":" << privateMessage;
                    } else {
                        // Отправляем сообщение об ошибке обратно отправителю
                        QString errorMessage = "ERROR:User " + recipientUsername + " is not available";
                        Data.clear();
                        QDataStream out(&Data, QIODevice::WriteOnly);
                        out.setVersion(QDataStream::Qt_6_2);
                        out << quint16(0) << errorMessage;
                        out.device()->seek(0);
                        out << quint16(Data.size() - sizeof(quint16));
                        socket->write(Data);
                        qDebug() << "Sent error message to" << senderUsername << ":" << errorMessage;
                    }
                }
            }
            else if (message.startsWith("GET_PRIVATE_HISTORY:")) {
                QString otherUser = message.mid(QString("GET_PRIVATE_HISTORY:").length());
                
                QString currentUser = "Unknown";
                for (const AuthenticatedUser& user : authenticatedUsers) {
                    if (user.socket == socket) {
                        currentUser = user.username;
                        break;
                    }
                }
                
                sendPrivateMessageHistory(socket, currentUser, otherUser);
            }
            else if (message != "GET_USERLIST") { // Фильтрация системного сообщения
                QString senderUsername = "Unknown";
                for (const AuthenticatedUser& user : authenticatedUsers) {
                    if (user.socket == socket) {
                        senderUsername = user.username;
                        break;
                    }
                }

                // Логируем общее сообщение
                logMessage(senderUsername, "", message);
                
                // Сохраняем сообщение в историю
                saveToHistory(senderUsername, message);

                QString formattedMessage = senderUsername + ": " + message;
                SendToCllient(formattedMessage);
                qDebug() << "Broadcasting message to all clients:" << formattedMessage;
            }
        }
    }
}

void Server::SendToCllient(QString str){
    Data.clear();
    QDataStream out(&Data, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_2);
    out << quint16(0) << str;
    out.device()->seek(0);
    out << quint16(Data.size() - sizeof(quint16));
    for(int i = 0; i < Sockets.size(); i++){
        Sockets[i]->write(Data);
        Sockets[i]->flush(); 
    }
    qDebug() << "Sent message to all clients:" << str;
}

bool Server::sendPrivateMessage(const QString &recipientUsername, const QString &message)
{
    for (const AuthenticatedUser& user : authenticatedUsers) {
        if (user.username == recipientUsername) {
            Data.clear();
            QDataStream out(&Data, QIODevice::WriteOnly);
            out.setVersion(QDataStream::Qt_6_2);
            out << quint16(0) << message;
            out.device()->seek(0);
            out << quint16(Data.size() - sizeof(quint16));
            user.socket->write(Data);
            user.socket->flush(); 
            qDebug() << "Sent private message to" << recipientUsername << ":" << message;
            return true;
        }
    }
    qDebug() << "User not found for private message:" << recipientUsername;
    return false;
}

// Новый метод для отправки списка пользователей всем клиентам
void Server::broadcastUserList()
{
    QStringList users;
    for (const AuthenticatedUser& user : authenticatedUsers) {
        users.append(user.username);
    }

    QString userList = "USERLIST:" + users.join(",");
    Data.clear();
    QDataStream out(&Data, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_2);
    out << quint16(0) << userList;
    out.device()->seek(0);
    out << quint16(Data.size() - sizeof(quint16));

    // Отправляем список всем авторизованным пользователям
    for (const AuthenticatedUser& user : authenticatedUsers) {
        user.socket->write(Data);
    }

    qDebug() << "Broadcast user list to all clients:" << userList;
}

// Метод для отправки списка пользователей конкретному клиенту
void Server::sendUserList(QTcpSocket* clientSocket)
{
    QStringList users;
    for (const AuthenticatedUser& user : authenticatedUsers) {
        users.append(user.username);
    }

    QString userList = "USERLIST:" + users.join(",");
    Data.clear();
    QDataStream out(&Data, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_2);
    out << quint16(0) << userList;
    out.device()->seek(0);
    out << quint16(Data.size() - sizeof(quint16));
    clientSocket->write(Data);

    qDebug() << "Sent user list to client:" << userList;
}

bool Server::connectDB()
{
    srv_db = QSqlDatabase::addDatabase("QSQLITE");

        QDir appDir(QCoreApplication::applicationDirPath());

        appDir.cdUp();
        appDir.cdUp();
        appDir.cdUp();

        QString dataPath = appDir.absolutePath() + "/data";
        QDir dataDir(dataPath);
        if (!dataDir.exists()) {
            dataDir.mkpath(".");
        }

        srv_db.setDatabaseName(dataPath + "/authorisation.db");

        if(!srv_db.open())
        {
            qDebug() << "Cannot open database: " << srv_db.lastError();
            return false;
        }

        qDebug() << "Connected to database at: " << dataPath + "/authorisation.db";

        if (!initUserTable()) {
            qDebug() << "Failed to initialize user table";
            return false;
        }

        if (!initMessageTable()) {
            qDebug() << "Failed to initialize message table";
            return false;
        }
    
        if (!initHistoryTable()) {
            qDebug() << "Failed to initialize history table";
            return false;
        }
    return true;
}

bool Server::initUserTable()
{
    QSqlQuery query(srv_db);
    return query.exec("CREATE TABLE IF NOT EXISTS userlist ("
                      "number INTEGER PRIMARY KEY AUTOINCREMENT,"
                      "name VARCHAR(20) UNIQUE, "
                      "pass VARCHAR(12), "
                      "xpos INTEGER DEFAULT 100, "
                      "ypos INTEGER DEFAULT 100, "
                      "width INTEGER DEFAULT 400, "
                      "length INTEGER DEFAULT 300)");
}

bool Server::initMessageTable()
{
    QSqlQuery query(srv_db);
    return query.exec("CREATE TABLE IF NOT EXISTS messages ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                      "sender VARCHAR(20), "
                      "recipient VARCHAR(20), "  // NULL для общих сообщений
                      "message TEXT, "
                      "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP)");
}

bool Server::initHistoryTable()
{
    QSqlQuery query(srv_db);
    return query.exec("CREATE TABLE IF NOT EXISTS history ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                      "sender VARCHAR(20), "
                      "message TEXT, "
                      "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP)");
}

bool Server::logMessage(const QString &sender, const QString &recipient, const QString &message)
{
    QSqlQuery query(srv_db);
    query.prepare("INSERT INTO messages (sender, recipient, message) VALUES (:sender, :recipient, :message)");
    query.bindValue(":sender", sender);
    query.bindValue(":recipient", recipient.isEmpty() ? QVariant() : recipient);
    query.bindValue(":message", message);

    bool success = query.exec();
    if (!success) {
        qDebug() << "Failed to log message:" << query.lastError().text();
    }
    return success;
}

bool Server::saveToHistory(const QString &sender, const QString &message)
{
    QSqlQuery query(srv_db);
    query.prepare("INSERT INTO history (sender, message) VALUES (:sender, :message)");
    query.bindValue(":sender", sender);
    query.bindValue(":message", message);

    bool success = query.exec();
    if (!success) {
        qDebug() << "Failed to save message to history:" << query.lastError().text();
    } else {
        qDebug() << "Message saved to history - sender:" << sender << "message:" << message;
    }
    return success;
}

void Server::sendMessageHistory(QTcpSocket* clientSocket)
{
    qDebug() << "Sending message history to client...";
    
    // 1.Настройка начала истории
    {
        Data.clear();
        QDataStream beginOut(&Data, QIODevice::WriteOnly);
        beginOut.setVersion(QDataStream::Qt_6_2);
        beginOut << quint16(0) << QString("HISTORY_CMD:BEGIN");
        beginOut.device()->seek(0);
        beginOut << quint16(Data.size() - sizeof(quint16));
        clientSocket->write(Data);
        clientSocket->flush();
        qDebug() << "Sent history begin marker";
        
        
        QThread::msleep(10);
    }
    
    // 2. Отправка всех сообщений истории
    QSqlQuery query(srv_db);
    query.exec("SELECT sender, message, timestamp FROM history ORDER BY timestamp");
    

    bool hasRecords = query.next();
    
    if (!hasRecords) {
        // Если истории нет выводим сообщение об этом
        qDebug() << "No history records found, sending empty history message";
        
        Data.clear();
        QDataStream msgOut(&Data, QIODevice::WriteOnly);
        msgOut.setVersion(QDataStream::Qt_6_2);
        QString noHistoryMsg = "HISTORY_MSG:0000-00-00 00:00:00|Система|История сообщений пуста";
        msgOut << quint16(0) << noHistoryMsg;
        msgOut.device()->seek(0);
        msgOut << quint16(Data.size() - sizeof(quint16));
        clientSocket->write(Data);
        clientSocket->flush();
        qDebug() << "Sent empty history message:" << noHistoryMsg;
        

        QThread::msleep(10);
    } else {
        // Отправка истории сообщений
        do {
            QString sender = query.value("sender").toString();
            QString message = query.value("message").toString();
            QString timestamp = query.value("timestamp").toString();
            
            QString historyMsg = QString("HISTORY_MSG:%1|%2|%3").arg(timestamp, sender, message);
            
            Data.clear();
            QDataStream msgOut(&Data, QIODevice::WriteOnly);
            msgOut.setVersion(QDataStream::Qt_6_2);
            msgOut << quint16(0) << historyMsg;
            msgOut.device()->seek(0);
            msgOut << quint16(Data.size() - sizeof(quint16));
            clientSocket->write(Data);
            clientSocket->flush();
            qDebug() << "Sent history message:" << historyMsg;
            
            
            QThread::msleep(10);
        } while (query.next());
    }
    
    // 3. Конец истории
    {
        QThread::msleep(10);
        
        Data.clear();
        QDataStream endOut(&Data, QIODevice::WriteOnly);
        endOut.setVersion(QDataStream::Qt_6_2);
        endOut << quint16(0) << QString("HISTORY_CMD:END");
        endOut.device()->seek(0);
        endOut << quint16(Data.size() - sizeof(quint16));
        clientSocket->write(Data);
        clientSocket->flush();
        qDebug() << "Sent history end marker";
    }
}

void Server::sendPrivateMessageHistory(QTcpSocket* clientSocket, const QString &user1, const QString &user2)
{
    qDebug() << "Sending private message history between" << user1 << "and" << user2;
    // Находим кто запрашивает историю по соединению
    QString requesterUsername = "Unknown";
    for (const AuthenticatedUser& user : authenticatedUsers) {
        if (user.socket == clientSocket) {
            requesterUsername = user.username;
            break;
        }
    }
    
    if (requesterUsername != user1 && requesterUsername != user2) {
        qDebug() << "Security issue: User" << requesterUsername 
                 << "tried to access chat history between" << user1 << "and" << user2;
        return;
    }
    
    // 1. Настройка начала истории
    {
        QString otherUser = (requesterUsername == user1) ? user2 : user1;
        Data.clear();
        QDataStream beginOut(&Data, QIODevice::WriteOnly);
        beginOut.setVersion(QDataStream::Qt_6_2);
        beginOut << quint16(0) << QString("PRIVATE_HISTORY_CMD:BEGIN:%1").arg(otherUser);
        beginOut.device()->seek(0);
        beginOut << quint16(Data.size() - sizeof(quint16));
        clientSocket->write(Data);
        clientSocket->flush();
        qDebug() << "Sent private history begin marker for" << otherUser;
        QThread::msleep(10);
    }
    
    // 2. Очередьт сообщений истории
    QSqlQuery query(srv_db);
    query.prepare("SELECT sender, recipient, message, timestamp FROM messages "
                 "WHERE ((sender = :user1 AND recipient = :user2) OR "
                 "(sender = :user2 AND recipient = :user1)) "
                 "ORDER BY timestamp");
    query.bindValue(":user1", user1);
    query.bindValue(":user2", user2);
    
    bool hasRecords = false;
    if (query.exec()) {
        hasRecords = query.next();
    } else {
        qDebug() << "Database error when retrieving private history:" << query.lastError().text();
    }
    
    if (!hasRecords) {
        // Если истории личных сообщений нет, выводим сообщение об этом
        Data.clear();
        QDataStream msgOut(&Data, QIODevice::WriteOnly);
        msgOut.setVersion(QDataStream::Qt_6_2);
        QString noHistoryMsg = "PRIVATE_HISTORY_MSG:0000-00-00 00:00:00|Система|История личных сообщений пуста";
        msgOut << quint16(0) << noHistoryMsg;
        msgOut.device()->seek(0);
        msgOut << quint16(Data.size() - sizeof(quint16));
        clientSocket->write(Data);
        clientSocket->flush();
        qDebug() << "Sent empty private history message";
        QThread::msleep(10);
    } else {
        // Вывод истории личных сообщений
        do {
            QString sender = query.value("sender").toString();
            QString recipient = query.value("recipient").toString();
            QString message = query.value("message").toString();
            QString timestamp = query.value("timestamp").toString();
            
            QString historyMsg = QString("PRIVATE_HISTORY_MSG:%1|%2|%3|%4").arg(
                timestamp, sender, recipient, message);
            
            Data.clear();
            QDataStream msgOut(&Data, QIODevice::WriteOnly);
            msgOut.setVersion(QDataStream::Qt_6_2);
            msgOut << quint16(0) << historyMsg;
            msgOut.device()->seek(0);
            msgOut << quint16(Data.size() - sizeof(quint16));
            clientSocket->write(Data);
            clientSocket->flush();
            qDebug() << "Sent private history message:" << historyMsg;
            QThread::msleep(10);
        } while (query.next());
    }
    
    // 3. Конец истории
    {
        Data.clear();
        QDataStream endOut(&Data, QIODevice::WriteOnly);
        endOut.setVersion(QDataStream::Qt_6_2);
        endOut << quint16(0) << QString("PRIVATE_HISTORY_CMD:END");
        endOut.device()->seek(0);
        endOut << quint16(Data.size() - sizeof(quint16));
        clientSocket->write(Data);
        clientSocket->flush();
        qDebug() << "Sent private history end marker";
    }
}

bool Server::authenticateUser(const QString &username, const QString &password)
{
    QSqlQuery query(srv_db);
    query.prepare("SELECT * FROM userlist WHERE name = :name");
    query.bindValue(":name", username);

    if (query.exec() && query.next()) {
        QString storedPassword = query.value("pass").toString();
        return (storedPassword == password);
    }
    return false;
}

bool Server::registerUser(const QString &username, const QString &password)
{
    QSqlQuery query(srv_db);
    query.prepare("INSERT INTO userlist (name, pass) VALUES (:name, :pass)");
    query.bindValue(":name", username);
    query.bindValue(":pass", password);

    return query.exec();
}

QString Server::getUsernameBySocket(QTcpSocket *socket)
{
    for (const AuthenticatedUser &user : authenticatedUsers) {
        if (user.socket == socket) {
            return user.username;
        }
    }
    return "Anonymous";
}

void Server::clientDisconnected()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    QString username = "Unknown";
    for (int i = 0; i < authenticatedUsers.size(); ++i) {
        if (authenticatedUsers[i].socket == socket) {
            username = authenticatedUsers[i].username;
            authenticatedUsers.removeAt(i);
            break;
        }
    }

    qDebug() << "User " << username << " disconnected";

    Sockets.removeOne(socket);

    // Обновляем список пользователей для всех после отключения пользователя
    broadcastUserList();
}
