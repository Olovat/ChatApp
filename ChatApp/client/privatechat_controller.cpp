#include "privatechat_controller.h"
#include "privatechatwindow.h"
#include "mainwindow_controller.h"
#include "chat_controller.h"
#include <QDebug>
#include <QTimer>

PrivateChatController::PrivateChatController(MainWindowController *mainController, QObject *parent)
    : QObject(parent), m_mainController(mainController)
{
    // Получаем ChatController через MainWindowController
    m_chatController = mainController->getChatController();
    
    // Подключаем сигналы этого контроллера к слотам ChatController
    connect(this, &PrivateChatController::messageSent,
            m_chatController, &ChatController::sendPrivateMessage);
            
    qDebug() << "PrivateChatController initialized";
}

PrivateChatController::~PrivateChatController()
{
    // Очищаем модели чатов
    for (auto it = m_chatModels.begin(); it != m_chatModels.end(); ++it) {
        delete it.value();
    }
    m_chatModels.clear();
    
    // Окна чатов будут удалены автоматически, так как у них есть родитель
    m_chatWindows.clear();
    
    qDebug() << "PrivateChatController destroyed";
}

PrivateChatWindow* PrivateChatController::findOrCreateChatWindow(const QString &username)
{
    // Если окно уже существует, возвращаем его
    if (m_chatWindows.contains(username)) {
        qDebug() << "Found existing window for" << username;
          // Если нужно, проверяем историю и запрашиваем ее
        if (findOrCreateChatModel(username)->getMessages().isEmpty()) {
            // Если сообщений нет, запрашиваем историю и показываем индикатор загрузки
            m_chatWindows[username]->showLoadingIndicator();
            
            // Запрашиваем историю с сервера
            requestMessageHistory(username);
        }
        
        // Всегда запрашиваем историю при открытии окна, даже если оно уже существует
        QTimer::singleShot(100, [this, username]() {
            requestMessageHistory(username);
        });
        
        // Показываем индикатор загрузки
        m_chatWindows[username]->showLoadingIndicator();
        
        return m_chatWindows[username];
    }
    
    // Создаём новое окно чата
    PrivateChatWindow *chatWindow = new PrivateChatWindow(username);
    
    // Показываем индикатор загрузки сразу
    chatWindow->showLoadingIndicator();
    
    // Устанавливаем контроллер для окна
    chatWindow->setController(this);
    
    // Настраиваем соединения
    setupConnectionsForWindow(chatWindow, username);
      // Сохраняем окно в карте
    m_chatWindows[username] = chatWindow;
    
    // Запрашиваем историю сообщений с сервера
    qDebug() << "Requesting message history for new chat window with" << username;
    QTimer::singleShot(100, [this, username]() {
        requestMessageHistory(username);
    });
    
    return chatWindow;
}

QStringList PrivateChatController::getActiveChatUsernames() const
{
    return m_chatWindows.keys();
}

int PrivateChatController::getActiveChatsCount() const
{
    return m_chatWindows.size();
}

void PrivateChatController::updateUserStatus(const QString &username, bool isOnline)
{
    // Обновляем статус в модели
    if (m_chatModels.contains(username)) {
        m_chatModels[username]->setPeerStatus(isOnline);
    }
    
    // Обновляем статус в окне, если оно существует
    if (m_chatWindows.contains(username)) {
        m_chatWindows[username]->updateUserStatus(isOnline);
    }
}

void PrivateChatController::updateAllUserStatuses(const QMap<QString, bool> &userStatusMap)
{
    for (auto it = userStatusMap.begin(); it != userStatusMap.end(); ++it) {
        updateUserStatus(it.key(), it.value());
    }
}

void PrivateChatController::setCurrentUsername(const QString &username)
{
    m_currentUsername = username;
    qDebug() << "PrivateChatController: Current username set to" << username;
    
    // Обновляем текущего пользователя во всех моделях
    for (auto it = m_chatModels.begin(); it != m_chatModels.end(); ++it) {
        // Создаем новую модель с правильным текущим пользователем
        PrivateChatModel *newModel = new PrivateChatModel(username, it.key(), this);
        
        // Копируем сообщения из старой модели в новую
        newModel->setMessageHistory(it.value()->getMessages());
        
        // Удаляем старую модель и заменяем ее новой
        delete it.value();
        it.value() = newModel;
    }
}

