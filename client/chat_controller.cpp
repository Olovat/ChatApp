#include "chat_controller.h"
#include "mainwindow.h"
#include <QDataStream>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

ChatController::ChatController(QObject *parent)
    : QObject(parent),
      loginSuccessful(false),
      nextBlockSize(0),
      newFriendPollAttempts(0)
{
    // Инициализация сокета
    socket = new QTcpSocket(this);
    connect(socket, &QTcpSocket::readyRead, this, &ChatController::handleSocketReadyRead);
    connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater); 
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &ChatController::handleSocketError);
    
    // Инициализация таймера для отслеживания таймаутов аутентификации
    authTimeoutTimer = new QTimer(this);
    authTimeoutTimer->setSingleShot(true);
    connect(authTimeoutTimer, &QTimer::timeout, this, &ChatController::handleAuthenticationTimeout);

    newFriendStatusPollTimer = new QTimer(this);
    connect(newFriendStatusPollTimer, &QTimer::timeout, this, &ChatController::onPollNewFriendStatus);

   
    userListRefreshTimer = new QTimer(this);
    connect(userListRefreshTimer, &QTimer::timeout, this, &ChatController::refreshUserListSlot);
    userListRefreshTimer->start(500);
    
    // Инициализация счетчиков непрочитанных сообщений
    unreadPrivateMessageCounts = QMap<QString, int>();
    unreadGroupMessageCounts = QMap<QString, int>();
      // Инициализация списка друзей
    friendList = QStringList();
    lastSearchResults = QStringList();
    
    currentOperation = None;
}

ChatController::~ChatController()
{
    if (socket) {
        socket->close();
        delete socket;
    }
}

bool ChatController::connectToServer(const QString& host, quint16 port)
{
    socket->connectToHost(host, port);
    
    if (socket->waitForConnected(5000)) {
        qDebug() << "Connected to server" << host << ":" << port;
        emit connectionEstablished();
        return true;
    } else {
        qDebug() << "Connection failed:" << socket->errorString();
        emit connectionFailed(socket->errorString());
        return false;
    }
}

void ChatController::disconnectFromServer()
{
    if (socket && socket->state() == QAbstractSocket::ConnectedState) {
        socket->close();
    }
}

bool ChatController::reconnectToServer()
{
    // Если сокет не в подключенном состоянии, переподключаемся
    if (socket && socket->state() != QAbstractSocket::ConnectedState) {
        // Закрываем сокет, если он в процессе подключения или в другом неподходящем состоянии
        if (socket->state() != QAbstractSocket::UnconnectedState) {
            socket->abort();
        }
        
        // Подключаемся к серверу
        return connectToServer("localhost", 5402);
    }
    
    // Если сокет уже подключен, просто возвращаем true
    return socket && socket->state() == QAbstractSocket::ConnectedState;
}

bool ChatController::isConnected() const
{
    // Проверяем, существует ли сокет и находится ли он в подключенном состоянии
    return socket && socket->state() == QAbstractSocket::ConnectedState;
}

void ChatController::authorizeUser(const QString &username, const QString &password)
{
    if (loginSuccessful && this->username == username) {
        qDebug() << "User" << username << "already authenticated, skipping re-authentication";
        emit authenticationSuccessful();
        return;
    }
      this->username = username;
    this->password = password;
    
    QSettings settings("ChatApp", "Client");
    settings.beginGroup("Friends");
    
    if (settings.contains(username)) {
        friendList = settings.value(username, QStringList()).toStringList();
        qDebug() << "Loaded friend list for user" << username << ":" << friendList.size() 
                 << "friends. List contents:" << friendList;
    } else {
        friendList.clear();
        qDebug() << "No saved friend list found for user" << username << ", starting with empty list";
    }
    settings.endGroup();
    
    currentOperation = Auth;
    
    QString authRequest = "AUTH:" + username + ":" + password;
    sendToServer(authRequest);
    
    // Запускаем таймер ожидания ответа от сервера
    authTimeoutTimer->start(10000); // 10 секунд на ответ
}

void ChatController::registerUser(const QString &username, const QString &password)
{
    this->username = username;
    this->password = password;
    
    currentOperation = Register;
    
    QString regRequest = "REGISTER:" + username + ":" + password;
    sendToServer(regRequest);
    
    // Запускаем таймер ожидания ответа от сервера
    authTimeoutTimer->start(10000); // 10 секунд на ответ
}

