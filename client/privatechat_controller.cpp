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
    qDebug() << "PrivateChatController: Received message from" << sender << ":" << message;

    // Если это сообщение от текущего пользователя на другом устройстве
    if (sender == m_currentUsername) {
        qDebug() << "Message from current user, likely from another device.";
        // Добавляем сообщение в модель чата с получателем
        // Здесь необходимо знать получателя, но это сложно определить
        // Пока просто добавляем сообщение для отладки
        PrivateChatModel *model = findOrCreateChatModel(sender);
        model->addMessage(sender, message, timestamp.isEmpty() ? 
                      QDateTime::currentDateTime().toString("hh:mm:ss") : timestamp);
        return; // Важно - не открываем новое окно
    }    // Получаем или создаём модель для этого чата
    PrivateChatModel *model = findOrCreateChatModel(sender);
    
    // Добавляем сообщение в модель
    QString actualTimestamp = timestamp.isEmpty() ? 
                      QDateTime::currentDateTime().toString("hh:mm:ss") : timestamp;
    model->addMessage(sender, message, actualTimestamp);
      // Если окно чата с этим пользователем уже открыто и видимо
    if (m_chatWindows.contains(sender) && m_chatWindows[sender]->isVisible()) {
        // Активируем окно только если оно уже открыто и видимо
        m_chatWindows[sender]->activateWindow();
        m_chatWindows[sender]->raise();
    } 
    // Больше не открываем окно автоматически при получении сообщения
    // Вместо этого полагаемся на уведомления через счетчик непрочитанных сообщений
    
    // Счетчик непрочитанных сообщений будет обновлен через ChatController
    // который запросит актуальные данные с сервера
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
    
    // Добавляем сообщение в модель чата для отображения (это наше сообщение)
    PrivateChatModel *model = findOrCreateChatModel(recipient);
    model->addMessage(m_currentUsername, message, timestamp);
    
    // Не запрашиваем историю сразу после отправки, чтобы избежать дублирования
    // История будет обновлена при следующем открытии окна чата
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
    
    if (m_chatModels.contains(username)) {
        m_chatModels[username]->markAllMessagesAsRead();
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
    // Находим или создаем модель для этого чата
    PrivateChatModel *model = findOrCreateChatModel(username);
    
    // Соединяем сигналы окна с соответствующими слотами
    connect(window, &PrivateChatWindow::messageSent, 
            this, &PrivateChatController::sendMessage);
            
    connect(window, &PrivateChatWindow::windowClosed,
            this, &PrivateChatController::handleChatWindowClosed);
            
    connect(window, &PrivateChatWindow::markAsReadRequested,
            this, &PrivateChatController::markMessagesAsRead);
    
    // Используем наш собственный сигнал shown вместо несуществующего windowActivated
    connect(window, &PrivateChatWindow::shown, [this, username]() {
        // Запрашиваем историю каждый раз при показе окна
        qDebug() << "Window shown for user" << username << ", explicitly requesting message history";
        // Добавляем небольшую задержку для надежности
        QTimer::singleShot(300, [this, username]() {
            requestMessageHistory(username);
        });
    });
            
    // Соединяем сигналы модели с слотами окна
    connect(model, &PrivateChatModel::messageAdded,
            [window](const PrivateMessage &message) {
                window->receiveMessage(message.sender, message.content, message.timestamp);
            });
            
    connect(model, &PrivateChatModel::peerStatusChanged,
            window, &PrivateChatWindow::onPeerStatusChanged);
            
    connect(model, &PrivateChatModel::unreadCountChanged,
            window, &PrivateChatWindow::onUnreadCountChanged);
            
    connect(model, &PrivateChatModel::messagesUpdated,
            [window, model]() {
                window->displayMessages(model->getMessages());
            });
            
    // Обновляем статус пользователя в окне
    window->updateUserStatus(model->isPeerOnline());
}

