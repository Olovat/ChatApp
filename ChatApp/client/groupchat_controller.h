#ifndef GROUPCHAT_CONTROLLER_H
#define GROUPCHAT_CONTROLLER_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QList>
#include <QDateTime>
#include <QUuid>
#include "groupchat_model.h" // Подключаем заголовочный файл с моделью

class MainWindowController;
class ChatController;
class GroupChatWindow;

// Контроллер для групповых чатов
class GroupChatController : public QObject
{
    Q_OBJECT

public:
    GroupChatController(MainWindowController *mainController, QObject *parent = nullptr);
    ~GroupChatController();

    // Основные методы управления окнами чатов
    GroupChatWindow* findOrCreateChatWindow(const QString &chatId, const QString &chatName);
    QStringList getActiveChatIds() const;
    int getActiveChatsCount() const;
    void setCurrentUsername(const QString &username);
    QString getCurrentUsername() const;
    
    // Методы для работы с чатами
    void createGroupChat(const QString &chatName);
    void joinGroupChat(const QString &chatId);
    void deleteGroupChat(const QString &chatId);
    
    // Методы для работы с участниками
    void addUserToGroupChat(const QString &chatId, const QString &username);
    void removeUserFromGroupChat(const QString &chatId, const QString &username);
    void startAddUserToGroupMode(const QString &chatId);
    
    // Методы для работы с сообщениями
    void markMessagesAsRead(const QString &chatId);
    
    // Доступ к ChatController
    ChatController* getChatController() const;

public slots:
    // Обработка входящих сообщений и обновлений
    void handleIncomingMessage(const QString &chatId, const QString &sender, const QString &message, const QString &timestamp = QString());
    void handleMessageHistory(const QString &chatId, const QList<QPair<QString, QString>> &history);
    void handleMembersUpdated(const QString &chatId, const QStringList &members, const QString &creator);
    void handleCreatorUpdated(const QString &chatId, const QString &creator);
    void handleChatDeleted(const QString &chatId);
    
    // Обработка действий пользователя
    void sendMessage(const QString &chatId, const QString &message);
    void requestMessageHistory(const QString &chatId);
    void requestGroupChatInfo(const QString &chatId);
    void handleChatWindowClosed(const QString &chatId);
    
    // Обработка управления участниками
    void handleAddUserToGroup(const QString &chatId, const QString &username);
    void handleRemoveUserFromGroup(const QString &chatId, const QString &username);
    void handleDeleteGroupChat(const QString &chatId);

signals:
    void messageSent(const QString &chatId, const QString &message);
    void chatCreationRequested(const QString &chatName);
    void chatJoinRequested(const QString &chatId);
    void userAdditionRequested(const QString &chatId, const QString &username);
    void userRemovalRequested(const QString &chatId, const QString &username);
    void chatDeletionRequested(const QString &chatId);
    void addUserModeRequested(const QString &chatId);

private:
    MainWindowController *m_mainController;
    ChatController *m_chatController;
    QString m_currentUsername;
    QMap<QString, GroupChatWindow*> m_chatWindows;
    QMap<QString, GroupChatModel*> m_chatModels;
    
    // Вспомогательные методы
    GroupChatModel* findOrCreateChatModel(const QString &chatId, const QString &chatName);
    void setupConnectionsForWindow(GroupChatWindow *window, const QString &chatId);
    void parseMessageHistory(const QString &chatId, const QList<QPair<QString, QString>> &history);
    QString generateChatId() const;
};

#endif // GROUPCHAT_CONTROLLER_H