void ChatController::handleSocketReadyRead()
{
    QDataStream in(socket);
    in.setVersion(QDataStream::Qt_5_15);
    
    if (in.status() == QDataStream::Ok) {
        // Обработка входящих данных
        for(;;) {
            if (nextBlockSize == 0) {
                if (socket->bytesAvailable() < static_cast<qint64>(sizeof(quint16))) {
                    break;
                }
                in >> nextBlockSize;
            }
            
            if (socket->bytesAvailable() < nextBlockSize) {
                break;
            }
            
            QString str;
            in >> str;
            nextBlockSize = 0;
            
            processServerResponse(str);
        }
    }
}

void ChatController::handleSocketError(QAbstractSocket::SocketError socketError)
{
    QString errorMessage;
    
    switch (socketError) {
        case QAbstractSocket::RemoteHostClosedError:
            errorMessage = "Соединение с сервером закрыто";
            break;
        case QAbstractSocket::HostNotFoundError:
            errorMessage = "Сервер не найден";
            break;
        case QAbstractSocket::ConnectionRefusedError:
            errorMessage = "Соединение отклонено";
            break;
        default:
            errorMessage = "Ошибка: " + socket->errorString();
    }
    
    emit connectionFailed(errorMessage);
}

void ChatController::handleAuthenticationTimeout()
{
    if (currentOperation == Auth) {
        emit authenticationFailed("Время ожидания ответа от сервера истекло");
    } else if (currentOperation == Register) {
        emit registrationFailed("Время ожидания ответа от сервера истекло");
    }
    
    currentOperation = None;
}

void ChatController::sendToServer(const QString &message)
{
    recentSentMessages.append(message);
    
    QByteArray arrBlock;
    QDataStream out(&arrBlock, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_15);
    
    out << quint16(0) << message;
    out.device()->seek(0);
    out << quint16(arrBlock.size() - sizeof(quint16));
    
    socket->write(arrBlock);
}

void ChatController::sendMessageToServer(const QString &message)
{
    sendToServer(message);
}

