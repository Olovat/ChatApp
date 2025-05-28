#include "groupchat_controller.h"
#include "groupchatwindow.h"
#include "mainwindow_controller.h"
#include "chat_controller.h"
#include <QDebug>
#include <QTimer>
#include <QMessageBox>

GroupChatController::GroupChatController(MainWindowController *mainController, QObject *parent)
    : QObject(parent), m_mainController(mainController)
{
    // Получаем ChatController через MainWindowController
    m_chatController = mainController->getChatController();
    
    // Подключаем сигналы этого контроллера к слотам ChatController
    connect(this, &GroupChatController::messageSent,
            m_chatController, [this](const QString &chatId, const QString &message) {
                m_chatController->sendMessageToServer("GROUP_MESSAGE:" + chatId + ":" + message);
            });
            
    connect(this, &GroupChatController::chatCreationRequested,
            this, [this](const QString &chatName) {
                // Сервер сам генерирует chatId, поэтому передаем только chatName
                m_chatController->createGroupChat(chatName, QString());
            });
            
    connect(this, &GroupChatController::chatJoinRequested,
            m_chatController, &ChatController::joinGroupChat);
            
    connect(this, &GroupChatController::userAdditionRequested,
            m_chatController, &ChatController::addUserToGroupChat);
            
    connect(this, &GroupChatController::userRemovalRequested,
            m_chatController, &ChatController::removeUserFromGroupChat);
            
    connect(this, &GroupChatController::chatDeletionRequested,
            m_chatController, &ChatController::deleteGroupChat);
            
    connect(this, &GroupChatController::addUserModeRequested,
            m_chatController, &ChatController::startAddUserToGroupMode);
            
    qDebug() << "GroupChatController initialized";
}

GroupChatController::~GroupChatController()
{
    // Очищаем модели чатов
    for (auto it = m_chatModels.begin(); it != m_chatModels.end(); ++it) {
        delete it.value();
    }
    m_chatModels.clear();
    
    // Окна чатов будут удалены автоматически, так как у них есть родитель
    m_chatWindows.clear();
    
    qDebug() << "GroupChatController destroyed";
}

// Основные методы управления окнами чатов
GroupChatWindow* GroupChatController::findOrCreateChatWindow(const QString &chatId, const QString &chatName)
{
    // Проверяем, существует ли уже окно для этого чата
    if (m_chatWindows.contains(chatId)) {
        qDebug() << "Returning existing group chat window for" << chatName;
        
        // ИСПРАВЛЕНИЕ: Даже для существующих окон запрашиваем данные как в PrivateChatController
        // Проверяем, есть ли данные в модели
        if (m_chatModels.contains(chatId)) {
            GroupChatModel *model = m_chatModels[chatId];
            if (model->getMessages().isEmpty() || model->getMembers().isEmpty()) {
                qDebug() << "Requesting data for existing group chat window" << chatId;
                // Запрашиваем информацию о чате и историю
                QTimer::singleShot(100, [this, chatId]() {
                    requestGroupChatInfo(chatId);
                    requestMessageHistory(chatId);
                });
            }
        } else {
            // Если модель не существует, обязательно запрашиваем данные
            QTimer::singleShot(100, [this, chatId]() {
                requestGroupChatInfo(chatId);
                requestMessageHistory(chatId);
            });
        }
        
        return m_chatWindows[chatId];
    }
    
    // Создаем новое окно
    GroupChatWindow *window = new GroupChatWindow(chatId, chatName);
    window->setController(m_mainController);
    window->setGroupChatController(this); // Устанавливаем групповой контроллер
    
    // Настраиваем соединения
    setupConnectionsForWindow(window, chatId);
    
    // Сохраняем ссылку на окно
    m_chatWindows[chatId] = window;
    
    // ИСПРАВЛЕНИЕ: Всегда запрашиваем информацию о чате и историю как в PrivateChatController
    qDebug() << "Requesting data for new group chat window" << chatId;
    QTimer::singleShot(100, [this, chatId]() {
        requestGroupChatInfo(chatId);
        requestMessageHistory(chatId);
    });
    
    qDebug() << "Created new group chat window for" << chatName << "with ID" << chatId;
    return window;
}

