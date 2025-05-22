#ifndef MAINWINDOW_CONTROLLER_H
#define MAINWINDOW_CONTROLLER_H

#include <QObject>
#include "chat_controller.h"
#include "mainwindow.h"

// Контроллер для MainWindow, объединяющий ChatController и MainWindow
class MainWindowController : public QObject
{
    Q_OBJECT

public:
    explicit MainWindowController(MainWindow *view, QObject *parent = nullptr);
    ~MainWindowController();

    // Доступ к чат-контроллеру
    ChatController* getChatController() const;

public slots:
    // Слоты для обработки действий пользователя из MainWindow
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
    void handleUserSearch(const QString &query);
    void handleAddUserToFriends(const QString &username);
    void handleRequestPrivateMessageHistory(const QString &username); // Новый метод
    
    // Слоты для обработки событий от ChatController
    void handleAuthenticationSuccessful();
    void handleAuthenticationFailed(const QString &errorMessage);
    void handleRegistrationSuccessful();
    void handleRegistrationFailed(const QString &errorMessage);
    void handleUserListUpdated(const QStringList &users);
    void handleSearchResultsReady(const QStringList &users);
    void handlePrivateMessageReceived(const QString &sender, const QString &message, const QString &timestamp);
    void handleGroupMessageReceived(const QString &chatId, const QString &sender, const QString &message, const QString &timestamp);
    void handleGroupMembersUpdated(const QString &chatId, const QStringList &members, const QString &creator);
    void handleUnreadCountsUpdated(const QMap<QString, int> &privateCounts, const QMap<QString, int> &groupCounts);

private:
    MainWindow *view;
    ChatController *chatController;
    
    // Вспомогательные методы
    void setupViewConnections();
    void setupControllerConnections();
};

#endif // MAINWINDOW_CONTROLLER_H