void ChatController::processServerResponse(const QString &response)
{
    qDebug() << "Received from server:" << response;
    
    // Остановка таймера ожидания ответа
    if (authTimeoutTimer && authTimeoutTimer->isActive()) { 
        authTimeoutTimer->stop();
    }
    
    QStringList parts = response.split(":");
    
    if (parts.isEmpty()) {
        return;
    }
    
    QString command = parts[0];
    if (command == "AUTH_OK" || command == "AUTH_SUCCESS") {
        loginSuccessful = true;
        currentOperation = None;
        
        emit authenticationSuccessful();
        requestUserList();
    }    else if (command == "AUTH_ERROR" || command == "AUTH_FAIL" || command == "AUTH_FAILED") {
        // Проверяем, не обрабатываем ли мы уже ошибку
        if (currentOperation != Auth) {
            qDebug() << "Ignoring authentication error as currentOperation is not Auth";
            return;
        }
        
        loginSuccessful = false;
        currentOperation = None;
        QString errorMessage = parts.size() > 1 ? parts[1] : "Ошибка авторизации";
        
        // Специальная обработка для случая "User already logged in"
        if (errorMessage.contains("User already logged in")) {
            qDebug() << "User already logged in - treating as success";
            loginSuccessful = true;
            emit authenticationSuccessful();
        } else {
            // Стандартная обработка ошибок авторизации
            emit authenticationFailed(errorMessage);
            
            // Очищаем буфер, чтобы избежать дублирования сообщений
            clearSocketBuffer();
        }
    }else if (command == "REGISTER_OK" || command == "REGISTER_SUCCESS") {
        currentOperation = None;
        emit registrationSuccessful();
    }
    else if (command == "REGISTER_ERROR" || command == "REGISTER_FAILED") {
        currentOperation = None;
        QString errorMessage = parts.size() > 1 ? parts[1] : "Ошибка регистрации";
        emit registrationFailed(errorMessage);
    }    else if (command == "USER_LIST" || command == "USERLIST") {
        QStringList tempUserList;
        QStringList tempOnlineUsernames; 
        int firstColonPos = response.indexOf(':');
        if (firstColonPos == -1) {
            qDebug() << "ChatController: Malformed USERLIST command (no colon):" << response;
            return;
        }
        QString payload = response.mid(firstColonPos + 1);
        
        if (payload.isEmpty()) {
            qDebug() << "ChatController: USERLIST payload is empty. Clearing user lists. Response:" << response;
            this->userList.clear();
            this->onlineUsers.clear();
            emit userListUpdated(this->userList);
            return;
        }

        QStringList userEntries = payload.split(',');
        if (userEntries.isEmpty() || (userEntries.size() == 1 && userEntries.first().isEmpty())) {
            qDebug() << "ChatController: USERLIST no user entries after splitting payload. Clearing user lists. Payload:" << payload;
            this->userList.clear();
            this->onlineUsers.clear();
            emit userListUpdated(this->userList);
            return;
        }

        qDebug() << "Received user list command. Payload:" << payload << "Entries count:" << userEntries.size();
        
        this->userList.clear(); 
        this->onlineUsers.clear(); 
        
        for (const QString &entry : userEntries) {
            if (entry.isEmpty()) {
                continue;
            }
            
            // Поддержка формата с 4 частами (username:status:type1:type2)
            QString username;
            bool isOnline = false;
            QString userType;
            
            QStringList userDetails = entry.split(':');
            if (userDetails.size() >= 2) {
                username = userDetails[0].trimmed();
                QString statusStr = userDetails[1].trimmed();
                isOnline = (statusStr == "1" || statusStr.compare("online", Qt::CaseInsensitive) == 0 || statusStr.toInt() > 0);
                
                // Собираем типы пользователя, если они есть
                if (userDetails.size() >= 3) {
                    userType = userDetails.mid(2).join(":");
                }
                
                QString formattedUser = username + ":" + (isOnline ? "1" : "0") + ":" + userType;
                tempUserList.append(formattedUser);
                
                qDebug() << "ChatController parsed user entry:" << formattedUser 
                        << "(username=" << username << ", isOnline=" << isOnline 
                        << ", userType=" << userType << ")";

                if (isOnline) {
                    tempOnlineUsernames.append(username);
                }
            } else {
                qDebug() << "ChatController: Malformed user entry in USERLIST:" << entry 
                        << "(expected at least 2 parts, got" << userDetails.size() << ")";
            }
        }
        
        this->userList = tempUserList;
        this->onlineUsers = tempOnlineUsernames;
        
        emit userListUpdated(this->userList);
    } else if (command == "FRIEND_STATUS" || command == "NEW_FRIEND_STATUS") {
        // Обработка статуса нового друга
        if (parts.size() < 3) {
            qDebug() << "ChatController: Malformed FRIEND_STATUS command:" << response;
            return;
        }
        
        QString friendUsername = parts[1];
        QString status = parts[2];
        
        qDebug() << "Friend status update:" << friendUsername << "is" << (status == "1" ? "online" : "offline");
        
        // Обновляем статус друга в списке друзей
        int index = friendList.indexOf(friendUsername);
        if (index != -1) {
            // Друг найден в списке, обновляем статус
            friendList[index] = friendUsername + ":" + status;
        } else {
            // Друг не найден, добавляем в список с новым статусом
            friendList.append(friendUsername + ":" + status);
        }
          emit friendStatusUpdated(friendUsername, (status == "1"));    } else if (command == "PRIVATE") {
        // Обработка входящего приватного сообщения в формате PRIVATE:sender:message
        if (parts.size() < 3) {
            qDebug() << "ChatController: Malformed PRIVATE command:" << response;
            return;
        }
        
        QString sender = parts[1];
        QString message = parts.mid(2).join(":");
        
        qDebug() << "Received private message from" << sender << ":" << message;
        
        // Текущее время для метки времени сообщения
        QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
        
        // Проверка на дубликаты сообщений
        if (!isMessageDuplicate(sender, timestamp, false)) {
            // Сохраняем timestamp сообщения
            if (!timestamp.isEmpty()) {
                lastPrivateChatTimestamps[sender] = timestamp;
            }
            
            // Добавляем отправителя в список недавних собеседников
            recentChatPartners.insert(sender);
            
            // Отправляем сигнал о получении сообщения
            emit privateMessageReceived(sender, message, timestamp);
            
            // Сохраняем сообщение в истории
            emit privateMessageStored(sender, username, message, timestamp);
            
            // Увеличиваем счетчик непрочитанных сообщений для отправителя
            unreadPrivateMessageCounts[sender] = unreadPrivateMessageCounts.value(sender, 0) + 1;
            
            // Сообщаем об обновлении счетчиков непрочитанных сообщений
            emit unreadCountsUpdated(unreadPrivateMessageCounts, unreadGroupMessageCounts);
            
            qDebug() << "Updated unread count for" << sender << "to" << unreadPrivateMessageCounts[sender];
        } else {
            qDebug() << "PRIVATE: Duplicate message detected, ignoring";
        }
    } else if (command == "PRIVATE_MESSAGE" || command == "PRIVMSG") {
        // Обработка входящего приватного сообщения
        if (parts.size() < 3) {
            qDebug() << "ChatController: Malformed PRIVATE_MESSAGE command:" << response;
            return;
        }
        
        QString sender = parts[1];
        QString message = parts.mid(2).join(":");
        
        qDebug() << "Received private message from" << sender << ":" << message;
        
        // Текущее время для метки времени сообщения
        QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
        
        // Отправляем сигнал о получении сообщения
        emit privateMessageReceived(sender, message, timestamp);
        
        // Сохраняем сообщение в истории
        emit privateMessageStored(sender, username, message, timestamp);
    } else if (command == "MESSAGE_HISTORY" || command == "HISTORY") {
        // Обработка истории сообщений
        if (parts.size() < 2) {
            qDebug() << "ChatController: Malformed MESSAGE_HISTORY command:" << response;
            return;
        }
        
        QStringList messages = QString(parts.mid(1).join(":")).split(";");
        for (const QString &msg : messages) {
            QStringList msgParts = msg.split(',');
            if (msgParts.size() < 3) {
                continue; // Игнорируем некорректные сообщения
            }
            
            QString sender = msgParts[0];
            QString recipient = msgParts[1];
            QString message = msgParts.mid(2).join(",");
            QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
            
            // Отправляем сигнал о получении сообщения из истории
            emit privateMessageReceived(sender, message, timestamp);
        }
    } else if (command == "PRIVATE_MSG") {
    if (parts.size() >= 3) {
        QString sender = parts[1];
        QString message = parts[2];
        QString timestamp = parts.size() > 3 ? parts[3] : QDateTime::currentDateTime().toString("hh:mm:ss");
        
        // Отладка
        qDebug() << "PRIVATE_MSG received: Sender=" << sender << ", Message=" << message << ", Timestamp=" << timestamp;
        
        // Проверка на дубликаты сообщений
        if (!isMessageDuplicate(sender, timestamp, false)) {
            // Сохраняем timestamp сообщения
            if (!timestamp.isEmpty()) {
                lastPrivateChatTimestamps[sender] = timestamp;
            }
            
            // Добавляем отправителя в список недавних собеседников
            recentChatPartners.insert(sender);
            
            // ВАЖНО: Сохраняем сообщение в базе данных на сервере
            if (sender != getCurrentUsername()) {
                storePrivateMessage(sender, getCurrentUsername(), message, timestamp);
            }
            
            // Вызов сигнала для обработки приватного сообщения
            emit privateMessageReceived(sender, message, timestamp);
            
            // Увеличиваем счетчик непрочитанных сообщений только если окно не активно
            unreadPrivateMessageCounts[sender] = unreadPrivateMessageCounts.value(sender, 0) + 1;
            
            // Сообщаем об обновлении счетчиков непрочитанных сообщений
            emit unreadCountsUpdated(unreadPrivateMessageCounts, unreadGroupMessageCounts);
        } else {
            qDebug() << "PRIVATE_MSG: Duplicate message detected, ignoring";
        }
    }
}
// Обработчик сообщения от сервера в формате "username: PRIVMSG:recipient:message"
else if (parts.size() >= 2 && parts[1].trimmed() == "PRIVMSG") {
    if (parts.size() >= 4) {
        QString sender = parts[0];
        QString recipient = parts[2];
        QString message = parts[3];
        QString timestamp = parts.size() > 4 ? parts[4] : QDateTime::currentDateTime().toString("hh:mm:ss");
        
        qDebug() << "Interpreted as private message: sender=" << sender 
                << ", recipient=" << recipient << ", message=" << message;
        
        // Проверяем, относится ли сообщение к текущему пользователю
        if (recipient == getCurrentUsername() || sender == getCurrentUsername()) {
            // Проверка на дубликаты
            if (!isMessageDuplicate(sender, timestamp, false)) {
                // Добавляем в список недавних собеседников
                QString chatPartner = (sender == getCurrentUsername()) ? recipient : sender;
                recentChatPartners.insert(chatPartner);
                
                // Отправляем сигнал о новом приватном сообщении
                emit privateMessageReceived(sender, message, timestamp);
            }
        }
    }
} else if (command == "PRIVATE_HISTORY_CMD") {
    if (parts.size() >= 3 && parts[1] == "BEGIN") {
        // Начало получения истории приватных сообщений
        currentHistoryTarget = parts[2];
        historyBuffer.clear();
        qDebug() << "Starting to receive private history for:" << currentHistoryTarget;
    } else if (parts.size() >= 2 && parts[1] == "END") {
        // Конец получения истории
        qDebug() << "Finished receiving private history for:" << currentHistoryTarget;
        if (!currentHistoryTarget.isEmpty()) {
            emit privateHistoryReceived(currentHistoryTarget, historyBuffer);
            currentHistoryTarget.clear();
            historyBuffer.clear();
        }
    }
} else if (command == "PRIVATE_HISTORY_MSG") {
    // Сообщение из истории: PRIVATE_HISTORY_MSG:sender:recipient:message:timestamp
    if (parts.size() >= 5 && !currentHistoryTarget.isEmpty()) {
        QString sender = parts[1];
        QString recipient = parts[2]; 
        QString message = parts[3];
        QString timestamp = parts[4];
        
        // Форматируем сообщение для добавления в буфер истории
        QString formattedMessage = QString("[%1] %2: %3").arg(timestamp, sender, message);
        historyBuffer.append(formattedMessage);
        
        qDebug() << "Received history message:" << formattedMessage;
    }
} else if (command == "SEARCH_RESULTS") {
    // Обработка результатов поиска пользователей
    QStringList searchResults;
    if (parts.size() > 1) {
        // Пропускаем первую часть (команду), остальные - это найденные пользователи
        for (int i = 1; i < parts.size(); ++i) {
            if (!parts[i].trimmed().isEmpty()) {
                searchResults.append(parts[i].trimmed());
            }
        }
    }
    
    qDebug() << "Received search results:" << searchResults;
    lastSearchResults = searchResults;
    emit searchResultsReady(searchResults);
} else if (command == "FRIEND_ADDED") {
    // Обработка успешного добавления друга
    if (parts.size() >= 2) {
        QString friendUsername = parts[1];
        qDebug() << "Friend successfully added:" << friendUsername;
        
        // Добавляем друга в локальный список, если его там еще нет
        if (!friendList.contains(friendUsername)) {
            friendList.append(friendUsername);
            saveFriendList();
        }
        
        // Начинаем отслеживание статуса нового друга
        startPollingForFriendStatus(friendUsername);
        
        // Отправляем сигнал об успешном добавлении
        emit friendAddedSuccessfully(friendUsername);
        
        // Запрашиваем обновленный список пользователей
        requestUserList();
    }
} else if (command == "FRIEND_ADD_FAIL") {
    // Обработка неудачного добавления друга
    if (parts.size() >= 2) {
        QString friendUsername = parts[1];
        qDebug() << "Failed to add friend:" << friendUsername;
        
        // Можно показать сообщение об ошибке пользователю
        // Или отправить соответствующий сигнал
    }
} else if (command == "UNREAD_COUNT") {
    // Обработка ответа сервера о количестве непрочитанных сообщений
    if (parts.size() >= 3) {
        QString chatPartner = parts[1];
        int count = parts[2].toInt();
        
        qDebug() << "Received unread count for" << chatPartner << ":" << count;
        
        // Обновляем счетчик непрочитанных сообщений
        if (count > 0) {
            unreadPrivateMessageCounts[chatPartner] = count;
        } else {
            unreadPrivateMessageCounts.remove(chatPartner);
        }
        
        // Отправляем сигнал об обновлении счетчиков
        emit unreadCountsUpdated(unreadPrivateMessageCounts, unreadGroupMessageCounts);
    }
} else {
        qDebug() << "ChatController: Unknown command from server:" << command;
    }
}