QStringList GroupChatController::getActiveChatIds() const
{
    return m_chatWindows.keys();
}

int GroupChatController::getActiveChatsCount() const
{
    return m_chatWindows.size();
}

void GroupChatController::setCurrentUsername(const QString &username)
{
    m_currentUsername = username;
    
    for (auto it = m_chatModels.begin(); it != m_chatModels.end(); ++it) {
        GroupChatModel *model = it.value();        if (model) {
            
            model->setCurrentUser(username);
            
            qDebug() << "Updated current user to" << username << "for chat model" << it.key() 
                     << "unread count:" << model->getUnreadCount();
        }
    }
    
    qDebug() << "Current username set to" << username << "in GroupChatController";
}

QString GroupChatController::getCurrentUsername() const
{
    return m_currentUsername;
}

ChatController* GroupChatController::getChatController() const
{
    return m_chatController;
}

// Методы для работы с чатами
void GroupChatController::createGroupChat(const QString &chatName)
{
    emit chatCreationRequested(chatName);
}

void GroupChatController::joinGroupChat(const QString &chatId)
{
    emit chatJoinRequested(chatId);
}

void GroupChatController::deleteGroupChat(const QString &chatId)
{
    emit chatDeletionRequested(chatId);
}

// Методы для работы с участниками
void GroupChatController::addUserToGroupChat(const QString &chatId, const QString &username)
{
    emit userAdditionRequested(chatId, username);
}

void GroupChatController::removeUserFromGroupChat(const QString &chatId, const QString &username)
{
    emit userRemovalRequested(chatId, username);
}

void GroupChatController::startAddUserToGroupMode(const QString &chatId)
{
    emit addUserModeRequested(chatId);
}

// Методы для работы с сообщениями
void GroupChatController::markMessagesAsRead(const QString &chatId)
{
    qDebug() << "GroupChatController: Marking messages as read for chat" << chatId;
    
    if (m_chatModels.contains(chatId)) {
        // Отмечаем сообщения как прочитанные в локальной модели
        m_chatModels[chatId]->markAllAsRead();
        
        // ИСПРАВЛЕНИЕ: Немедленно обновляем счетчик в ChatController (как в приватных чатах)
        if (m_chatController) {
            // Обновляем локальные счетчики в ChatController
            m_chatController->updateUnreadGroupCount(chatId, 0);
            qDebug() << "Immediately updated unread count in ChatController for group chat" << chatId;
        }
        
        // Уведомляем сервер о прочтении
        m_chatController->sendMessageToServer("MARK_GROUP_READ:" + chatId);
        
        // Запрашиваем обновленный счетчик с сервера для подтверждения
        QTimer::singleShot(200, this, [this, chatId]() {
            if (m_chatController) {
                m_chatController->requestUnreadCountForGroupChat(chatId);
                qDebug() << "Requested updated unread count for group chat" << chatId;
            }
        });
        qDebug() << "Marked messages as read and sent update to server for chat" << chatId;
    }
}

