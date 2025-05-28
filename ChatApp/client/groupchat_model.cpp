#include "groupchat_model.h"

GroupChatModel::GroupChatModel(const QString &chatId, const QString &chatName, const QString &currentUser, QObject *parent)
    : QObject(parent),
      m_chatId(chatId),
      m_chatName(chatName),
      m_currentUser(currentUser),
      m_unreadCount(0)
{
}

GroupChatModel::~GroupChatModel()
{
}

// Геттеры
QString GroupChatModel::getChatId() const
{
    return m_chatId;
}

QString GroupChatModel::getChatName() const
{
    return m_chatName;
}

QString GroupChatModel::getCurrentUser() const
{
    return m_currentUser;
}

QString GroupChatModel::getCreator() const
{
    return m_creator;
}

QList<GroupMessage> GroupChatModel::getMessages() const
{
    return m_messages;
}

QStringList GroupChatModel::getMembers() const
{
    return m_members;
}

int GroupChatModel::getUnreadCount() const
{
    return m_unreadCount;
}

bool GroupChatModel::isCurrentUserCreator() const
{
    return m_creator == m_currentUser;
}

// Методы проверки возможности действий
bool GroupChatModel::canRemoveMember(const QString &username) const
{
    // Только создатель может удалять участников
    if (!isCurrentUserCreator()) {
        return false;
    }
    
    // Создателя нельзя удалить
    if (username == m_creator) {
        return false;
    }
    
    // Пользователь должен быть в списке участников
    if (!m_members.contains(username)) {
        return false;
    }
    
    return true;
}

// Методы для работы с сообщениями
void GroupChatModel::addMessage(const QString &sender, const QString &content, const QString &timestamp)
{
    if (!m_messages.isEmpty()) {
        const GroupMessage &lastMessage = m_messages.last();
        if (lastMessage.sender == sender && 
            lastMessage.timestamp == timestamp) {
            qDebug() << "Duplicate message ignored (same timestamp):" << sender << timestamp;
            return;
        }
    }
    bool isFromCurrentUser = (sender == m_currentUser);
    GroupMessage message(sender, content, timestamp, isFromCurrentUser);
    addMessage(message);
}

void GroupChatModel::addMessage(const GroupMessage &message)
{
    m_messages.append(message);
    
    // Обновляем счетчик непрочитанных, если сообщение не от текущего пользователя и не прочитано
    if (message.sender != m_currentUser && !message.isRead) {
        m_unreadCount++;
        emit unreadCountChanged(m_unreadCount);
    }
    
    emit messageAdded(message);
}

void GroupChatModel::setMessageHistory(const QList<GroupMessage> &messages)
{
    m_messages = messages;
    // Важно: вызываем updateUnreadCount() для обновления счетчика непрочитанных
    // сообщений на основе признака isRead в сообщениях
    updateUnreadCount();
    emit messagesCleared(); // Сигнализируем об обновлении истории
    qDebug() << "Message history set for group chat" << m_chatName << ":" 
             << messages.size() << "messages, unread count:" << m_unreadCount;
}

void GroupChatModel::clearMessages()
{
    m_messages.clear();
    m_unreadCount = 0;
    emit messagesCleared();
    emit unreadCountChanged(m_unreadCount);
    qDebug() << "Messages cleared for group chat" << m_chatName;
}

// Методы для работы с участниками
void GroupChatModel::setMembers(const QStringList &members)
{
    m_members = members;
    emit membersUpdated(members);
    qDebug() << "Members updated for group chat" << m_chatName << ":" << members;
}

void GroupChatModel::addMember(const QString &member)
{
    if (!m_members.contains(member)) {
        m_members.append(member);
        emit membersUpdated(m_members);
        qDebug() << "Member added to group chat" << m_chatName << ":" << member;
    }
}

void GroupChatModel::removeMember(const QString &member)
{
    if (m_members.removeOne(member)) {
        emit membersUpdated(m_members);
        qDebug() << "Member removed from group chat" << m_chatName << ":" << member;
    }
}

// Методы для работы с создателем
void GroupChatModel::setCreator(const QString &creator)
{
    if (m_creator != creator) {
        m_creator = creator;
        emit creatorChanged(creator);
        qDebug() << "Creator set for group chat" << m_chatName << ":" << creator;
    }
}

// Методы для работы с непрочитанными сообщениями
void GroupChatModel::markAllAsRead()
{
    bool hasUnread = false;
    for (auto &message : m_messages) {
        if (!message.isRead && message.sender != m_currentUser) {
            message.isRead = true;
            hasUnread = true;
        }
    }
    
    if (hasUnread || m_unreadCount > 0) {
        m_unreadCount = 0;
        emit unreadCountChanged(m_unreadCount);
        qDebug() << "All messages marked as read for group chat" << m_chatName;
    }
}

void GroupChatModel::setUnreadCount(int count)
{
    if (m_unreadCount != count) {
        m_unreadCount = count;
        emit unreadCountChanged(count);
        qDebug() << "Unread count set for group chat" << m_chatName << ":" << count;
    }
}

// Методы для работы с названием чата
void GroupChatModel::setChatName(const QString &name)
{
    if (m_chatName != name) {
        m_chatName = name;
        emit chatNameChanged(name);
        qDebug() << "Chat name changed from" << m_chatName << "to" << name;
    }
}

// Метод для обновления текущего пользователя
void GroupChatModel::setCurrentUser(const QString &username)
{
    if (m_currentUser != username) {
        qDebug() << "Group chat" << m_chatName << "current user changed from" << m_currentUser << "to" << username;
        m_currentUser = username;
        // После изменения текущего пользователя обновляем счетчик непрочитанных
        updateUnreadCount();
    }
}

// Вспомогательные методы
void GroupChatModel::updateUnreadCount()
{
    int count = 0;
    for (const auto &message : m_messages) {
        // Учитываем только непрочитанные сообщения от других пользователей
        if (!message.isRead && message.sender != m_currentUser) {
            count++;
        }
    }
    
    // Отправляем сигнал только при изменении счетчика
    if (m_unreadCount != count) {
        qDebug() << "Group chat" << m_chatName << "unread count changed from" << m_unreadCount << "to" << count;
        m_unreadCount = count;
        emit unreadCountChanged(count);
    }
}
