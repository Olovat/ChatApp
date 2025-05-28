#include "chat_controller.h"
#include "mainwindow.h"
#include <QDataStream>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>

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
    userListRefreshTimer->start(5000); // Обновляем каждые 5 секунд чтобы не сбрасывать счетчики уведомлений
    
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
    
    // Очищаем список друзей - он будет загружен с сервера
    friendList.clear();
    qDebug() << "Starting with empty friend list for user" << username << ", friends will be loaded from server";
    
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
        QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");        // Проверка на дубликаты сообщений
        if (!isMessageDuplicate(sender, message, false)) {
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
            
            // Запрашиваем актуальный счетчик непрочитанных сообщений с сервера
            // вместо локального увеличения на 1
            requestUnreadCountForUser(sender);
            
            qDebug() << "Requested updated unread count for" << sender << "from server";
        } else {
            qDebug() << "PRIVATE: Duplicate message detected, ignoring";
        }}else if (command == "MESSAGE_HISTORY" || command == "HISTORY") {
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
            // Проверка на дубликаты - используем отправителя как chatId для приватных сообщений
            QString chatPartner = (sender == getCurrentUsername()) ? recipient : sender;
            if (!isMessageDuplicate(chatPartner, message, false)) {
                // Добавляем в список недавних собеседников
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
    // Сообщение из истории: PRIVATE_HISTORY_MSG:timestamp|sender|recipient|message_text
    if (parts.size() >= 2 && !currentHistoryTarget.isEmpty()) {
        // Парсим данные, разделенные символом "|"
        QString remainingData = parts.mid(1).join(":");
        QStringList msgParts = remainingData.split("|");
        
        if (msgParts.size() >= 4) {
            QString timestamp = msgParts[0];
            QString sender = msgParts[1];
            QString recipient = msgParts[2];
            QString message = msgParts[3];
            
            // Форматируем сообщение для добавления в буфер истории, включая всю информацию
            QString formattedMessage = QString("%1|%2|%3|%4").arg(timestamp, sender, recipient, message);
            historyBuffer.append(formattedMessage);
            
            qDebug() << "Received history message:" << formattedMessage;
        }
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
            // Friend list is now managed entirely by server, no local persistence needed
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
    if (parts.size() >= 3) {        QString chatPartner = parts[1];
        int count = parts[2].toInt();
        
        qDebug() << "Received unread count for" << chatPartner << ":" << count;
        
        // Определяем, является ли это групповым чатом по формату идентификатора
        // Групповые чаты обычно имеют UUID-подобный формат, а приватные чаты - имя пользователя
        bool isGroupChat = chatPartner.contains("-") || emit groupChatExists(chatPartner);
          if (isGroupChat) {
            // Обновляем счетчик непрочитанных сообщений для группового чата
            // ИСПРАВЛЕНИЕ: НЕ удаляем chatPartner из карты, а устанавливаем значение
            unreadGroupMessageCounts[chatPartner] = count;
        } else {
            // Обновляем счетчик непрочитанных сообщений для приватного чата
            // ИСПРАВЛЕНИЕ: НЕ удаляем chatPartner из карты, а устанавливаем значение
            unreadPrivateMessageCounts[chatPartner] = count;
        }
        
        // Отправляем сигнал об обновлении счетчиков
        emit unreadCountsUpdated(unreadPrivateMessageCounts, unreadGroupMessageCounts);
    }
} else if (command == "GROUP_CHAT_CREATED") {
    // Обработка успешного создания группового чата
    if (parts.size() >= 3) {
        QString chatId = parts[1];
        QString chatName = parts[2];
        qDebug() << "Group chat created successfully:" << chatName << "(ID:" << chatId << ")";
        
        // Отправляем сигнал о создании группового чата
        emit groupChatCreated(chatId, chatName);
        
        // Запрашиваем обновленный список групповых чатов
        sendToServer("GET_GROUP_CHATS");
    }
} else if (command == "GROUP_MESSAGE") {
    // Обработка входящего группового сообщения: GROUP_MESSAGE:chatId:sender:message:timestamp
    if (parts.size() >= 4) {
        QString chatId = parts[1];
        QString sender = parts[2];
        QString message = parts[3];
        QString timestamp = parts.size() > 4 ? parts[4] : QDateTime::currentDateTime().toString("hh:mm:ss");
        
        qDebug() << "Received group message in chat" << chatId << "from" << sender << ":" << message;
         bool isSystemMessage = (sender == "SYSTEM" || sender.isEmpty()) || 
                              message.contains("присоединился к чату") ||
                              message.contains("покинул чат") ||
                              message.contains("был добавлен") ||
                              message.contains("был удален") ||
                              message.contains("создал чат");
        
        // Проверка на дубликаты
        if (!isMessageDuplicate(chatId, message, true)) {
            // Отправляем сигнал о получении группового сообщения
            emit groupMessageReceived(chatId, sender, message, timestamp);
            
            // Обновляем счетчик непрочитанных сообщений для группового чата
            if (!isSystemMessage && sender != getCurrentUsername()) {
                unreadGroupMessageCounts[chatId]++;
                emit unreadCountsUpdated(unreadPrivateMessageCounts, unreadGroupMessageCounts);
            }
        }
    }
} else if (command == "GROUP_CHATS_LIST") {
    // Обработка списка групповых чатов: GROUP_CHATS_LIST:chatId1:chatName1,chatId2:chatName2,...
    QStringList groupChats;
    if (parts.size() > 1) {
        QString chatData = parts.mid(1).join(":");
        if (!chatData.isEmpty()) {
            QStringList chatEntries = chatData.split(",");
            for (const QString &entry : chatEntries) {
                if (!entry.isEmpty()) {
                    groupChats.append(entry);
                }
            }
        }
    }
    
    qDebug() << "Received group chats list:" << groupChats;
    emit groupChatsListReceived(groupChats);
} else if (command == "GROUP_CHAT_INFO") {
    // Обработка информации о групповом чате: GROUP_CHAT_INFO:chatId:chatName:members
    if (parts.size() >= 4) {
        QString chatId = parts[1];
        QString chatName = parts[2];
        QStringList members = parts[3].split(",", Qt::SkipEmptyParts);
        
        qDebug() << "Received group chat info for" << chatName << "(" << chatId << ") - members:" << members;
        emit groupMembersUpdated(chatId, members, QString()); // creator будет получен отдельно
    }
} else if (command == "GROUP_CHAT_CREATOR") {
    // Обработка информации о создателе группового чата: GROUP_CHAT_CREATOR:chatId:creator
    if (parts.size() >= 3) {
        QString chatId = parts[1];
        QString creator = parts[2];
        
        qDebug() << "Received group chat creator info for" << chatId << "- creator:" << creator;
        emit groupCreatorUpdated(chatId, creator);
    }
} else if (command == "GROUP_HISTORY") {
    // Обработка истории группового чата: GROUP_HISTORY:chatId:timestamp|sender|message,...
    if (parts.size() >= 2) {
        QString chatId = parts[1];
        QList<QPair<QString, QString>> history;
        
        if (parts.size() > 2) {
            QString historyData = parts.mid(2).join(":");
            QStringList historyEntries = historyData.split(",");
            
            for (const QString &entry : historyEntries) {
                QStringList msgParts = entry.split("|");
                if (msgParts.size() >= 3) {
                    QString timestamp = msgParts[0];
                    QString sender = msgParts[1];
                    QString message = msgParts[2];
                    history.append(qMakePair(sender + " (" + timestamp + ")", message));
                }
            }
        }
        
        qDebug() << "Received group history for chat" << chatId << "with" << history.size() << "messages";
        emit groupHistoryReceived(chatId, history);
    }
} else if (command == "GROUP_HISTORY_BEGIN") {
    // Начало получения истории группового чата
    if (parts.size() >= 2) {
        QString chatId = parts[1];
        qDebug() << "Beginning group history reception for chat" << chatId;
        currentGroupHistoryTarget = chatId;
        groupHistoryBuffer.clear();
    }
} else if (command == "GROUP_HISTORY_MSG") {
    if (parts.size() >= 2) {
        QString messageData = parts.mid(1).join(":");
        QStringList msgParts = messageData.split("|");
        
        if (msgParts.size() >= 4) {
            QString chatId = msgParts[0];
            QString timestamp = msgParts[1];
            QString sender = msgParts[2];
            QString message = msgParts[3];
            QString isRead = msgParts.size() >= 5 ? msgParts[4] : "1"; // По умолчанию считаем прочитанным для совместимости
            
            // Пропускаем системные сообщения о пустой истории
            if (sender != "SYSTEM" || !message.contains("пуста")) {
                QString formattedMessage = QString("[%1] %2: %3|READ:%4").arg(timestamp, sender, message, isRead);
                groupHistoryBuffer.append(formattedMessage);
            }
            
            qDebug() << "Received group history message for chat" << chatId << "from" << sender << "isRead:" << isRead;
        }
    }
} else if (command == "GROUP_HISTORY_END") {
    // Конец получения истории группового чата
    if (parts.size() >= 2) {
        QString chatId = parts[1];
        qDebug() << "Finished receiving group history for chat" << chatId << "with" << groupHistoryBuffer.size() << "messages";
          // Отправляем всю собранную историю
        QList<QPair<QString, QString>> history;
        for (const QString &message : groupHistoryBuffer) {
            QRegularExpression regex(R"(\[(.+?)\] (.+?): (.+)\|READ:(.+))");
            QRegularExpressionMatch match = regex.match(message);
            
            if (match.hasMatch()) {
                QString timestamp = match.captured(1);
                QString sender = match.captured(2);
                QString content = match.captured(3);
                QString readStatus = match.captured(4);
                // Формируем строку в ожидаемом GroupChatController формате: "[время] отправитель"
                QString formattedSender = "[" + timestamp + "] " + sender;
                QString contentWithReadStatus = content + "|READ:" + readStatus;
                history.append(qMakePair(formattedSender, contentWithReadStatus));
                qDebug() << "ChatController: Formatted history entry - sender:" << formattedSender 
                         << "content:" << content << "readStatus:" << readStatus;
            } else {
                qDebug() << "ChatController: Failed to parse message:" << message;
            }
        }
        
        qDebug() << "ChatController: Emitting groupHistoryReceived with" << history.size() << "messages for chat" << chatId;
        emit groupHistoryReceived(chatId, history);
        groupHistoryBuffer.clear();
        currentGroupHistoryTarget.clear();
    }
} else if (command == "GROUP_CHAT_DELETED") {
    // Обработка удаления группового чата
    if (parts.size() >= 2) {
        QString chatId = parts[1];
        qDebug() << "Group chat deleted:" << chatId;
        emit groupChatDeleted(chatId);
        
        // Удаляем счетчик непрочитанных сообщений для этого чата
        unreadGroupMessageCounts.remove(chatId);
        emit unreadCountsUpdated(unreadPrivateMessageCounts, unreadGroupMessageCounts);
    }
} else if (command == "USER_ADDED_TO_GROUP") {
    // Обработка добавления пользователя в группу
    if (parts.size() >= 3) {
        QString chatId = parts[1];
        QString username = parts[2];
        qDebug() << "User" << username << "added to group" << chatId;
        
        // Запрашиваем обновленную информацию о группе
        sendToServer("GROUP_GET_INFO:" + chatId);
    }
} else if (command == "USER_REMOVED_FROM_GROUP") {
    // Обработка удаления пользователя из группы
    if (parts.size() >= 3) {
        QString chatId = parts[1];
        QString username = parts[2];
        qDebug() << "User" << username << "removed from group" << chatId;
        
        // Запрашиваем обновленную информацию о группе
        sendToServer("GROUP_GET_INFO:" + chatId);
    }
} else if (command == "GROUP_CHAT_ERROR") {
    // Обработка ошибок группого чата
    if (parts.size() >= 2) {
        QString errorMessage = parts[1];
        qDebug() << "Group chat error:" << errorMessage;
        // Можно отправить сигнал об ошибке или показать пользователю
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
    
    // Сохраняем сообщение в истории без дублирования отображения
    // Сообщение будет добавлено в модель через PrivateChatController
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
   // Используем неиспользуемый параметр, чтобы убрать предупреждение
    const QString actualChatId = chatId.isEmpty() ? "AUTO_GENERATE" : chatId;
    
    // Отправляем запрос на создание группы
    sendToServer("CREATE_GROUP_CHAT:" + chatName + ":" + actualChatId);
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
    sendToServer("GROUP_ADD_USER:" + chatId + ":" + username);
}

bool ChatController::isPendingGroupChatAddition() const
{
    return !pendingGroupChatId.isEmpty();
}

QString ChatController::getPendingGroupChatId() const
{
    return pendingGroupChatId;
}

void ChatController::clearPendingGroupChatId()
{
    pendingGroupChatId.clear();
}

void ChatController::removeUserFromGroupChat(const QString &chatId, const QString &username)
{
    sendToServer("GROUP_REMOVE_USER:" + chatId + ":" + username);
}

void ChatController::deleteGroupChat(const QString &chatId)
{
    sendToServer("DELETE_GROUP_CHAT:" + chatId);
}

void ChatController::requestUnreadCounts()
{
    sendToServer("GET_UNREAD_COUNTS");
}

void ChatController::requestUnreadCountForUser(const QString &username)
{
    sendToServer("GET_UNREAD_COUNT:" + username);
}

void ChatController::requestUnreadCountForGroupChat(const QString &chatId)
{
    sendToServer("GET_UNREAD_COUNT:" + chatId);
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

bool ChatController::isMessageDuplicate(const QString &chatId, const QString &content, bool isGroup)
{
    Q_UNUSED(content); // Содержимое не проверяем - только временные интервалы
    
    // Проверяем только временные интервалы для предотвращения технических дубликатов
    // Не проверяем содержимое - пользователь может отправить одинаковые сообщения
    static QMap<QString, qint64> lastMessageTimes;
    QString key = (isGroup ? "GROUP_" : "PRIVATE_") + chatId;
    
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    
    if (lastMessageTimes.contains(key)) {
        qint64 lastTime = lastMessageTimes[key];
        qint64 timeDiff = currentTime - lastTime;
        
        // Считаем дубликатом только если сообщения пришли слишком быстро (менее 100мс)
        // Это защищает от технических дубликатов, но позволяет отправлять одинаковые сообщения
        if (timeDiff < 100) {
            qDebug() << "Technical duplicate detected for" << chatId 
                    << "time diff:" << timeDiff << "ms - likely network duplicate";
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