void ChatController::sendPrivateMessage(const QString &recipient, const QString &message)
{
    qDebug() << "Sending private message to" << recipient << ":" << message;
    
    // Формат команды для отправки приватного сообщения
    QString command = QString("PRIVATE:%1:%2").arg(recipient, message);
    sendToServer(command);
    
    // Текущее время для сохранения в истории
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    
    // УБИРАЕМ это - сообщение будет показано когда сервер пришлет подтверждение
    // emit privateMessageReceived(username, message, timestamp);
    
    // Сохраняем сообщение в истории
    emit privateMessageStored(username, recipient, message, timestamp);
    
    // Добавляем в список последних отправленных сообщений для отладки
    recentSentMessages.append(command);
    if (recentSentMessages.size() > 10) {
        recentSentMessages.removeFirst();
    }
}

void ChatController::storePrivateMessage(const QString &sender, const QString &recipient, const QString &message, const QString &timestamp)
{
    // Эта команда заставляет сервер сохранить сообщение в базе данных
    QString command = QString("STORE_PRIVMSG:%1:%2:%3:%4").arg(sender, recipient, message, timestamp);
    sendToServer(command);
    qDebug() << "Requesting server to store message in history:" << command;
    
    // Добавляем получателя в список последних собеседников
    recentChatPartners.insert(recipient);
}