QString PrivateChatController::getCurrentUsername() const
{
    return m_currentUsername;
}

void PrivateChatController::handleIncomingMessage(const QString &sender, const QString &message, const QString &timestamp)
{
    qDebug() << "PrivateChatController: Handling incoming message from" << sender << ":" << message;
    
    QString actualTimestamp = timestamp.isEmpty() ? 
                      QDateTime::currentDateTime().toString("hh:mm:ss") : timestamp;
    
    // ИСПРАВЛЕНИЕ: Обрабатываем сообщения от текущего пользователя (с другого устройства)
    if (sender == m_currentUsername) {
        qDebug() << "Message from current user, likely from another device or echo.";
        
        // ВАЖНО: Нужно определить получателя для корректного отображения
        // Это может быть сложно без дополнительной информации о получателе
        // Пока просто добавляем в модель без отображения в окне
        PrivateChatModel *model = findOrCreateChatModel(sender);
        model->addMessage(sender, message, actualTimestamp);
        return;
    }
    
    // Получаем или создаём модель для этого чата
    PrivateChatModel *model = findOrCreateChatModel(sender);
    
    // ИСПРАВЛЕНИЕ: Проверяем, открыто ли окно и видимо ли оно
    bool windowIsVisibleAndActive = false;
    if (m_chatWindows.contains(sender)) {
        PrivateChatWindow* window = m_chatWindows[sender];
        windowIsVisibleAndActive = window && 
                                  window->isVisible() && 
                                  !window->isMinimized() &&
                                  window->isActiveWindow();
        
        qDebug() << "Window check for" << sender << ": exists=" << (window != nullptr) 
                 << ", visible=" << (window ? window->isVisible() : false)
                 << ", minimized=" << (window ? window->isMinimized() : false)
                 << ", active=" << (window ? window->isActiveWindow() : false)
                 << ", final=" << windowIsVisibleAndActive;
    }
    
    // КЛЮЧЕВОЕ ИСПРАВЛЕНИЕ: Создаем сообщение с правильным статусом прочитанности
    PrivateMessage newMessage(sender, message, actualTimestamp, windowIsVisibleAndActive);
    model->addMessage(newMessage);
    
    // Если окно открыто и активно, немедленно помечаем ВСЕ сообщения как прочитанные
    if (windowIsVisibleAndActive) {
        model->markAllMessagesAsRead();
        qDebug() << "Marked all messages as read because window is visible and active";
        
        // Активируем окно
        m_chatWindows[sender]->activateWindow();
        m_chatWindows[sender]->raise();
    }
    
    // ИСПРАВЛЕНИЕ: Отображаем сообщение в окне для ВСЕХ сообщений (не только от других)
    if (m_chatWindows.contains(sender)) {
        m_chatWindows[sender]->receiveMessage(sender, message, actualTimestamp);
        qDebug() << "Displayed message in window for" << sender;
    }
}

void PrivateChatController::handleMessageHistory(const QString &username, const QStringList &messages)
{
    qDebug() << "PrivateChatController: Received message history for" << username << "with" << messages.size() << "messages";
    
    // Подробный вывод для отладки
    for (int i = 0; i < messages.size(); ++i) {
        qDebug() << "History message" << i << ":" << messages[i];
    }
    
    // Если список сообщений пуст, отображаем пустую историю
    if (messages.isEmpty()) {
        qDebug() << "Empty history received for" << username;
        
        if (m_chatWindows.contains(username)) {
            m_chatWindows[username]->displayMessages(QList<PrivateMessage>());
        }
        return;
    }
    
    // Парсим и обрабатываем историю сообщений
    parseMessageHistory(username, messages);
    
    // Проверяем, существует ли окно для этого пользователя
    if (m_chatWindows.contains(username)) {
        PrivateChatModel *model = findOrCreateChatModel(username);
        m_chatWindows[username]->displayMessages(model->getMessages());
    }
}

