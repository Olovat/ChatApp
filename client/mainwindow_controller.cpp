#include "mainwindow_controller.h"
#include "mainwindow.h"
#include "groupchatwindow.h"
#include "controller_manager.h"
#include "privatechat_controller.h"
#include "privatechatwindow.h"
#include <QDebug>
#include <QMessageBox>
#include <QDateTime>
#include <QTimer>

MainWindowController::MainWindowController(MainWindow *view, QObject *parent)
    : QObject(parent), view(view)
{
    chatController = ControllerManager::instance().getChatController();
    chatController->setParent(this);
    
    // Инициализация контроллера приватных чатов
    privateChatController = new PrivateChatController(this, this);
    
    ControllerManager::instance().registerMainWindowController(this);
    
    setupViewConnections();    
    setupControllerConnections();
}

MainWindowController::~MainWindowController()
{
    // ChatController и privateChatController будут удалены автоматически, так как имеют родителя this
}

ChatController* MainWindowController::getChatController() const
{
    return chatController;
}

void MainWindowController::setupViewConnections()
{
    // Соединения сигналов представления с методами этого контроллера
    connect(view, &MainWindow::requestAuthorizeUser, 
            this, &MainWindowController::handleAuthorizeUser);
            
    connect(view, &MainWindow::requestRegisterUser, 
            this, &MainWindowController::handleRegisterUser);
            
    connect(view, &MainWindow::userSelected, 
            this, &MainWindowController::handleUserSelected);
            
    connect(view, &MainWindow::requestPrivateMessageSend, 
            this, &MainWindowController::handlePrivateMessageSend);
            
    connect(view, &MainWindow::requestGroupMessageSend, 
            this, &MainWindowController::handleGroupMessageSend);
            
    connect(view, &MainWindow::requestCreateGroupChat, 
            this, &MainWindowController::handleCreateGroupChat);
            
    connect(view, &MainWindow::requestJoinGroupChat, 
            this, &MainWindowController::handleJoinGroupChat);
            
    connect(view, &MainWindow::requestSearchUsers, 
            this, &MainWindowController::handleUserSearch);
            
    connect(view, &MainWindow::requestAddUserToFriends, 
            this, &MainWindowController::handleAddUserToFriends);
            
    connect(view, &MainWindow::requestPrivateMessageHistory,
            chatController, &ChatController::requestPrivateMessageHistory);
            
    // Соединения для групповых чатов
    connect(view, &MainWindow::requestStartAddUserToGroupMode,
            chatController, &ChatController::startAddUserToGroupMode);
            
    connect(view, &MainWindow::requestAddUserToGroupChat,
            this, &MainWindowController::handleAddUserToGroup);
            
    connect(view, &MainWindow::requestRemoveUserFromGroupChat,
            this, &MainWindowController::handleRemoveUserFromGroup);
            
    connect(view, &MainWindow::requestDeleteGroupChat,
            this, &MainWindowController::handleDeleteGroupChat);
    
    // Соединение сигнала двойного клика по пользователю
    connect(view, &MainWindow::userDoubleClicked, 
            this, &MainWindowController::handleUserDoubleClicked);
}

void MainWindowController::setupControllerConnections()
{
    // Соединения сигналов ChatController с методами этого контроллера
    connect(chatController, &ChatController::authenticationSuccessful, 
            this, &MainWindowController::handleAuthenticationSuccessful);
            
    connect(chatController, &ChatController::authenticationFailed,
            this, &MainWindowController::handleAuthenticationFailed);
            
    connect(chatController, &ChatController::registrationSuccessful,
            this, &MainWindowController::handleRegistrationSuccessful);
            
    connect(chatController, &ChatController::registrationFailed,
            this, &MainWindowController::handleRegistrationFailed);
            
    connect(chatController, &ChatController::userListUpdated,
            this, &MainWindowController::handleUserListUpdated);
            
    connect(chatController, &ChatController::searchResultsReady,
            this, &MainWindowController::handleSearchResultsReady);
            
    // Соединения для приватных чатов
    if (privateChatController && chatController) {
        connect(chatController, &ChatController::privateMessageReceived,
                privateChatController, &PrivateChatController::handleIncomingMessage);
        
        connect(chatController, &ChatController::privateHistoryReceived,
                privateChatController, &PrivateChatController::handleMessageHistory);
    }
    else {
        qDebug() << "Warning: privateChatController or chatController is null in setupControllerConnections";
    }
    
    connect(chatController, &ChatController::groupMessageReceived,
            this, &MainWindowController::handleGroupMessageReceived);
            
    connect(chatController, &ChatController::groupMembersUpdated,
            this, &MainWindowController::handleGroupMembersUpdated);
            
    connect(chatController, &ChatController::unreadCountsUpdated,
            this, &MainWindowController::handleUnreadCountsUpdated);
    
    // Добавляем соединение для сохранения сообщений (исправленная версия)
    if (chatController) {
        connect(chatController, &ChatController::privateMessageStored,
                chatController, &ChatController::storePrivateMessage);
    }
}

// Обработка действий пользователя из MainWindow
void MainWindowController::handleAuthorizeUser(const QString &username, const QString &password)
{
    chatController->authorizeUser(username, password);
}

void MainWindowController::handleRegisterUser(const QString &username, const QString &password)
{
    chatController->registerUser(username, password);
}

