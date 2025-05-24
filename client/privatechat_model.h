#ifndef PRIVATECHAT_MODEL_H
#define PRIVATECHAT_MODEL_H

#include <QObject>
#include <QString>
#include <QList>
#include <QDateTime>
#include <QDebug>

/**
 * @brief Структура для хранения сообщения личного чата
 */
struct PrivateMessage {
    QString sender;
    QString content;
    QString timestamp;
    bool isRead;
    
    PrivateMessage(const QString &sender, const QString &content, const QString &timestamp, bool isRead = false)
        : sender(sender), content(content), timestamp(timestamp), isRead(isRead) {}
};

/**
 * @brief Модель данных для личного чата (Model в MVC)
 * 
 * Хранит данные о сообщениях чата, собеседнике, статусе сообщений
 */
class PrivateChatModel : public QObject
{
    Q_OBJECT
    
public:
    explicit PrivateChatModel(const QString &currentUser, const QString &peerUser, QObject *parent = nullptr);
    ~PrivateChatModel();
    
    // Геттеры
    QString getCurrentUser() const;
    QString getPeerUser() const;
    QList<PrivateMessage> getMessages() const;
    bool isPeerOnline() const;
    int getUnreadCount() const;
    
    // Методы для работы с сообщениями
    void addMessage(const QString &sender, const QString &content, const QString &timestamp);
    void addMessage(const PrivateMessage &message);
    void setMessageHistory(const QList<PrivateMessage> &messages);
    void markAllMessagesAsRead();
    
    // Настройка статуса собеседника
    void setPeerStatus(bool isOnline);
    
    // Добавляем метод для увеличения счетчика непрочитанных сообщений
    void incrementUnreadCount();
    
signals:
    void messageAdded(const PrivateMessage &message);
    void messagesUpdated();
    void peerStatusChanged(bool isOnline);
    void unreadCountChanged(int count);

private:
    QString m_currentUser;
    QString m_peerUser;
    QList<PrivateMessage> m_messages;
    bool m_peerOnline;
    int m_unreadCount;
    
    void updateUnreadCount();
};

#endif // PRIVATECHAT_MODEL_H