void PrivateChatController::sendMessage(const QString &recipient, const QString &message)
{
    qDebug() << "PrivateChatController: Sending message to" << recipient << ":" << message;
    
    // Отправляем сообщение через ChatController
    emit messageSent(recipient, message);
    
    // Текущее время для метки сообщения
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    
    // ИСПРАВЛЕНИЕ: Добавляем сообщение в модель чата для отображения (это наше сообщение)
    PrivateChatModel *model = findOrCreateChatModel(recipient);
    model->addMessage(m_currentUsername, message, timestamp);
    
    // ИСПРАВЛЕНИЕ: Немедленно отображаем сообщение в окне чата
    if (m_chatWindows.contains(recipient)) {
        m_chatWindows[recipient]->receiveMessage(m_currentUsername, message, timestamp);
        qDebug() << "Displayed sent message in window for" << recipient;
    }
    
    // Не запрашиваем историю сразу после отправки, чтобы избежать дублирования
}

void PrivateChatController::requestMessageHistory(const QString &username)
{
    qDebug() << "PrivateChatController: Requesting message history for" << username;
    
    // Показываем индикатор загрузки в окне, если оно существует
    if (m_chatWindows.contains(username)) {
        m_chatWindows[username]->showLoadingIndicator();
    }
    
    if (m_chatController) {
        // Запрашиваем историю дважды с небольшой задержкой
        m_chatController->requestPrivateMessageHistory(username);
        
        // Запрашиваем еще раз через небольшую задержку для надежности
        QTimer::singleShot(500, [this, username]() {
            // Проверяем, что модель все еще пуста перед повторным запросом
            PrivateChatModel *model = findOrCreateChatModel(username);
            if (model->getMessages().isEmpty()) {
                qDebug() << "No messages received yet, requesting history again for" << username;
                m_chatController->requestPrivateMessageHistory(username);
            }
        });
    } else {
        qDebug() << "ERROR: ChatController is null in requestMessageHistory!";
    }
}

void PrivateChatController::markMessagesAsRead(const QString &username)
{
    qDebug() << "PrivateChatController: Marking messages as read for" << username;
    
    // Помечаем в модели
    if (m_chatModels.contains(username)) {
        m_chatModels[username]->markAllMessagesAsRead();
        qDebug() << "Marked messages as read in model for" << username;
    }
    
    // Отправляем команду на сервер
    if (m_chatController) {
        m_chatController->markMessagesAsRead(username);
        qDebug() << "Sent mark as read command to server for" << username;
    }
}

void PrivateChatController::handleChatWindowClosed(const QString &username)
{
    qDebug() << "PrivateChatController: Chat window closed for" << username;
    
    // ВАЖНО: НЕ удаляем окно и связанные объекты, а просто скрываем его
    // Это позволит сохранить историю между сессиями
    if (m_chatWindows.contains(username)) {
        m_chatWindows[username]->hide(); // Просто скрываем окно
        qDebug() << "Window hidden instead of destroyed for" << username;
    }
    
    // Важно: НЕ удаляем модель данных, чтобы сохранить историю
    // m_chatWindows.remove(username); - эта строка удалена!
}

PrivateChatModel* PrivateChatController::findOrCreateChatModel(const QString &username)
{
    if (m_chatModels.contains(username)) {
        return m_chatModels[username];
    }
    
    PrivateChatModel *model = new PrivateChatModel(m_currentUsername, username, this);
    m_chatModels[username] = model;
    
    return model;
}

void PrivateChatController::setupConnectionsForWindow(PrivateChatWindow *window, const QString &username)
{
    if (!window) return;
    
    // Подключаем сигналы от окна к контроллеру
    connect(window, &PrivateChatWindow::messageSent,
            this, &PrivateChatController::sendMessage);
            
    connect(window, &PrivateChatWindow::windowClosed,
            this, &PrivateChatController::handleChatWindowClosed);
            
    // ИСПРАВЛЕНИЕ: Подключаем сигнал markAsReadRequested
    connect(window, &PrivateChatWindow::markAsReadRequested,
            this, &PrivateChatController::markMessagesAsRead);
            
    connect(window, &PrivateChatWindow::shown, this, [this, username]() {
        // При показе окна сразу помечаем как прочитанные
        markMessagesAsRead(username);
    });
    
    qDebug() << "Setup connections for chat window with" << username;
}

