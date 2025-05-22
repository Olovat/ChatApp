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
      newFriendPollAttempts(0), 
      mainWindowRef(nullptr)
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

void ChatController::setMainWindow(MainWindow *mainWindow)
{
    mainWindowRef = mainWindow;
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
                if (socket->bytesAvailable() < sizeof(quint16)) {
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
            
            QStringList userDetails = entry.split(':');
            if (userDetails.size() == 3) { 
                QString username = userDetails[0];
                QString status_str = userDetails[1];
                QString type_from_server = userDetails[2]; 

                bool isOnline = (status_str == "1" || status_str.compare("online", Qt::CaseInsensitive) == 0 || status_str.toInt() > 0);


                QString formattedUser = username + ":" + (isOnline ? "1" : "0") + ":" + type_from_server;
                
                tempUserList.append(formattedUser);
                qDebug() << "ChatController parsed user entry:" << formattedUser;

                if (isOnline) {
                    tempOnlineUsernames.append(username); 
                }
            } else {
                qDebug() << "ChatController: Malformed user entry in USERLIST:" << entry << "(expected 3 parts, got" << userDetails.size() << ")";
            }
        }
        
 
        this->userList = tempUserList; 
        this->onlineUsers = tempOnlineUsernames;
        
        qDebug() << "ChatController emitting userListUpdated with" << this->userList.size() << "total users processed. Online usernames:" << this->onlineUsers.size();
        emit userListUpdated(this->userList); 

        if (!currentlyPollingFriend.isEmpty() && !this->onlineUsers.filter(currentlyPollingFriend, Qt::CaseInsensitive).isEmpty()) {
            qDebug() << "New friend" << currentlyPollingFriend << "is now reported as online. Stopping poll.";
            stopPollingForFriendStatus();
        }
    } else if (command == "SEARCH_RESULTS") {
        // Предотвращаем дублирование результатов поиска с одинаковым содержимым
        QStringList currentResults;
        
        for (int i = 1; i < parts.size(); ++i) {
            // Добавляем пользователей в список результатов
            currentResults.append(parts[i]);
        }
        
        // Если список пуст, не отправляем его дальше
        if (currentResults.isEmpty()) {
            qDebug() << "Получены пустые результаты поиска";
            emit searchResultsReady(QStringList());
            return;
        }
        
        // Проверяем, не совпадают ли текущие результаты с предыдущими
        if (currentResults != lastSearchResults) {
            qDebug() << "Получены новые результаты поиска:" << currentResults;
            lastSearchResults = currentResults;
            searchResults = currentResults;
            emit searchResultsReady(searchResults);
        } else {
            qDebug() << "Игнорирование дублированных результатов поиска:" << currentResults;
        }
    }
    else if (command == "PRIVATE_MSG") {
        if (parts.size() >= 3) {
            QString sender = parts[1];
            QString message = parts[2];
            QString timestamp = parts.size() > 3 ? parts[3] : QString();
            
            // Проверка на дубликаты сообщений
            if (!isMessageDuplicate(sender, timestamp, false)) {
                // Сохраняем timestamp сообщения
                if (!timestamp.isEmpty()) {
                    lastPrivateChatTimestamps[sender] = timestamp;
                }
                
                // Увеличиваем счетчик непрочитанных сообщений
                unreadPrivateMessageCounts[sender] = unreadPrivateMessageCounts.value(sender, 0) + 1;
                
                // Добавляем отправителя в список недавних собеседников
                recentChatPartners.insert(sender);
                
                // Отправляем сигнал о получении сообщения
                emit privateMessageReceived(sender, message, timestamp);
                
                // Сообщаем об обновлении счетчиков непрочитанных сообщений
                emit unreadCountsUpdated(unreadPrivateMessageCounts, unreadGroupMessageCounts);
            }
        }
    }
    else if (command == "GROUP_MSG") {
        if (parts.size() >= 4) {
            QString chatId = parts[1];
            QString sender = parts[2];
            QString message = parts[3];
            QString timestamp = parts.size() > 4 ? parts[4] : QString();
            
            // Проверка на дубликаты сообщений
            if (!isMessageDuplicate(chatId, timestamp, true)) {
                // Сохраняем timestamp
                if (!timestamp.isEmpty()) {
                    lastGroupChatTimestamps[chatId] = timestamp;
                }
                
                // Увеличиваем счетчик непрочитанных сообщений
                unreadGroupMessageCounts[chatId] = unreadGroupMessageCounts.value(chatId, 0) + 1;
                
                // Отправляем сигнал о получении группового сообщения
                emit groupMessageReceived(chatId, sender, message, timestamp);
                
                // Сообщаем об обновлении счетчиков непрочитанных сообщений
                emit unreadCountsUpdated(unreadPrivateMessageCounts, unreadGroupMessageCounts);
            }
        }
    }
    else if (command == "GROUP_MEMBERS") {
        if (parts.size() >= 3) {
            QString chatId = parts[1];
            QString creator = parts[2];
            QStringList members;
            
            for (int i = 3; i < parts.size(); ++i) {
                members.append(parts[i]);
            }
            
            emit groupMembersUpdated(chatId, members, creator);
        }
    }
    else if (command == "UNREAD_COUNTS") {
        unreadPrivateMessageCounts.clear();
        unreadGroupMessageCounts.clear();
        
        int privateCount = parts.size() > 1 ? parts[1].toInt() : 0;
        int groupCount = parts.size() > 2 ? parts[2].toInt() : 0;
        
        for (int i = 0; i < privateCount && (3 + i*2 + 1) < parts.size(); i++) {
            QString user = parts[3 + i*2];
            int count = parts[3 + i*2 + 1].toInt();
            unreadPrivateMessageCounts[user] = count;
        }
        
        int groupOffset = 3 + privateCount * 2;
        for (int i = 0; i < groupCount && (groupOffset + i*2 + 1) < parts.size(); i++) {
            QString chatId = parts[groupOffset + i*2];
            int count = parts[groupOffset + i*2 + 1].toInt();
            unreadGroupMessageCounts[chatId] = count;
        }
        
        emit unreadCountsUpdated(unreadPrivateMessageCounts, unreadGroupMessageCounts);
    }    else if (command == "FRIEND_ADDED") {
        // Обработка успешного добавления друга
        if (parts.size() > 1) {
            QString addedUsername = parts[1];
            qDebug() << "Friend added successfully:" << addedUsername;
            
            // Сохраняем ID друга в перманентном списке друзей
            if (!friendList.contains(addedUsername)) {
                friendList.append(addedUsername);
                  // Сохраняем в QSettings для сохранения между сессиями
                saveFriendList();
                
                qDebug() << "Added" << addedUsername << "to permanent friend list. Total friends:" << friendList.size();
            }
            
            // Уведомляем о том, что друг успешно добавлен
            emit friendAddedSuccessfully(addedUsername);

            startPollingForFriendStatus(addedUsername);
        }
    }
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

