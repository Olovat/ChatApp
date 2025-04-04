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
        qint64 bytesAvailable = socket->bytesAvailable();
        if (bytesAvailable == 0) return;
        
        qDebug() << "Processing data with" << bytesAvailable << "bytes available";
        
        while(socket->bytesAvailable() > 0){
            if(nextBlockSize == 0){
                if(socket->bytesAvailable() < 2){
                    break;
                }
                in >> nextBlockSize;
            }
            
            if(socket->bytesAvailable() < nextBlockSize){
                qDebug() << "Not enough data available";
                break;
            }
            
            QString message;
            in >> message;
            
            // Проверка на ошибки при чтении данных
            if (in.status() != QDataStream::Ok) {
                qDebug() << "Warning: Error reading data stream, status:" << in.status();
                nextBlockSize = 0;
                
                // Отправка ошибки клиенту
                Data.clear();
                QDataStream out(&Data, QIODevice::WriteOnly);
                out.setVersion(QDataStream::Qt_6_2);
                out << quint16(0) << QString("ERROR:Failed to process request");
                out.device()->seek(0);
                out << quint16(Data.size() - sizeof(quint16));
                socket->write(Data);
                break;
            }

            qDebug() << "Processing message, size:" << nextBlockSize << "Message:" << message;
            
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
                        int userIndex = -1;
                        
                        // Проверяем, существует ли пользователь в списке
                        for (int i = 0; i < authenticatedUsers.size(); ++i) {
                            if (authenticatedUsers[i].username == username) {
                                userExists = true;
                                userIndex = i;
                                break;
                            }
                        }
                        
                        if (userExists) {
                            // Если пользователь уже существует, обновляем его статус и сокет
                            authenticatedUsers[userIndex].isOnline = true;
                            authenticatedUsers[userIndex].socket = socket;
                            
                            // Отправляем сохраненные оффлайн-сообщения
                            sendStoredOfflineMessages(username, socket);
                        } else {
                            // Если пользователь новый, добавляем его
                            AuthenticatedUser user;
                            user.username = username;
                            user.socket = socket;
                            user.isOnline = true;
                            authenticatedUsers.append(user);
                        }

                        // Обновляем список пользователей для всех после успешной авторизации
                        broadcastUserList();
                        
                        // Отправляем историю сообщений после успешной авторизации
                        sendMessageHistory(socket);
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
            else if (message.startsWith("CREATE_GROUP_CHAT:")) {
                QStringList parts = message.split(":", Qt::SkipEmptyParts);
                if (parts.size() >= 3) {
                    QString chatName = parts[1];
                    QString chatId = parts[2];
                    
                    // Определяем, кто создал чат
                    QString creatorUsername = "Unknown";
                    for (const AuthenticatedUser& user : authenticatedUsers) {
                        if (user.socket == socket) {
                            creatorUsername = user.username;
                            break;
                        }
                    }
                    
                    bool success = createGroupChat(chatId, chatName, creatorUsername);
                    if (success) {
                        // Добавляем создателя как участника
                        addUserToGroupChat(chatId, creatorUsername);
                        
                        // Отправляем подтверждение создания
                        QString response = "GROUP_CHAT_CREATED:" + chatId + ":" + chatName;
                        Data.clear();
                        QDataStream out(&Data, QIODevice::WriteOnly);
                        out.setVersion(QDataStream::Qt_6_2);
                        out << quint16(0) << response;
                        out.device()->seek(0);
                        out << quint16(Data.size() - sizeof(quint16));
                        socket->write(Data);
                        
                        qDebug() << "Group chat created:" << chatName << "with ID:" << chatId << "by user:" << creatorUsername;
                    } else {
                        QString response = "ERROR:Failed to create group chat";
                        Data.clear();
                        QDataStream out(&Data, QIODevice::WriteOnly);
                        out.setVersion(QDataStream::Qt_6_2);
                        out << quint16(0) << response;
                        out.device()->seek(0);
                        out << quint16(Data.size() - sizeof(quint16));
                        socket->write(Data);
                        
                        qDebug() << "Failed to create group chat:" << chatName;
                    }
                }
            }
            else if (message.startsWith("JOIN_GROUP_CHAT:")) {
                QStringList parts = message.split(":", Qt::SkipEmptyParts);
                if (parts.size() >= 2) {
                    QString chatId = parts[1];
                    
                    QString username = "Unknown";
                    for (const AuthenticatedUser& user : authenticatedUsers) {
                        if (user.socket == socket) {
                            username = user.username;
                            break;
                        }
                    }
                    

                    // Проверяем, является ли пользователь уже участником чата
                    QSqlQuery checkMemberQuery(srv_db);
                    checkMemberQuery.prepare("SELECT 1 FROM group_chat_members WHERE chat_id = :chat_id AND username = :username");
                    checkMemberQuery.bindValue(":chat_id", chatId);
                    checkMemberQuery.bindValue(":username", username);
                    
                    bool isExistingMember = checkMemberQuery.exec() && checkMemberQuery.next();

                    bool success = addUserToGroupChat(chatId, username);
                    if (success) {
                        // Отправляем информацию о чате
                        sendGroupChatInfo(chatId, socket);
                        
                        // Отправляем историю сообщений чата
                        sendGroupChatHistory(chatId, socket);
                        
                        // Оповещаем всех участников о новом пользователе только если он действительно новый
                        if (!isExistingMember) {
                            sendGroupChatMessage(chatId, "SYSTEM", username + " присоединился к чату");
                        }

                    } else {
                        QString response = "ERROR:Failed to join group chat";
                        Data.clear();
                        QDataStream out(&Data, QIODevice::WriteOnly);
                        out.setVersion(QDataStream::Qt_6_2);
                        out << quint16(0) << response;
                        out.device()->seek(0);
                        out << quint16(Data.size() - sizeof(quint16));
                        socket->write(Data);
                    }
                }
            }
            else if (message.startsWith("GROUP_MESSAGE:")) {
                QStringList parts = message.split(":", Qt::SkipEmptyParts);
                if (parts.size() >= 3) {
                    QString chatId = parts[1];
                    QString messageText = parts.mid(2).join(":");
                    
                    QString senderUsername = "Unknown";
                    for (const AuthenticatedUser& user : authenticatedUsers) {
                        if (user.socket == socket) {
                            senderUsername = user.username;
                            break;
                        }
                    }
                    
                    // Сохраняем и рассылаем сообщение всем участникам группы
                    saveGroupChatMessage(chatId, senderUsername, messageText);
                    sendGroupChatMessage(chatId, senderUsername, messageText);
                }
            }

            else if (message.startsWith("GROUP_ADD_USER:")) {
                QStringList parts = message.split(":", Qt::SkipEmptyParts);
                if (parts.size() >= 3) {
                    QString chatId = parts[1];
                    QString userToAdd = parts[2];
                    
                    // Находим имя пользователя, который отправил запрос
                    QString senderUsername = "Unknown";
                    for (const AuthenticatedUser& user : authenticatedUsers) {
                        if (user.socket == socket) {
                            senderUsername = user.username;
                            break;
                        }
                    }
                    
                    // Добавляем пользователя в групповой чат
                    bool success = addUserToGroupChat(chatId, userToAdd);
                    if (success) {
                        // Оповещаем всех участников о новом пользователе
                        sendGroupChatMessage(chatId, "SYSTEM", userToAdd + " добавлен в чат пользователем " + senderUsername);
                        
                        // Отправляем обновленную информацию о чате
                        sendGroupChatInfo(chatId, socket);
                        
                        // Обновляем список пользователей для всех
                        broadcastUserList();
                    }
                }
            }
            else if (message.startsWith("GROUP_REMOVE_USER:")) {
                QStringList parts = message.split(":", Qt::SkipEmptyParts);
                if (parts.size() >= 3) {
                    QString chatId = parts[1];
                    QString userToRemove = parts[2];
                    
                    // Находим имя пользователя, который отправил запрос
                    QString senderUsername = "Unknown";
                    for (const AuthenticatedUser& user : authenticatedUsers) {
                        if (user.socket == socket) {
                            senderUsername = user.username;
                            break;
                        }
                    }
                    
                    // Удаляем пользователя из группового чата
                    bool success = removeUserFromGroupChat(chatId, userToRemove);
                    if (success) {
                        // Оповещаем всех участников об удалении пользователя
                        sendGroupChatMessage(chatId, "SYSTEM", userToRemove + " удален из чата пользователем " + senderUsername);
                        
                        // Отправляем обновленную информацию о чате всем оставшимся участникам
                        QSqlQuery membersQuery(srv_db);
                        membersQuery.prepare("SELECT username FROM group_chat_members WHERE chat_id = :chat_id");
                        membersQuery.bindValue(":chat_id", chatId);
                        
                        if (membersQuery.exec()) {
                            while (membersQuery.next()) {
                                QString memberUsername = membersQuery.value("username").toString();
                                
                                // Находим сокет участника
                                for (const AuthenticatedUser& user : authenticatedUsers) {
                                    if (user.username == memberUsername && user.isOnline && user.socket) {
                                        sendGroupChatInfo(chatId, user.socket);
                                    }
                                }
                            }
                        }
                        
                        // Обновляем список пользователей для всех
                        broadcastUserList();
                    }
                }
            }

            else if (message.startsWith("GET_GROUP_CHATS")) {
                // Отправка списка групповых чатов пользователю
                QString username = "Unknown";
                for (const AuthenticatedUser& user : authenticatedUsers) {
                    if (user.socket == socket) {
                        username = user.username;
                        break;
                    }
                }
                
                sendUserGroupChats(username, socket);
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
            if (user.isOnline && user.socket) {
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
            } else {
                // Пользователь оффлайн, сохраняем сообщение для последующей отправки
                QString sender = message.split(":")[1]; // Извлекаем отправителя из формата "PRIVATE:sender:message"
                QString content = message.mid(message.indexOf(":", message.indexOf(":") + 1) + 1); // Извлекаем текст сообщения
                
                storeOfflineMessage(sender, recipientUsername, content);
                qDebug() << "User" << recipientUsername << "is offline, message stored";
                return true; // Возвращаем true, так как сообщение успешно сохранено
            }
        }
    }
    qDebug() << "User not found for private message:" << recipientUsername;
    return false;
}