void ChatController::requestPrivateMessageHistory(const QString &username)
{
    qDebug() << "Requesting private message history with" << username;
    
    // Очищаем буфер истории перед запросом
    historyBuffer.clear();
    currentHistoryTarget = username;
    
    // Запрашиваем историю сообщений - сервер должен вернуть все сообщения между пользователями
    // ИСПРАВЛЕНО: делаем только один запрос, сервер сам обработает историю в обе стороны
    sendToServer(QString("GET_PRIVATE_HISTORY:%1:%2").arg(this->username, username));
    
    // Обновляем интерфейс, чтобы показать, что идёт загрузка
    emit historyRequestStarted(username);
}

void ChatController::searchUsers(const QString &query)
{
    qDebug() << "Отправка запроса на поиск пользователей:" << query;
    sendToServer("SEARCH_USERS:" + query);
    // Очищаем предыдущие результаты поиска
    lastSearchResults.clear();
}

void ChatController::addUserToFriends(const QString &username)
{
    qDebug() << "Отправка запроса на добавление пользователя в друзья:" << username;
    sendToServer("ADD_FRIEND:" + username);
}

void ChatController::createGroupChat(const QString &chatName, const QString &chatId)
{
    sendToServer("CREATE_GROUP:" + chatId + ":" + chatName);
}