// Обработка входящих сообщений и обновлений
void GroupChatController::handleIncomingMessage(const QString &chatId, const QString &sender, const QString &message, const QString &timestamp)
{
    qDebug() << "GroupChatController: Handling incoming message from" << sender << "in chat" << chatId;
    
    // Определяем время сообщения
    QString actualTimestamp = timestamp.isEmpty() ? QDateTime::currentDateTime().toString("hh:mm") : timestamp;
    
    // Получаем или создаём модель для этого чата
    if (!m_chatModels.contains(chatId)) {
        qDebug() << "Creating new model for incoming message from chat" << chatId;
        findOrCreateChatModel(chatId, QString("Групповой чат %1").arg(chatId.left(8)));
    }
    
    GroupChatModel *model = m_chatModels[chatId];
    if (model) {
        // ИСПРАВЛЕНИЕ: Используем упрощенную логику - если окно видимо, считаем сообщение прочитанным
        bool windowIsVisibleAndActive = false;
        if (m_chatWindows.contains(chatId)) {
            GroupChatWindow *window = m_chatWindows[chatId];
            // Упрощаем проверку: если окно видимо и не свернуто, считаем его активным
            windowIsVisibleAndActive = window && 
                                      window->isVisible() && 
                                      !window->isMinimized();
            
            qDebug() << "Window check for chat" << chatId << ": exists=" << (window != nullptr) 
                     << ", visible=" << (window ? window->isVisible() : false)
                     << ", minimized=" << (window ? window->isMinimized() : false)
                     << ", final=" << windowIsVisibleAndActive;
        }
        
        // КЛЮЧЕВОЕ ИСПРАВЛЕНИЕ: Создаем сообщение с правильным статусом прочитанности
        bool isRead = (sender == m_currentUsername) || windowIsVisibleAndActive;
        GroupMessage newMessage(sender, message, actualTimestamp, isRead);
        model->addMessage(newMessage);
        
        // КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: Если окно видимо, немедленно сбрасываем счетчик
        if (windowIsVisibleAndActive) {
            // Помечаем все сообщения как прочитанные в модели
            model->markAllAsRead();
            
            // Немедленно обновляем счетчик в ChatController на 0
            if (m_chatController) {
                m_chatController->updateUnreadGroupCount(chatId, 0);
                qDebug() << "FORCE RESET: Set unread count to 0 for visible group chat" << chatId;
            }
            
            qDebug() << "Marked all messages as read because window is visible for chat" << chatId;
        } else {
            // Только если окно НЕ видимо, запрашиваем обновление счетчика с сервера
            QTimer::singleShot(100, this, [this, chatId]() {
                if (m_chatController) {
                    m_chatController->requestUnreadCountForGroupChat(chatId);
                }
            });
        }
        
        // Отображаем сообщение в окне, если оно существует
        if (m_chatWindows.contains(chatId)) {
            m_chatWindows[chatId]->receiveMessage(sender, message, actualTimestamp);
        }
    }
}

void GroupChatController::handleMessageHistory(const QString &chatId, const QList<QPair<QString, QString>> &history)
{
    qDebug() << "GroupChatController: Handling message history for chat" << chatId << "with" << history.size() << "messages";
    
    parseMessageHistory(chatId, history);
    
    // Отображаем историю в окне, если оно существует
    if (m_chatWindows.contains(chatId) && m_chatModels.contains(chatId)) {
        GroupChatModel *model = m_chatModels[chatId];
        m_chatWindows[chatId]->displayMessages(model->getMessages());
        qDebug() << "Displayed" << model->getMessages().size() << "messages in window for chat" << chatId;
    }
}

void GroupChatController::handleMembersUpdated(const QString &chatId, const QStringList &members, const QString &creator)
{
    qDebug() << "GroupChatController: Members updated for chat" << chatId << ":" << members << "Creator:" << creator;
    
    // Обновляем модель
    if (m_chatModels.contains(chatId)) {
        GroupChatModel *model = m_chatModels[chatId];
        model->setMembers(members);
        model->setCreator(creator);
    }
    
    // Обновляем окно
    if (m_chatWindows.contains(chatId)) {
        GroupChatWindow *window = m_chatWindows[chatId];
        window->updateMembersList(members);
        window->setCreator(creator);
    }
}

void GroupChatController::handleChatDeleted(const QString &chatId)
{
    qDebug() << "GroupChatController: Chat deleted:" << chatId;
    
    // Закрываем и удаляем окно
    if (m_chatWindows.contains(chatId)) {
        GroupChatWindow *window = m_chatWindows[chatId];
        if (window) {
            window->close();
            window->deleteLater();
        }
        m_chatWindows.remove(chatId);
    }
    
    // Удаляем модель
    if (m_chatModels.contains(chatId)) {
        delete m_chatModels[chatId];
        m_chatModels.remove(chatId);
    }
}

