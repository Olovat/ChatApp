#ifndef CHAT_CONTROLLER_H
#define CHAT_CONTROLLER_H

#include <QObject>
#include <QTcpSocket>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QList>
#include <QSet>
#include <QTimer>


class MainWindow;
class PrivateChatWindow;
class GroupChatWindow;

// Структура для хранения непрочитанных сообщений
struct UnreadMessage {
    QString sender;
    QString message;
    QString timestamp;
};

// Контроллер для обработки логики чата
class ChatController : public QObject
{
    Q_OBJECT

public:
    enum Operation {
        None,
        Auth,
        Register
    };

    explicit ChatController(QObject *parent = nullptr);
    ~ChatController();

    // Методы подключения и отключения
    bool connectToServer(const QString& host, quint16 port);
    void disconnectFromServer();
    bool isConnected() const;
    bool reconnectToServer(); // Метод для переподключения существующего контроллера

    // Методы авторизации и регистрации
    void authorizeUser(const QString &username, const QString &password);
    void registerUser(const QString &username, const QString &password);
    bool isLoginSuccessful() const;
    
    // Методы для управления пользователями
    void requestUserList();
    void searchUsers(const QString &query);
    void addUserToFriends(const QString &username);
    QStringList getOnlineUsers() const;
    QStringList getUserList() const;
    QStringList getDisplayedUsers() const;
    
    // Методы для групповых чатов
    void createGroupChat(const QString &chatName, const QString &chatId);
    void joinGroupChat(const QString &chatId);
    void startAddUserToGroupMode(const QString &chatId);
    void addUserToGroupChat(const QString &chatId, const QString &username);
    void removeUserFromGroupChat(const QString &chatId, const QString &username);
    void deleteGroupChat(const QString &chatId);    // Методы для обработки непрочитанных сообщений
    void requestUnreadCounts();
    void requestUnreadCountForUser(const QString &username);
    void markMessagesAsRead(const QString &username);
    
    // Метод для отправки произвольных сообщений на сервер
    void sendMessageToServer(const QString &message);
    
    // Методы для установки и получения текущего пользователя
    void setCurrentUser(const QString &username, const QString &password);
    QString getCurrentUsername() const;
    
    // Методы для тестирования
    void testAuthorizeUser(const QString& username, const QString& password);
    bool testRegisterUser(const QString& username, const QString& password);
    QStringList getLastSentMessages() const;

    // Проверка валидности объекта контроллера
    bool isValid() const { 
        return socket && socket->state() == QAbstractSocket::ConnectedState;
    }
      // Методы для работы с друзьями
    bool isFriend(const QString &username) const;
    QStringList getFriendList() const;

signals:
    // Сигналы для UI
    void connectionEstablished();
    void connectionFailed(const QString &errorMessage);
    void authenticationSuccessful();
    void authenticationFailed(const QString &errorMessage);
    void registrationSuccessful();
    void registrationFailed(const QString &errorMessage);
    void userListUpdated(const QStringList &users);
    void searchResultsReady(const QStringList &users);
    void groupMessageReceived(const QString &chatId, const QString &sender, const QString &message, const QString &timestamp);
    void groupHistoryReceived(const QString &chatId, const QList<QPair<QString, QString>> &history);
    void groupMembersUpdated(const QString &chatId, const QStringList &members, const QString &creator);
    void unreadCountsUpdated(const QMap<QString, int> &privateCounts, const QMap<QString, int> &groupCounts);
    void friendAddedSuccessfully(const QString &username);
    void privateMessageReceived(const QString &sender, const QString &message, const QString &timestamp);
    void privateHistoryReceived(const QString &username, const QStringList &messages);
    void privateMessageStored(const QString &sender, const QString &recipient, const QString &message, const QString &timestamp);
    void friendStatusUpdated(const QString &username, bool isOnline); // Добавляем сигнал
    void historyRequestStarted(const QString &username); // Добавляем сигнал

public slots:
    void sendPrivateMessage(const QString &recipient, const QString &message);
    void requestPrivateMessageHistory(const QString &username);
    void storePrivateMessage(const QString &sender, const QString &recipient, const QString &message, const QString &timestamp);
    void requestRecentChatPartners(); // Добавляем метод для запроса недавних собеседников

private slots: 
    void handleSocketReadyRead();
    void handleSocketError(QAbstractSocket::SocketError socketError);
    void handleAuthenticationTimeout();
    void onPollNewFriendStatus();
    void refreshUserListSlot();

private:
    QTcpSocket *socket;
    QString username;
    QString password;
    bool loginSuccessful;
    QByteArray buffer;
    quint16 nextBlockSize;
    QStringList recentSentMessages;
    QStringList userList;
    QStringList onlineUsers;
    QStringList searchResults;
    QStringList lastSearchResults;
    QMap<QString, int> unreadPrivateMessageCounts;
    QMap<QString, int> unreadGroupMessageCounts;
    QStringList friendList;
    QSet<QString> recentChatPartners;
    QString pendingGroupChatId;
    QTimer *authTimeoutTimer;
    Operation currentOperation;

    // Добавляем поля для сбора истории сообщений
    QStringList historyBuffer;
    QString currentHistoryTarget;

    QTimer *newFriendStatusPollTimer;     
    QString currentlyPollingFriend;        
    int newFriendPollAttempts;             

    QTimer *userListRefreshTimer;          

    QMap<QString, QString> lastPrivateChatTimestamps;
    QMap<QString, QString> lastGroupChatTimestamps;

    void sendToServer(const QString &message);
    void processServerResponse(const QString &response);
    void clearSocketBuffer();
    bool isMessageDuplicate(const QString &chatId, const QString &timestamp, bool isGroup);
    void startPollingForFriendStatus(const QString& username);
    void stopPollingForFriendStatus();
};

#endif // CHAT_CONTROLLER_H