void PrivateChatController::parseMessageHistory(const QString &username, const QStringList &messages)
{
    qDebug() << "Parsing message history for" << username << "with" << messages.size() << "messages";
    
    // Проверяем, реально ли история пуста, иногда сервер отправляет это как одно сообщение
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
        
        // Отображаем пустую историю в окне
        if (m_chatWindows.contains(username)) {
            m_chatWindows[username]->displayMessages(QList<PrivateMessage>());
        }
        return;
    }
    
    // Получаем модель для этого чата
    PrivateChatModel *model = findOrCreateChatModel(username);
    
    // Создаем список для новых обработанных сообщений
    QList<PrivateMessage> parsedMessages;
    
    // Сохраняем существующие сообщения, если есть
    QList<PrivateMessage> existingMessages = model->getMessages();
    QMap<QString, bool> existingMessageMap; // key -> exists
    
    // Создаем карту существующих сообщений для предотвращения дубликатов
    for (const PrivateMessage &msg : existingMessages) {
        QString key = msg.sender + "_" + msg.timestamp + "_" + msg.content.left(10);
        existingMessageMap[key] = true;
    }
    
    // Перебираем все сообщения из истории
    for (const QString &messageData : messages) {
        qDebug() << "Processing message data:" << messageData;
        
        // Проверяем формат сообщения (может быть несколько вариантов)
        QString sender;
        QString content;
        QString timestamp;
        
        // Попытаемся распознать все возможные форматы сообщений
          // Формат 1: Новый формат "timestamp|sender|recipient|message"
        if (messageData.contains('|')) {
            QStringList parts = messageData.split('|');
            
            if (parts.size() >= 4) {
                // Новый формат: timestamp|sender|recipient|message
                timestamp = parts[0].trimmed();
                QString messageSender = parts[1].trimmed();
                QString messageRecipient = parts[2].trimmed();
                content = parts[3].trimmed();
                
                // Проверяем, что сообщение не служебное
                if (messageSender == "Система" || messageSender == "System") {
                    qDebug() << "Skipping system message:" << messageData;
                    continue;
                }
                
                // Определяем отправителя на основе текущего пользователя и собеседника
                if (messageSender == m_currentUsername) {
                    sender = m_currentUsername;
                } else if (messageSender == username) {
                    sender = username;
                } else {
                    // Попробуем определить по получателю
                    if (messageRecipient == m_currentUsername) {
                        sender = username; // Сообщение пришло к нам от собеседника
                    } else if (messageRecipient == username) {
                        sender = m_currentUsername; // Мы отправили сообщение собеседнику
                    } else {
                        qDebug() << "Cannot determine sender for message:" << messageData;
                        continue;
                    }
                }
            } else if (parts.size() >= 3) {
                // Старый формат: timestamp|sender|message (для совместимости)
                timestamp = parts[0].trimmed();
                sender = parts[1].trimmed();
                content = parts[2].trimmed();
                
                // Проверяем, что сообщение не служебное
                if (sender == "Система" || sender == "System") {
                    qDebug() << "Skipping system message:" << messageData;
                    continue;
                }
            } else if (parts.size() >= 2) {
                // Очень старый формат: sender|message
                sender = parts[0].trimmed();
                content = parts[1].trimmed();
                timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
                
                // Проверяем, что сообщение не служебное
                if (sender == "Система" || sender == "System") {
                    qDebug() << "Skipping system message:" << messageData;
                    continue;
                }
            } else {
                qDebug() << "Invalid message format (|), skipping:" << messageData;
                continue;
            }
        }
        // Формат 2: "PRIVMSG:sender:content:timestamp" или "PRIVMSG:recipient:content:timestamp"
        else if (messageData.contains("PRIVMSG:") || messageData.contains("SEND_PRIVATE:")) {
            QStringList parts = messageData.split(':');
            if (parts.size() >= 3) {
                if (parts[0] == "PRIVMSG" || parts[0] == "SEND_PRIVATE") {
                    QString possibleSender = parts[1].trimmed();
                    // Определяем, кто отправитель на основе контекста
                    if (possibleSender == m_currentUsername) {
                        sender = m_currentUsername; // Это сообщение от текущего пользователя
                    } else if (possibleSender == username) {
                        sender = username; // Это сообщение от собеседника
                    } else {
                        // Это может быть получатель, тогда отправитель - текущий пользователь
                        sender = m_currentUsername;
                    }
                    
                    content = parts[2];
                    
                    if (parts.size() > 3) {
                        timestamp = parts[3];
                    } else {
                        timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
                    }
                } else {
                    // Проверяем другие форматы
                    sender = parts[0].trimmed();
                    
                    if (parts.size() >= 2 && (parts[1] == "PRIVMSG" || parts[1] == "SEND_PRIVATE")) {
                        // Формат: "sender:PRIVMSG:recipient:content[:timestamp]"
                        if (parts.size() >= 4) {
                            content = parts[3];
                            if (parts.size() > 4) {
                                timestamp = parts[4];
                            } else {
                                timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
                            }
                        } else {
                            qDebug() << "Invalid command format, skipping:" << messageData;
                            continue;
                        }
                    } else {
                        // Возможно, это просто sender:content:timestamp
                        if (parts.size() >= 2) {
                            content = parts[1];
                            if (parts.size() > 2) {
                                timestamp = parts[2];
                            } else {
                                timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
                            }
                        } else {
                            qDebug() << "Unknown format, skipping:" << messageData;
                            continue;
                        }
                    }
                }
            } else {
                qDebug() << "Invalid message format with PRIVMSG, skipping:" << messageData;
                continue;
            }
        }
        // Формат 3: Простой формат "sender:content[:timestamp]"
        else if (messageData.contains(':')) {
            QStringList parts = messageData.split(':');
            if (parts.size() >= 2) {
                sender = parts[0].trimmed();
                content = parts[1];
                
                if (parts.size() > 2) {
                    timestamp = parts[2];
                } else {
                    timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
                }
            } else {
                qDebug() << "Invalid message format with colon, skipping:" << messageData;
                continue;
            }
        } else {
            qDebug() << "Unrecognized message format, skipping:" << messageData;
            continue;
        }
        
        // Проверяем, что отправитель является либо текущим пользователем, либо собеседником
        if (sender != m_currentUsername && sender != username) {
            qDebug() << "Message from unknown sender " << sender << ", trying to determine...";
            
            // Пытаемся определить отправителя по контексту
            if (messageData.contains(m_currentUsername)) {
                sender = username; // Предположительно, от собеседника
                qDebug() << "Assuming message is from chat partner: " << username;
            } else if (messageData.contains(username)) {
                sender = m_currentUsername; // Предположительно, от текущего пользователя
                qDebug() << "Assuming message is from current user: " << m_currentUsername;
            } else {
                qDebug() << "Cannot determine sender, using default values";
                sender = m_currentUsername; // По умолчанию считаем, что от текущего пользователя
            }
        }
        
        // Проверяем, что timestamp не пуст
        if (timestamp.trimmed().isEmpty()) {
            timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
        }
        
        // Дополнительная проверка избыточных пустых сообщений
        if (content.trimmed().isEmpty()) {
            qDebug() << "Skipping empty message content";
            continue;
        }
        
        // Проверяем, нет ли дубликата этого сообщения
        QString messageKey = sender + "_" + timestamp + "_" + content.left(10);
        if (!existingMessageMap.contains(messageKey)) {
            bool isFromCurrentUser = (sender == m_currentUsername);
            
            qDebug() << "Adding message: sender=" << sender 
                     << ", content=" << content 
                     << ", timestamp=" << timestamp;
            
            PrivateMessage message(sender, content, timestamp, isFromCurrentUser);
            parsedMessages.append(message);
            existingMessageMap[messageKey] = true;
        } else {
            qDebug() << "Skipping duplicate message:" << messageKey;
        }
    }
    
    // Добавляем новые сообщения в модель
    if (!parsedMessages.isEmpty()) {
        // Получаем модель для этого чата
        PrivateChatModel *model = findOrCreateChatModel(username);
        
        // Объединяем существующие и новые сообщения
        QList<PrivateMessage> existingMessages = model->getMessages();
        QList<PrivateMessage> allMessages = existingMessages;
        allMessages.append(parsedMessages);
        
        // Сортируем все сообщения по времени
        std::sort(allMessages.begin(), allMessages.end(), 
                 [](const PrivateMessage &a, const PrivateMessage &b) {
                     return a.timestamp < b.timestamp;
                 });
          qDebug() << "Setting complete message history with" << allMessages.size() 
                 << "messages (existing:" << existingMessages.size() 
                 << ", new:" << parsedMessages.size() << ")";
        
        model->setMessageHistory(allMessages);
        
        // Если окно существует, обновляем его
        if (m_chatWindows.contains(username)) {
            m_chatWindows[username]->displayMessages(model->getMessages());
            qDebug() << "Updated chat window display for" << username;
        }
    } else {
        qDebug() << "No new messages were parsed";
        // Показываем существующие сообщения, если они есть
        if (!existingMessages.isEmpty() && m_chatWindows.contains(username)) {
            m_chatWindows[username]->displayMessages(existingMessages);
        }
    }
}