void PrivateChatController::parseMessageHistory(const QString &username, const QStringList &messages)
{
    qDebug() << "Parsing message history for" << username << "with" << messages.size() << "messages";
    
    // Проверяем, реально ли история пуста
    bool emptyHistory = false;
    for (const QString &msg : messages) {
        if (msg.contains("История личных сообщений пуста") || 
            msg.contains("0000-00-00 00:00:00|Система|") || 
            msg.contains("Empty private message history")) {
            emptyHistory = true;
            break;
        }
    }
    
    if (emptyHistory) {
        qDebug() << "Empty history detected for" << username;
        if (m_chatWindows.contains(username)) {
            m_chatWindows[username]->displayMessages(QList<PrivateMessage>());
        }
        return;
    }
    
    // Получаем модель для этого чата
    PrivateChatModel *model = findOrCreateChatModel(username);
    
    // ИСПРАВЛЕНИЕ: Очищаем существующие сообщения при загрузке истории
    // чтобы избежать дублирования
    QList<PrivateMessage> parsedMessages;
    QSet<QString> processedMessages; // Используем Set для быстрой проверки дубликатов
    
    // Перебираем все сообщения из истории
    for (const QString &messageData : messages) {
        qDebug() << "Processing message data:" << messageData;
        
        QString sender;
        QString content;
        QString timestamp;
        
        // Парсим сообщение (основной формат: timestamp|sender|recipient|message)
        if (messageData.contains('|')) {
            QStringList parts = messageData.split('|');
            
            if (parts.size() >= 4) {
                // Новый формат: timestamp|sender|recipient|message
                timestamp = parts[0].trimmed();
                QString messageSender = parts[1].trimmed();
                QString messageRecipient = parts[2].trimmed();
                content = parts[3].trimmed();
                
                // Пропускаем системные сообщения
                if (messageSender == "Система" || messageSender == "System") {
                    continue;
                }
                
                // Определяем отправителя
                if (messageSender == m_currentUsername) {
                    sender = m_currentUsername;
                } else if (messageSender == username) {
                    sender = username;
                } else if (messageRecipient == m_currentUsername) {
                    sender = username;
                } else if (messageRecipient == username) {
                    sender = m_currentUsername;
                } else {
                    qDebug() << "Cannot determine sender for message:" << messageData;
                    continue;
                }
            } else if (parts.size() >= 3) {
                // Старый формат для совместимости
                timestamp = parts[0].trimmed();
                sender = parts[1].trimmed();
                content = parts[2].trimmed();
                
                if (sender == "Система" || sender == "System") {
                    continue;
                }
            } else {
                continue;
            }
        } else {
            // Другие форматы (для совместимости)
            continue;
        }
        
        // Проверяем корректность данных
        if (content.trimmed().isEmpty()) {
            continue;
        }
        
        if (timestamp.trimmed().isEmpty()) {
            timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
        }
        
        // Создаем уникальный ключ для проверки дубликатов
        QString messageKey = QString("%1|%2|%3").arg(timestamp, sender, content);
        
        // ИСПРАВЛЕНИЕ: Проверяем дубликаты более строго
        if (!processedMessages.contains(messageKey)) {
            bool isFromCurrentUser = (sender == m_currentUsername);
            
            qDebug() << "Adding unique message: sender=" << sender 
                     << ", content=" << content 
                     << ", timestamp=" << timestamp;
            
            PrivateMessage message(sender, content, timestamp, isFromCurrentUser);
            parsedMessages.append(message);
            processedMessages.insert(messageKey);
        } else {
            qDebug() << "Skipping duplicate message:" << messageKey;
        }
    }
    
    // ИСПРАВЛЕНИЕ: Заменяем все сообщения вместо добавления к существующим
    if (!parsedMessages.isEmpty()) {
        // Сортируем сообщения по времени
        std::sort(parsedMessages.begin(), parsedMessages.end(), 
                 [](const PrivateMessage &a, const PrivateMessage &b) {
                     return a.timestamp < b.timestamp;
                 });
        
        qDebug() << "Setting message history with" << parsedMessages.size() << "unique messages";
        
        // КЛЮЧЕВОЕ ИСПРАВЛЕНИЕ: заменяем историю полностью
        model->setMessageHistory(parsedMessages);
        
        // Обновляем отображение в окне
        if (m_chatWindows.contains(username)) {
            m_chatWindows[username]->displayMessages(parsedMessages);
            qDebug() << "Updated chat window display for" << username;
        }
    } else {
        qDebug() << "No valid messages were parsed";
        // Очищаем окно, если нет сообщений
        if (m_chatWindows.contains(username)) {
            m_chatWindows[username]->displayMessages(QList<PrivateMessage>());
        }
    }
}
