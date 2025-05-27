#ifndef MAINWINDOW_CONTROLLER_H
#define MAINWINDOW_CONTROLLER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>

// Предварительные объявления классов
class MainWindow;
class ChatController;
class PrivateChatController;
class GroupChatController;

// Контроллер для MainWindow, объединяющий ChatController и MainWindow
class MainWindowController : public QObject
{
    Q_OBJECT

public:
    explicit MainWindowController(MainWindow *view, QObject *parent = nullptr);
    ~MainWindowController();

    // Доступ к чат-контроллерам
    ChatController* getChatController() const;
    PrivateChatController* getPrivateChatController() const { return privateChatController; }
    GroupChatController* getGroupChatController() const { return groupChatController; }

public slots:
    // Обработчики для сигналов от MainWindow
    void handleAuthorizeUser(const QString &username, const QString &password);
    void handleRegisterUser(const QString &username, const QString &password);
    void handleUserSelected(const QString &username);
    void handlePrivateMessageSend(const QString &recipient, const QString &message);
    void handleGroupMessageSend(const QString &chatId, const QString &message);
    void handleCreateGroupChat(const QString &chatName);
    void handleJoinGroupChat(const QString &chatId);
    void handleAddUserToGroup(const QString &chatId, const QString &username);
    void handleRemoveUserFromGroup(const QString &chatId, const QString &username);
    void handleDeleteGroupChat(const QString &chatId);
    void handleStartAddUserToGroupMode(const QString &chatId);
    void handleUserSearch(const QString &query);
    void handleAddUserToFriends(const QString &username);
    void handleUserDoubleClicked(const QString &username);
    void handleGroupChatSelected(const QString &chatId);

    // Обработчики для сигналов от ChatController
    void handleAuthenticationSuccessful();
    void handleAuthenticationFailed(const QString &errorMessage);
    void handleRegistrationSuccessful();
    void handleRegistrationFailed(const QString &errorMessage);
    void handleUserListUpdated(const QStringList &users);
    void handleSearchResultsReady(const QStringList &users);    void handleGroupMessageReceived(const QString &chatId, const QString &sender, const QString &message, const QString &timestamp);
    void handleGroupMembersUpdated(const QString &chatId, const QStringList &members, const QString &creator);
    void handleGroupCreatorUpdated(const QString &chatId, const QString &creator);
    void handleGroupChatCreated(const QString &chatId, const QString &chatName);
    void handleUnreadCountsUpdated(const QMap<QString, int> &privateCounts, const QMap<QString, int> &groupCounts);

private:
    MainWindow *view;
    ChatController *chatController;
    PrivateChatController *privateChatController;
    GroupChatController *groupChatController;

    // Вспомогательные методы
    void setupViewConnections();
    void setupControllerConnections();
};

#endif // MAINWINDOW_CONTROLLER_H