void ChatController::joinGroupChat(const QString &chatId)
{
    sendToServer("JOIN_GROUP:" + chatId);
}

void ChatController::startAddUserToGroupMode(const QString &chatId)
{
    pendingGroupChatId = chatId;
}

void ChatController::addUserToGroupChat(const QString &chatId, const QString &username)
{
    sendToServer("ADD_TO_GROUP:" + chatId + ":" + username);
}

void ChatController::removeUserFromGroupChat(const QString &chatId, const QString &username)
{
    sendToServer("REMOVE_FROM_GROUP:" + chatId + ":" + username);
}

void ChatController::deleteGroupChat(const QString &chatId)
{
    sendToServer("DELETE_GROUP:" + chatId);
}

void ChatController::requestUnreadCounts()
{
    sendToServer("GET_UNREAD_COUNTS");
}

void ChatController::requestUnreadCountForUser(const QString &username)
{
    sendToServer("GET_UNREAD_COUNT:" + username);
}

void ChatController::markMessagesAsRead(const QString &username)
{
    sendToServer("MARK_READ:" + username);
}

void ChatController::clearSocketBuffer()
{
    buffer.clear();
    nextBlockSize = 0;
}

void ChatController::requestUserList()
{
    qDebug() << "Requesting user list";
    sendToServer("GET_USERS");
    sendToServer("GET_USERLIST");
}