void ChatController::sendPrivateMessage(const QString &recipient, const QString &message)
{
    sendToServer("SEND_PRIVATE:" + recipient + ":" + message);
    recentChatPartners.insert(recipient);
}

void ChatController::requestPrivateMessageHistory(const QString &otherUser)
{
    sendToServer("GET_PRIVATE_HISTORY:" + otherUser);
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

void ChatController::setCurrentUser(const QString &username, const QString &password)
{
    this->username = username;
    this->password = password;
}

QString ChatController::getCurrentUsername() const
{
    return username;
}

bool ChatController::isLoginSuccessful() const
{
    return loginSuccessful;
}

QStringList ChatController::getOnlineUsers() const
{
    return onlineUsers;
}

QStringList ChatController::getUserList() const
{
    return userList;
}

QStringList ChatController::getDisplayedUsers() const
{
    QStringList displayed = userList;
    
    // Добавляем недавних собеседников, даже если их нет в основном списке
    for (const QString &user : recentChatPartners) {
        if (!displayed.contains(user)) {
            displayed.append(user);
        }
    }
    
    return displayed;
}

QStringList ChatController::getLastSentMessages() const
{
    return recentSentMessages;
}

bool ChatController::hasPrivateChatWith(const QString &username) const
{
    if (mainWindowRef) {
        return mainWindowRef->hasPrivateChatWith(username);
    }
    return false;
}

int ChatController::privateChatsCount() const
{
    if (mainWindowRef) {
        return mainWindowRef->privateChatsCount();
    }
    return 0;
}

QStringList ChatController::privateChatParticipants() const
{
    if (mainWindowRef) {
        return mainWindowRef->privateChatParticipants();
    }
    return QStringList();
}

void ChatController::testAuthorizeUser(const QString& username, const QString& password)
{
    this->username = username;
    this->password = password;
    loginSuccessful = true;
    emit authenticationSuccessful();
}

bool ChatController::testRegisterUser(const QString& username, const QString& password)
{
    this->username = username;
    this->password = password;
    emit registrationSuccessful();
    return true;
}

bool ChatController::isMessageDuplicate(const QString &chatId, const QString &timestamp, bool isGroup)
{
    if (timestamp.isEmpty()) {
        return false;
    }
    
    if (isGroup) {
        if (lastGroupChatTimestamps.contains(chatId) && lastGroupChatTimestamps[chatId] == timestamp) {
            return true;
        }
    } else {
        if (lastPrivateChatTimestamps.contains(chatId) && lastPrivateChatTimestamps[chatId] == timestamp) {
            return true;
        }
    }
    
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
    newFriendStatusPollTimer->start();
    onPollNewFriendStatus(); 
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