// Обработка действий пользователя
void GroupChatController::sendMessage(const QString &chatId, const QString &message)
{
    qDebug() << "GroupChatController: Sending message to chat" << chatId << ":" << message;
    emit messageSent(chatId, message);
}

void GroupChatController::requestMessageHistory(const QString &chatId)
{
    qDebug() << "GroupChatController: Requesting message history for chat" << chatId;
    m_chatController->sendMessageToServer("GET_GROUP_HISTORY:" + chatId);
}

void GroupChatController::requestGroupChatInfo(const QString &chatId)
{
    qDebug() << "GroupChatController: Requesting group chat info for chat" << chatId;
    m_chatController->sendMessageToServer("GROUP_GET_INFO:" + chatId);
    m_chatController->sendMessageToServer("GROUP_GET_CREATOR:" + chatId);
}

void GroupChatController::handleChatWindowClosed(const QString &chatId)
{
    qDebug() << "GroupChatController: Chat window closed for" << chatId;
    
    // НЕ удаляем окно и связанные объекты, а просто скрываем его
    // Это позволит сохранить историю между сессиями
    if (m_chatWindows.contains(chatId)) {
        m_chatWindows[chatId]->hide(); // Просто скрываем окно
        qDebug() << "Window hidden instead of destroyed for chat" << chatId;
    }
}

// Обработка управления участниками
void GroupChatController::handleAddUserToGroup(const QString &chatId, const QString &username)
{
    qDebug() << "GroupChatController: Adding user" << username << "to chat" << chatId;
    emit userAdditionRequested(chatId, username);
}

void GroupChatController::handleRemoveUserFromGroup(const QString &chatId, const QString &username)
{
    qDebug() << "GroupChatController: Removing user" << username << "from chat" << chatId;
    
    // Получаем модель чата для проверки прав
    if (!m_chatModels.contains(chatId)) {
        qDebug() << "GroupChatController: Chat model not found for" << chatId;
        return;
    }
    
    GroupChatModel *model = m_chatModels[chatId];
    
    // Используем метод модели для проверки возможности удаления
    if (!model->canRemoveMember(username)) {
        qDebug() << "GroupChatController: Cannot remove user" << username << "from chat" << chatId;
        // В будущем можно добавить сигнал об ошибке для отображения в UI
        return;
    }
    
    // Если все проверки пройдены, отправляем запрос
    emit userRemovalRequested(chatId, username);
}

void GroupChatController::handleDeleteGroupChat(const QString &chatId)
{
    qDebug() << "GroupChatController: Deleting chat" << chatId;
    emit chatDeletionRequested(chatId);
}

// Вспомогательные методы
GroupChatModel* GroupChatController::findOrCreateChatModel(const QString &chatId, const QString &chatName)
{
    if (m_chatModels.contains(chatId)) {
        return m_chatModels[chatId];
    }
    
    // Создаем новую модель
    GroupChatModel *model = new GroupChatModel(chatId, chatName, m_currentUsername, this);
    m_chatModels[chatId] = model;
    
    qDebug() << "Created new group chat model for" << chatName << "with ID" << chatId;
    return model;
}

void GroupChatController::setupConnectionsForWindow(GroupChatWindow *window, const QString &chatId)
{
    if (!window) return;
    
    // Подключаем сигналы от окна к контроллеру
    connect(window, &GroupChatWindow::messageSent,
            this, &GroupChatController::sendMessage);
            
    connect(window, &GroupChatWindow::addUserRequested,
            this, &GroupChatController::handleAddUserToGroup);
            
    connect(window, &GroupChatWindow::removeUserRequested,
            this, &GroupChatController::handleRemoveUserFromGroup);
            
    connect(window, &GroupChatWindow::deleteChatRequested,
            this, &GroupChatController::handleDeleteGroupChat);
    
    // Подключаем сигнал закрытия окна к обработчику
    connect(window, &QWidget::destroyed,
            this, [this, chatId]() {
                handleChatWindowClosed(chatId);
            });
            
    // Подключаем сигнал windowClosed от окна (для скрытия, а не уничтожения окна)
    connect(window, &GroupChatWindow::windowClosed,
            this, &GroupChatController::handleChatWindowClosed);
    
    // Подключаем сигнал для отметки сообщений как прочитанных
    connect(window, &GroupChatWindow::markAsReadRequested,
            this, &GroupChatController::markMessagesAsRead);
              // Подключаем модель к окну для обновления счетчика непрочитанных сообщений
    if (m_chatModels.contains(chatId)) {
        GroupChatModel *model = m_chatModels[chatId];
        connect(model, &GroupChatModel::unreadCountChanged,
                window, &GroupChatWindow::onUnreadCountChanged);
        connect(model, &GroupChatModel::creatorChanged,
                window, &GroupChatWindow::onCreatorChanged);
        connect(model, &GroupChatModel::membersUpdated,
                window, &GroupChatWindow::onMembersUpdated);
    }
    
    qDebug() << "Connections set up for group chat window" << chatId;
}