bool ChatController::isMessageDuplicate(const QString &chatId, const QString &timestamp, bool isGroup)
{
    if (timestamp.isEmpty()) {
        return false;
    }
    
    // Используем более консервативный подход - считаем дубликатом только если прошло менее 50мс
    // Это позволяет быстро отправлять сообщения, но предотвращает настоящие дубликаты
    static QMap<QString, QDateTime> lastMessageTimes;
    QString key = (isGroup ? "GROUP_" : "PRIVATE_") + chatId;
    
    QDateTime currentTime = QDateTime::currentDateTime();
    
    if (lastMessageTimes.contains(key)) {
        QDateTime lastTime = lastMessageTimes[key];
        qint64 timeDiff = lastTime.msecsTo(currentTime);
        
        // Считаем дубликатом только если прошло менее 50 миллисекунд
        // Это достаточно для предотвращения настоящих дубликатов сети
        if (timeDiff < 50) {
            qDebug() << "Duplicate message detected for" << chatId << "time diff:" << timeDiff << "ms";
            return true;
        }
    }
    
    // Обновляем время последнего сообщения
    lastMessageTimes[key] = currentTime;
    
    return false;
}

bool ChatController::isFriend(const QString &username) const 
{
    return friendList.contains(username);
}

QStringList ChatController::getFriendList() const 
{
    return friendList;
}

void ChatController::saveFriendList()
{
    if (!username.isEmpty()) {
        QSettings settings("ChatApp", "Client");
        settings.beginGroup("Friends");
        
        qDebug() << "Saving friend list with key:" << username << "Value:" << friendList;
        
        settings.setValue(username, friendList);
        settings.endGroup();
        
        settings.beginGroup("Friends");
        QStringList savedList = settings.value(username, QStringList()).toStringList();
        settings.endGroup();
        
        qDebug() << "Saved friend list for" << username << ":" << friendList.size() 
                 << "friends. Verification read back:" << savedList.size() << "friends";
    } else {
        qDebug() << "Cannot save friend list: current username is empty.";
    }
}

void ChatController::startPollingForFriendStatus(const QString& username) {
    if (newFriendStatusPollTimer->isActive()) {
        qDebug() << "Poll timer was active for" << currentlyPollingFriend << ", stopping it first.";
        newFriendStatusPollTimer->stop();
    }
    currentlyPollingFriend = username;
    newFriendPollAttempts = 0;
    qDebug() << "Starting to poll for status of new friend:" << username << "at" << newFriendStatusPollTimer->interval() << "ms interval.";
    newFriendStatusPollTimer->start(2000); // интервал в 2 секунды
    onPollNewFriendStatus(); // вызываем сразу, чтобы не ждать таймера
}

void ChatController::stopPollingForFriendStatus() {
    if (newFriendStatusPollTimer->isActive()) {
        newFriendStatusPollTimer->stop();
        qDebug() << "Stopped polling for status of:" << currentlyPollingFriend;
    }
    currentlyPollingFriend.clear();
    newFriendPollAttempts = 0;
}

void ChatController::onPollNewFriendStatus() {
    if (currentlyPollingFriend.isEmpty()) {
        stopPollingForFriendStatus();
        return;
    }

    newFriendPollAttempts++;
    qDebug() << "Polling for status of" << currentlyPollingFriend << ", attempt" << newFriendPollAttempts;
    requestUserList(); 

    if (newFriendPollAttempts >= 20) { 
        qDebug() << "Max poll attempts reached for" << currentlyPollingFriend << ". Stopping poll.";
        stopPollingForFriendStatus();
    }
}

void ChatController::refreshUserListSlot()
{
    if (isConnected() && isLoginSuccessful()) {
        requestUserList();
    }
}

void ChatController::requestRecentChatPartners()
{
    // Запрашиваем список недавних собеседников
    QString command = "GET_RECENT_PARTNERS";
    sendToServer(command);
    qDebug() << "Requesting recent chat partners";
}

bool ChatController::isLoginSuccessful() const
{
    return loginSuccessful;
}

QString ChatController::getCurrentUsername() const
{
    return username;
}

