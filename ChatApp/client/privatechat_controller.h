#ifndef PRIVATECHAT_CONTROLLER_H
#define PRIVATECHAT_CONTROLLER_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QList>
#include <QDateTime>
#include "privatechat_model.h" // Подключаем заголовочный файл с моделью

class MainWindowController;
class ChatController;
class PrivateChatWindow;

// Контроллер для приватных чатов
class PrivateChatController : public QObject
{
    Q_OBJECT

public:
    PrivateChatController(MainWindowController *mainController, QObject *parent = nullptr);
    ~PrivateChatController();

    PrivateChatWindow* findOrCreateChatWindow(const QString &username);
    QStringList getActiveChatUsernames() const;
    int getActiveChatsCount() const;
    void updateUserStatus(const QString &username, bool isOnline);
    void updateAllUserStatuses(const QMap<QString, bool> &userStatusMap);
    void setCurrentUsername(const QString &username);
    QString getCurrentUsername() const;

public slots:
    void handleIncomingMessage(const QString &sender, const QString &message, const QString &timestamp = QString());
    void handleMessageHistory(const QString &username, const QStringList &messages);
    void sendMessage(const QString &recipient, const QString &message);
    void requestMessageHistory(const QString &username);
    void markMessagesAsRead(const QString &username);
    void handleChatWindowClosed(const QString &username);

signals:
    void messageSent(const QString &recipient, const QString &message);

private:
    MainWindowController *m_mainController;
    ChatController *m_chatController;
    QString m_currentUsername;
    QMap<QString, PrivateChatWindow*> m_chatWindows;
    QMap<QString, PrivateChatModel*> m_chatModels;

    PrivateChatModel* findOrCreateChatModel(const QString &username);    void setupConnectionsForWindow(PrivateChatWindow *window, const QString &username);
    void parseMessageHistory(const QString &username, const QStringList &messages);
};

#endif // PRIVATECHAT_CONTROLLER_H