void MainWindowController::handleUserSelected(const QString &username)
{
    // Проверяем, что privateChatController инициализирован
    if (privateChatController) {
        // Если пользователь не выбирает себя
        if (username != view->getCurrentUsername()) {
            PrivateChatWindow *window = privateChatController->findOrCreateChatWindow(username);
            if (window) {
                // Запрашиваем историю явно перед показом окна
                privateChatController->requestMessageHistory(username);
                
                window->show();
                window->activateWindow();
                window->raise(); // Поднимаем окно наверх
            }
        } else {
            QMessageBox::information(view, "Информация", "Вы не можете отправлять сообщения самому себе.");
        }
    }
    else {
        qDebug() << "Error: privateChatController is null in handleUserSelected";
    }
}

void MainWindowController::handleGroupMessageSend(const QString &chatId, const QString &message)
{
    chatController->sendMessageToServer("SEND_GROUP|" + chatId + "|" + message); // поменять на : не забыть
}

void MainWindowController::handleCreateGroupChat(const QString &chatName)
{
    QString chatId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    chatController->createGroupChat(chatName, chatId);
}

void MainWindowController::handleJoinGroupChat(const QString &chatId)
{
    chatController->joinGroupChat(chatId);
}

void MainWindowController::handleAddUserToGroup(const QString &chatId, const QString &username)
{
    chatController->addUserToGroupChat(chatId, username);
}

void MainWindowController::handleRemoveUserFromGroup(const QString &chatId, const QString &username)
{
    chatController->removeUserFromGroupChat(chatId, username);
}

void MainWindowController::handleDeleteGroupChat(const QString &chatId)
{
    chatController->deleteGroupChat(chatId);
}

void MainWindowController::handleUserSearch(const QString &query)
{
    chatController->searchUsers(query);
}

void MainWindowController::handleAddUserToFriends(const QString &username)
{
    chatController->addUserToFriends(username);
}

// Обработка событий от ChatController
void MainWindowController::handleAuthenticationSuccessful()
{
    emit view->authSuccess();
    
    // Важно: устанавливаем текущего пользователя в PrivateChatController
    if (privateChatController) {
        privateChatController->setCurrentUsername(view->getCurrentUsername());
        qDebug() << "Set current username in PrivateChatController to" << view->getCurrentUsername();
        
        // Загрузка истории сообщений, но НЕ открытие окон автоматически
        if (chatController) {
            QTimer::singleShot(1000, [this]() {
                // Запрашиваем историю сообщений для всех известных пользователей
                chatController->requestRecentChatPartners();
            });
        }
    }
    
    view->display();
}

void MainWindowController::handleAuthenticationFailed(const QString &errorMessage)
{
    view->ui_Auth.setButtonsEnabled(true);
    
    // Очищаем поля ввода для возможности повторной авторизации
    view->ui_Auth.LineClear();
    
    // Показываем сообщение об ошибке авторизации - однократно
    static QString lastError;
    static QDateTime lastErrorTime;
    
    QDateTime currentTime = QDateTime::currentDateTime();
    
    // Предотвращаем повторное отображение того же сообщения в течение 2 секунд
    if (lastError != errorMessage || lastErrorTime.msecsTo(currentTime) > 2000) {
        QMessageBox::critical(view, "Ошибка авторизации", errorMessage);
        lastError = errorMessage;
        lastErrorTime = currentTime;
    }
    
    // Устанавливаем статус авторизации через публичный метод
    view->setLoginSuccessful(false);
    
    // Логируем для отладки
    qDebug() << "Authentication failed:" << errorMessage;
}

void MainWindowController::handleRegistrationSuccessful()
{
    QMessageBox::information(view, "Регистрация", "Регистрация успешно завершена");
    emit view->registerSuccess();
}

void MainWindowController::handleRegistrationFailed(const QString &errorMessage)
{
    QMessageBox::critical(view, "Ошибка регистрации", errorMessage);
}

void MainWindowController::handleUserListUpdated(const QStringList &users)
{
    view->updateUserList(users);
    emit view->userListUpdated();
}

void MainWindowController::handleSearchResultsReady(const QStringList &users)
{
    view->showSearchResults(users);
}

void MainWindowController::handleGroupMessageReceived(const QString &chatId, const QString &sender, const QString &message, const QString &timestamp)
{
    Q_UNUSED(chatId);
    Q_UNUSED(sender);
    Q_UNUSED(message);
    Q_UNUSED(timestamp);
    
    // TODO: Реализовать обработку группового сообщения в контроллере
}

void MainWindowController::handleGroupMembersUpdated(const QString &chatId, const QStringList &members, const QString &creator)
{
    // Функциональность должна быть добавлена в MainWindow
    Q_UNUSED(chatId);
    Q_UNUSED(members);
    Q_UNUSED(creator);
    
    // TODO: Реализовать обновление списка участников группы

}

void MainWindowController::handleUnreadCountsUpdated(const QMap<QString, int> &privateCounts, const QMap<QString, int> &groupCounts)
{
    // Функциональность должна быть добавлена в MainWindow
    Q_UNUSED(privateCounts);
    Q_UNUSED(groupCounts);
    
    // TODO: Реализовать обновление счетчиков непрочитанных сообщений
}

void MainWindowController::handlePrivateMessageSend(const QString &recipient, const QString &message)
{
    qDebug() << "MainWindowController: Handling private message send to" << recipient << ":" << message;
    
    if (chatController) {
        chatController->sendPrivateMessage(recipient, message);
    } else {
        qDebug() << "Error: chatController is null in handlePrivateMessageSend";
    }
}

void MainWindowController::handleUserDoubleClicked(const QString &username)
{
    qDebug() << "MainWindowController: User double-clicked on" << username;
    
    // Обрабатываем двойной клик так же, как и обычный выбор пользователя
    handleUserSelected(username);
}