// Новый метод для отправки списка пользователей всем клиентам
void Server::broadcastUserList()
{
    // Для каждого онлайн-пользователя отправляем персонализированный список
    for (const AuthenticatedUser& currentUser : authenticatedUsers) {
        if (currentUser.isOnline && currentUser.socket) {
            // Отправляем персонализированный список для этого пользователя
            sendUserList(currentUser.socket);
        }
    }

    qDebug() << "Broadcast user lists to all online clients.";
}

// Метод для отправки списка пользователей конкретному клиенту
void Server::sendUserList(QTcpSocket* clientSocket)
{
    // Определим текущего пользователя по сокету
    QString currentUsername;
    for (const AuthenticatedUser& user : authenticatedUsers) {
        if (user.socket == clientSocket) {
            currentUsername = user.username;
            break;
        }
    }

    // Список для хранения пользователей
    QStringList users;
    for (const AuthenticatedUser& user : authenticatedUsers) {
        // Формат для пользователей: username:status:type (1 - онлайн, 0 - оффлайн, тип U - пользователь)
        users.append(user.username + ":" + (user.isOnline ? "1" : "0") + ":U");
    }

    // Добавляем групповые чаты, в которых состоит текущий пользователь
    if (!currentUsername.isEmpty()) {
        QSqlQuery query(srv_db);
        query.prepare("SELECT g.id, g.name FROM group_chats g "
                    "INNER JOIN group_chat_members m ON g.id = m.chat_id "
                    "WHERE m.username = :username");
        query.bindValue(":username", currentUsername);
        
        if (query.exec()) {
            while (query.next()) {
                QString chatId = query.value("id").toString();
                QString chatName = query.value("name").toString();
                // Формат для групповых чатов: chatId:1:G:chatName (всегда онлайн, тип G - группа)
                users.append(chatId + ":1:G:" + chatName);
            }
        } else {
            qDebug() << "Failed to get group chats for user:" << query.lastError().text();
        }
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

    if (!initGroupChatTables()) {
        qDebug() << "Failed to initialize group chat tables";
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

bool Server::initGroupChatTables()
{
    QSqlQuery query(srv_db);
    bool success = query.exec("CREATE TABLE IF NOT EXISTS group_chats ("
                             "id TEXT PRIMARY KEY, "  // UUID в виде строки
                             "name TEXT NOT NULL, "
                             "created_by VARCHAR(20), "
                             "created_at DATETIME DEFAULT CURRENT_TIMESTAMP)");
    
    if (!success) {
        qDebug() << "Failed to create group_chats table:" << query.lastError().text();
        return false;
    }
    
    success = query.exec("CREATE TABLE IF NOT EXISTS group_chat_members ("
                        "chat_id TEXT, "
                        "username VARCHAR(20), "
                        "joined_at DATETIME DEFAULT CURRENT_TIMESTAMP, "
                        "PRIMARY KEY (chat_id, username))");
    
    if (!success) {
        qDebug() << "Failed to create group_chat_members table:" << query.lastError().text();
        return false;
    }
    
    success = query.exec("CREATE TABLE IF NOT EXISTS group_chat_messages ("
                        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                        "chat_id TEXT, "
                        "sender VARCHAR(20), "
                        "message TEXT, "
                        "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP)");
    
    if (!success) {
        qDebug() << "Failed to create group_chat_messages table:" << query.lastError().text();
    }
    
    return success;
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
    
    // 2. Отправка сообщений истории - увеличим период до 7 дней вместо 2
    QSqlQuery query(srv_db);
    // Измененный запрос для отображения большей истории
    query.prepare("SELECT sender, message, timestamp FROM history WHERE timestamp >= datetime('now','-7 days') ORDER BY timestamp");
    
    bool hasRecords = query.exec() && query.next();
    
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

bool Server::testRegisterUser(const QString &username, const QString &password) {
    return registerUser(username, password);
}

bool Server::testAuthenticateUser(const QString &username, const QString &password) {
    return authenticateUser(username, password);
}

void Server::testSendToClient(const QString &str) {
    SendToCllient(str);
}

bool Server::testLogMessage(const QString &sender, const QString &recipient, const QString &message) {
    return logMessage(sender, recipient, message);
}

bool Server::testSaveToHistory(const QString &sender, const QString &message) {
    return saveToHistory(sender, message);
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
    
    // 2. Очередь сообщений истории - увеличим период до 7 дней вместо 2
    QSqlQuery query(srv_db);
    query.prepare("SELECT sender, recipient, message, timestamp FROM messages "
                 "WHERE ((sender = :user1 AND recipient = :user2) OR "
                 "(sender = :user2 AND recipient = :user1)) "
                 "AND timestamp >= datetime('now','-7 days') "
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
            // Не удаляем пользователя, а только помечаем его как оффлайн
            authenticatedUsers[i].isOnline = false;
            authenticatedUsers[i].socket = nullptr; // Очищаем указатель на сокет
            break;
        }
    }

    qDebug() << "User " << username << " disconnected (marked as offline)";

    Sockets.removeOne(socket);

    // Обновляем список пользователей для всех после отключения пользователя
    broadcastUserList();
}

// Метод для хранения сообщений, отправленных оффлайн-пользователям
void Server::storeOfflineMessage(const QString &sender, const QString &recipient, const QString &message)
{
    QSqlQuery query(srv_db);
    query.prepare("INSERT INTO messages (sender, recipient, message) VALUES (:sender, :recipient, :message)");
    query.bindValue(":sender", sender);
    query.bindValue(":recipient", recipient);
    query.bindValue(":message", message);
    
    if (!query.exec()) {
        qDebug() << "Failed to store offline message:" << query.lastError().text();
    } else {
        qDebug() << "Offline message stored for" << recipient << "from" << sender;
    }
}

// Метод для отправки сохраненных оффлайн-сообщений
void Server::sendStoredOfflineMessages(const QString &username, QTcpSocket* socket)
{
    QSqlQuery query(srv_db);
    query.prepare("SELECT sender, message, timestamp FROM messages WHERE recipient = :username AND recipient <> ''");
    query.bindValue(":username", username);
    
    if (!query.exec()) {
        qDebug() << "Failed to retrieve offline messages:" << query.lastError().text();
        return;
    }
    
    while (query.next()) {
        QString sender = query.value("sender").toString();
        QString message = query.value("message").toString();
        
        // Формируем сообщение в правильном формате и отправляем
        QString privateMessage = "PRIVATE:" + sender + ":" + message;
        
        Data.clear();
        QDataStream out(&Data, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_6_2);
        out << quint16(0) << privateMessage;
        out.device()->seek(0);
        out << quint16(Data.size() - sizeof(quint16));
        socket->write(Data);
        
        qDebug() << "Sent stored offline message to" << username << "from" << sender << ":" << message;
    }
}

bool Server::createGroupChat(const QString &chatId, const QString &chatName, const QString &creator)
{
    QSqlQuery query(srv_db);
    query.prepare("INSERT INTO group_chats (id, name, created_by) VALUES (:id, :name, :creator)");
    query.bindValue(":id", chatId);
    query.bindValue(":name", chatName);
    query.bindValue(":creator", creator);
    
    bool success = query.exec();
    if (!success) {
        qDebug() << "Failed to create group chat:" << query.lastError().text();
    }
    return success;
}

bool Server::addUserToGroupChat(const QString &chatId, const QString &username)
{
    // Проверяем, есть ли уже пользователь в этом чате
    QSqlQuery checkQuery(srv_db);
    checkQuery.prepare("SELECT 1 FROM group_chat_members WHERE chat_id = :chat_id AND username = :username");
    checkQuery.bindValue(":chat_id", chatId);
    checkQuery.bindValue(":username", username);
    
    if (checkQuery.exec() && checkQuery.next()) {
        // Пользователь уже в чате
        return true;
    }
    
    // Добавляем пользователя в чат
    QSqlQuery query(srv_db);
    query.prepare("INSERT INTO group_chat_members (chat_id, username) VALUES (:chat_id, :username)");
    query.bindValue(":chat_id", chatId);
    query.bindValue(":username", username);
    
    bool success = query.exec();
    if (!success) {
        qDebug() << "Failed to add user to group chat:" << query.lastError().text();
    }
    return success;
}

void Server::sendGroupChatInfo(const QString &chatId, QTcpSocket *socket)
{
    // Получаем информацию о чате
    QSqlQuery chatQuery(srv_db);
    chatQuery.prepare("SELECT name, created_by FROM group_chats WHERE id = :id");

    chatQuery.bindValue(":id", chatId);
    
    if (chatQuery.exec() && chatQuery.next()) {
        QString chatName = chatQuery.value("name").toString();
        QString creator = chatQuery.value("created_by").toString();

        
        // Получаем список участников
        QStringList members;
        QSqlQuery membersQuery(srv_db);
        membersQuery.prepare("SELECT username FROM group_chat_members WHERE chat_id = :chat_id");
        membersQuery.bindValue(":chat_id", chatId);
        
        if (membersQuery.exec()) {
            while (membersQuery.next()) {
                members.append(membersQuery.value("username").toString());
            }
        }
        
        // Формируем и отправляем ответ
        QString response = "GROUP_CHAT_INFO:" + chatId + ":" + chatName + ":" + members.join(",");
        
        Data.clear();
        QDataStream out(&Data, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_6_2);
        out << quint16(0) << response;
        out.device()->seek(0);
        out << quint16(Data.size() - sizeof(quint16));
        socket->write(Data);
        
        // Отправляем информацию о создателе в отдельном сообщении

    }
}

bool Server::saveGroupChatMessage(const QString &chatId, const QString &sender, const QString &message)
{
    QSqlQuery query(srv_db);
    query.prepare("INSERT INTO group_chat_messages (chat_id, sender, message) VALUES (:chat_id, :sender, :message)");
    query.bindValue(":chat_id", chatId);
    query.bindValue(":sender", sender);
    query.bindValue(":message", message);
    
    bool success = query.exec();
    if (!success) {
        qDebug() << "Failed to save group chat message:" << query.lastError().text();
    }
    return success;
}

void Server::sendGroupChatMessage(const QString &chatId, const QString &sender, const QString &message)
{
    // Получаем всех участников чата
    QSqlQuery membersQuery(srv_db);
    membersQuery.prepare("SELECT username FROM group_chat_members WHERE chat_id = :chat_id");
    membersQuery.bindValue(":chat_id", chatId);
    
    if (membersQuery.exec()) {
        // Формируем сообщение
        QString formattedMessage = "GROUP_MESSAGE:" + chatId + ":" + sender + ":" + message;
        
        while (membersQuery.next()) {
            QString memberUsername = membersQuery.value("username").toString();
            
            // Находим сокет пользователя
            for (const AuthenticatedUser& user : authenticatedUsers) {
                if (user.username == memberUsername && user.isOnline && user.socket) {
                    // Отправляем сообщение
                    Data.clear();
                    QDataStream out(&Data, QIODevice::WriteOnly);
                    out.setVersion(QDataStream::Qt_6_2);
                    out << quint16(0) << formattedMessage;
                    out.device()->seek(0);
                    out << quint16(Data.size() - sizeof(quint16));
                    user.socket->write(Data);
                }
            }
        }
    }
}

void Server::sendGroupChatHistory(const QString &chatId, QTcpSocket *socket)
{
    // Отправляем начало истории
    {
        QString beginMsg = "GROUP_HISTORY_BEGIN:" + chatId;
        Data.clear();
        QDataStream out(&Data, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_6_2);
        out << quint16(0) << beginMsg;
        out.device()->seek(0);
        out << quint16(Data.size() - sizeof(quint16));
        socket->write(Data);
        QThread::msleep(10);
    }
    
    // Отправляем сообщения истории
    QSqlQuery query(srv_db);
    query.prepare("SELECT sender, message, timestamp FROM group_chat_messages "
                 "WHERE chat_id = :chat_id ORDER BY timestamp");
    query.bindValue(":chat_id", chatId);
    

    bool hasRecords = false;
    if (query.exec()) {
        hasRecords = query.next();
    } else {
        qDebug() << "Database error when retrieving group chat history:" << query.lastError().text();
    }
    
    if (!hasRecords) {
        // Если истории нет, отправляем сообщение об этом
        Data.clear();
        QDataStream msgOut(&Data, QIODevice::WriteOnly);
        msgOut.setVersion(QDataStream::Qt_6_2);
        // Исправляем формат сообщения - используем вертикальную черту для разделения полей
        QString noHistoryMsg = "GROUP_HISTORY_MSG:" + chatId + "|0000-00-00 00:00:00|SYSTEM|История группового чата пуста";
        msgOut << quint16(0) << noHistoryMsg;
        msgOut.device()->seek(0);
        msgOut << quint16(Data.size() - sizeof(quint16));
        socket->write(Data);
        QThread::msleep(10);
    } else {
        // Отправляем историю сообщений
        do {

            QString sender = query.value("sender").toString();
            QString message = query.value("message").toString();
            QString timestamp = query.value("timestamp").toString();
            

            // Исправляем формат - используем вертикальную черту для разделения полей
            QString historyMsg = "GROUP_HISTORY_MSG:" + chatId + "|" + timestamp + "|" + sender + "|" + message;

            
            Data.clear();
            QDataStream out(&Data, QIODevice::WriteOnly);
            out.setVersion(QDataStream::Qt_6_2);
            out << quint16(0) << historyMsg;
            out.device()->seek(0);
            out << quint16(Data.size() - sizeof(quint16));
            socket->write(Data);
            QThread::msleep(10);

        } while (query.next());

    }
    
    // Отправляем конец истории
    {
        QString endMsg = "GROUP_HISTORY_END:" + chatId;
        Data.clear();
        QDataStream out(&Data, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_6_2);
        out << quint16(0) << endMsg;
        out.device()->seek(0);
        out << quint16(Data.size() - sizeof(quint16));
        socket->write(Data);
    }
}

void Server::sendUserGroupChats(const QString &username, QTcpSocket *socket)
{
    // Получаем список групповых чатов пользователя
    QSqlQuery query(srv_db);
    query.prepare("SELECT g.id, g.name FROM group_chats g "
                 "INNER JOIN group_chat_members m ON g.id = m.chat_id "
                 "WHERE m.username = :username");
    query.bindValue(":username", username);
    
    QStringList chats;
    
    if (query.exec()) {
        while (query.next()) {
            QString chatId = query.value("id").toString();
            QString chatName = query.value("name").toString();
            chats.append(chatId + ":" + chatName);
        }
    }
    
    // Формируем и отправляем ответ
    QString response = "GROUP_CHATS_LIST:" + chats.join(",");
    
    Data.clear();
    QDataStream out(&Data, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_2);
    out << quint16(0) << response;
    out.device()->seek(0);
    out << quint16(Data.size() - sizeof(quint16));
    socket->write(Data);
}


bool Server::removeUserFromGroupChat(const QString &chatId, const QString &username)
{
    // Проверяем, есть ли пользователь в чате
    QSqlQuery checkQuery(srv_db);
    checkQuery.prepare("SELECT 1 FROM group_chat_members WHERE chat_id = :chat_id AND username = :username");
    checkQuery.bindValue(":chat_id", chatId);
    checkQuery.bindValue(":username", username);
    
    if (!(checkQuery.exec() && checkQuery.next())) {
        // Пользователь не найден в чате
        qDebug() << "User" << username << "not found in chat" << chatId;
        return false;
    }
    
    // Удаляем пользователя из чата
    QSqlQuery query(srv_db);
    query.prepare("DELETE FROM group_chat_members WHERE chat_id = :chat_id AND username = :username");
    query.bindValue(":chat_id", chatId);
    query.bindValue(":username", username);
    
    bool success = query.exec();
    if (!success) {
        qDebug() << "Failed to remove user from group chat:" << query.lastError().text();
    } else {
        qDebug() << "User" << username << "removed from chat" << chatId;
    }
    return success;
}

