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
    
    // Обновляем имя пользователя во всех моделях
    for (auto it = m_chatModels.begin(); it != m_chatModels.end(); ++it) {
        // Создаем новую модель с правильным именем пользователя
        QString chatId = it.key();
        QString chatName = it.value()->getChatName();
        
        delete it.value();
        m_chatModels[chatId] = new GroupChatModel(chatId, chatName, username, this);
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
        
        // Уведомляем сервер о прочтении
        m_chatController->sendMessageToServer("MARK_GROUP_READ:" + chatId);
        
        // Запрашиваем обновленный счетчик с сервера
        QTimer::singleShot(500, this, [this, chatId]() {
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
        // Проверяем, активно ли окно чата - это нужно для определения статуса прочитанности
        bool windowIsActiveAndVisible = false;
        if (m_chatWindows.contains(chatId)) {
            GroupChatWindow *window = m_chatWindows[chatId];
            if (window) {
                windowIsActiveAndVisible = window->isVisible() && window->isActiveWindow() && !window->isMinimized();
            }
        }
        
        // Сообщения считаются прочитанными только если:
        // 1. Это сообщения от текущего пользователя ИЛИ
        // 2. Окно чата активно и видимо в данный момент
        bool isRead = (sender == m_currentUsername) || windowIsActiveAndVisible;
        
        // Добавляем сообщение в модель
        GroupMessage newMessage(sender, message, actualTimestamp, isRead);
        model->addMessage(newMessage);
        
        // Запрашиваем обновление счетчика непрочитанных сообщений с сервера
        if (!isRead) {
            QTimer::singleShot(100, this, [this, chatId]() {
                if (m_chatController) {
                    m_chatController->requestUnreadCountForGroupChat(chatId);
                }
            });
        }
    }
    
    // Отображаем сообщение в окне, если оно открыто
    if (m_chatWindows.contains(chatId)) {
        GroupChatWindow *window = m_chatWindows[chatId];
        if (window) {
            window->receiveMessage(sender, message, actualTimestamp);
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
    for (const auto &entry : history) {
        QString senderAndTime = entry.first;
        QString content = entry.second;
        
        qDebug() << "GroupChatController: Parsing history entry - senderAndTime:" << senderAndTime << "content:" << content;
        
        // Парсим строку формата "[время] отправитель"
        QStringList parts = senderAndTime.split("] ");
        if (parts.size() >= 2) {
            QString timeStr = parts[0].mid(1); // Убираем [
            QString sender = parts[1];
              qDebug() << "GroupChatController: Parsed - time:" << timeStr << "sender:" << sender;
            
            bool isFromCurrentUser = (sender == m_currentUsername);
            // История НЕ всегда считается прочитанной, только собственные сообщения
            GroupMessage message(sender, content, timeStr, isFromCurrentUser, isFromCurrentUser); 
            messages.append(message);
        } else {
            qDebug() << "GroupChatController: Failed to parse senderAndTime:" << senderAndTime << "parts size:" << parts.size();
        }
    }
    
    model->setMessageHistory(messages);
    qDebug() << "Parsed" << messages.size() << "messages from history for chat" << chatId;
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