void GroupChatController::parseMessageHistory(const QString &chatId, const QList<QPair<QString, QString>> &history)
{
    if (!m_chatModels.contains(chatId)) {
        findOrCreateChatModel(chatId, QString("Групповой чат %1").arg(chatId.left(8)));
    }
    
    GroupChatModel *model = m_chatModels[chatId];
    if (!model) return;
    
    QList<GroupMessage> messages;
    QSet<QString> processedMessages; // Для фильтрации дубликатов
    
    for (const auto &entry : history) {
        QString senderAndTime = entry.first;
        QString content = entry.second;
        
        qDebug() << "GroupChatController: Parsing history entry - senderAndTime:" << senderAndTime << "content:" << content;
        
        // Извлекаем информацию о статусе прочитанности из content
        bool isRead = true; // По умолчанию считаем прочитанным
        QString actualContent = content;
        
        // Проверяем, есть ли в content информация о статусе прочитанности
        if (content.contains("|READ:")) {
            QStringList contentParts = content.split("|READ:");
            if (contentParts.size() >= 2) {
                actualContent = contentParts[0];
                QString readStatus = contentParts[1];
                isRead = (readStatus == "1");
                qDebug() << "GroupChatController: Extracted read status:" << readStatus << "isRead:" << isRead;
            }
        }
        
        // Парсим строку формата "[время] отправитель"
        QStringList parts = senderAndTime.split("] ");
        if (parts.size() >= 2) {
            QString timeStr = parts[0].mid(1); // Убираем [
            QString sender = parts[1];
            
            // ИСПРАВЛЕНИЕ: Создаем уникальный ключ для проверки дубликатов
            QString messageKey = QString("%1|%2|%3").arg(timeStr, sender, actualContent);
            
            // Проверяем дубликаты
            if (!processedMessages.contains(messageKey)) {
                qDebug() << "GroupChatController: Parsed - time:" << timeStr << "sender:" << sender << "isRead:" << isRead;
                
                // Создаем сообщение с правильным статусом прочитанности
                GroupMessage message(sender, actualContent, timeStr, isRead);
                messages.append(message);
                processedMessages.insert(messageKey);
            } else {
                qDebug() << "GroupChatController: Skipping duplicate message:" << messageKey;
            }
        } else {
            qDebug() << "GroupChatController: Failed to parse senderAndTime:" << senderAndTime << "parts size:" << parts.size();
        }
    }
    
    model->setMessageHistory(messages);
    qDebug() << "Parsed" << messages.size() << "messages from history for chat" << chatId << "(filtered" << (history.size() - messages.size()) << "duplicates)";
}

QString GroupChatController::generateChatId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void GroupChatController::handleCreatorUpdated(const QString &chatId, const QString &creator)
{
    qDebug() << "GroupChatController: Creator updated for chat" << chatId << ":" << creator;
    
    // Обновляем модель
    if (m_chatModels.contains(chatId)) {
        GroupChatModel *model = m_chatModels[chatId];
        model->setCreator(creator);
    }
    
    // Обновляем окно
    if (m_chatWindows.contains(chatId)) {
        GroupChatWindow *window = m_chatWindows[chatId];
        window->setCreator(creator);
    }
}


