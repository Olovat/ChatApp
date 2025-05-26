#ifndef GROUPCHAT_MODEL_H
#define GROUPCHAT_MODEL_H

#include <QObject>
#include <QString>
#include <QList>
#include <QStringList>
#include <QDateTime>
#include <QDebug>

/**
 * @brief Структура для хранения сообщения группового чата
 */
struct GroupMessage {
    QString sender;
    QString content;
    QString timestamp;
    bool isRead;
    
    // Основной конструктор с явным указанием статуса прочитанности
    GroupMessage(const QString &sender, const QString &content, const QString &timestamp, bool isRead = false)
        : sender(sender), content(content), timestamp(timestamp), isRead(isRead) {}
        
    // Дополнительный конструктор для совместимости
    GroupMessage(const QString &sender, const QString &content, const QString &timestamp, bool isFromCurrentUser, bool forceRead)
        : sender(sender), content(content), timestamp(timestamp), isRead(isFromCurrentUser || forceRead) {}
};

/**
 * @brief Модель данных для группового чата (Model в MVC)
 * 
 * Хранит данные о сообщениях чата, участниках, статусе создателя
 */
class GroupChatModel : public QObject
{
    Q_OBJECT
    
public:
    explicit GroupChatModel(const QString &chatId, const QString &chatName, const QString &currentUser, QObject *parent = nullptr);
    ~GroupChatModel();
    
    // Геттеры
    QString getChatId() const;
    QString getChatName() const;
    QString getCurrentUser() const;
    QString getCreator() const;
    QList<GroupMessage> getMessages() const;
    QStringList getMembers() const;
    int getUnreadCount() const;
    bool isCurrentUserCreator() const;
    
    // Методы для работы с сообщениями
    void addMessage(const QString &sender, const QString &content, const QString &timestamp);
    void addMessage(const GroupMessage &message);
    void setMessageHistory(const QList<GroupMessage> &messages);
    void clearMessages();
    
    // Методы для работы с участниками
    void setMembers(const QStringList &members);
    void addMember(const QString &member);
    void removeMember(const QString &member);
    
    // Методы для работы с создателем
    void setCreator(const QString &creator);
    
    // Методы для работы с непрочитанными сообщениями
    void markAllAsRead();
    void setUnreadCount(int count);
    
    // Методы для работы с названием чата
    void setChatName(const QString &name);
    
signals:
    void messageAdded(const GroupMessage &message);
    void messagesCleared();
    void membersUpdated(const QStringList &members);
    void creatorChanged(const QString &creator);
    void unreadCountChanged(int count);
    void chatNameChanged(const QString &name);
    
private:
    QString m_chatId;
    QString m_chatName;
    QString m_currentUser;
    QString m_creator;
    QList<GroupMessage> m_messages;
    QStringList m_members;
    int m_unreadCount;
    
    // Вспомогательные методы
    void updateUnreadCount();
};

#endif // GROUPCHAT_MODEL_H
