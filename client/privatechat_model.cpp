#include "privatechat_model.h"

PrivateChatModel::PrivateChatModel(const QString &currentUser, const QString &peerUser, QObject *parent)
    : QObject(parent), m_currentUser(currentUser), m_peerUser(peerUser), m_peerOnline(false), m_unreadCount(0)
{
    qDebug() << "Created private chat model for conversation with" << peerUser;
}

PrivateChatModel::~PrivateChatModel()
{
    qDebug() << "Destroyed private chat model for conversation with" << m_peerUser;
}

QString PrivateChatModel::getCurrentUser() const
{
    return m_currentUser;
}

QString PrivateChatModel::getPeerUser() const
{
    return m_peerUser;
}

QList<PrivateMessage> PrivateChatModel::getMessages() const
{
    return m_messages;
}

bool PrivateChatModel::isPeerOnline() const
{
    return m_peerOnline;
}

int PrivateChatModel::getUnreadCount() const
{
    return m_unreadCount;
}

void PrivateChatModel::addMessage(const QString &sender, const QString &content, const QString &timestamp)
{
    PrivateMessage message(sender, content, timestamp, sender == m_currentUser);
    addMessage(message);
}

void PrivateChatModel::addMessage(const PrivateMessage &message)
{
    m_messages.append(message);
    
    if (message.sender != m_currentUser && !message.isRead) {
        m_unreadCount++;
        emit unreadCountChanged(m_unreadCount);
    }
    
    emit messageAdded(message);
    emit messagesUpdated();
}

void PrivateChatModel::setMessageHistory(const QList<PrivateMessage> &messages)
{
    m_messages = messages;
    updateUnreadCount();
    emit messagesUpdated();
}

void PrivateChatModel::markAllMessagesAsRead()
{
    bool changed = false;
    
    for (int i = 0; i < m_messages.size(); ++i) {
        if (!m_messages[i].isRead && m_messages[i].sender != m_currentUser) {
            m_messages[i].isRead = true;
            changed = true;
        }
    }
    
    if (changed) {
        m_unreadCount = 0;
        emit unreadCountChanged(m_unreadCount);
        emit messagesUpdated();
    }
}

void PrivateChatModel::setPeerStatus(bool isOnline)
{
    if (m_peerOnline != isOnline) {
        m_peerOnline = isOnline;
        emit peerStatusChanged(isOnline);
    }
}

void PrivateChatModel::updateUnreadCount()
{
    int oldCount = m_unreadCount;
    m_unreadCount = 0;
    
    for (const PrivateMessage &message : m_messages) {
        if (message.sender != m_currentUser && !message.isRead) {
            m_unreadCount++;
        }
    }
    
    if (oldCount != m_unreadCount) {
        emit unreadCountChanged(m_unreadCount);
    }
}

void PrivateChatModel::incrementUnreadCount()
{
    m_unreadCount++;
    emit unreadCountChanged(m_unreadCount);
}
