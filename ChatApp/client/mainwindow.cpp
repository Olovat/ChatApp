#include "mainwindow.h"
#include "ui_mainwindow.h"
// #include "privatechatwindow.h" // Удалено
#include "groupchatwindow.h"
#include "transitwindow.h"
#include "groupchat_controller.h"
#include "mainwindow_controller.h"
#include <QMessageBox>
#include <QVBoxLayout>
#include <QShortcut>
#include <QKeySequence>
#include <QDebug>
#include <QDialog>
#include <QInputDialog>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      mode(Mode::Production),
      ui(new Ui::MainWindow),
      m_loginSuccesfull(false)
{
    initializeCommon();
}

MainWindow::MainWindow(Mode mode, QWidget *parent)
    : QMainWindow(parent),
      mode(mode),
      ui(new Ui::MainWindow),
      m_loginSuccesfull(false)
{
    initializeCommon();
}

void MainWindow::initializeCommon()
{
    ui->setupUi(this);

    // Добавляем явное подключение для поля поиска
    connect(ui->lineEdit, &QLineEdit::returnPressed, this, &MainWindow::on_lineEdit_returnPressed);
    
    // Инициализация переменных для поиска
    searchDialog = nullptr;
    searchListWidget = nullptr;

    // Инициализация контроллера
    controller = new MainWindowController(this, this);

    // Подключение сигналов для окон авторизации и регистрации
    connect(&ui_Auth, &auth_window::login_button_clicked, this, &MainWindow::authorizeUser);
    connect(&ui_Auth, &auth_window::register_button_clicked, this, &MainWindow::prepareForRegistration);
    connect(&ui_Reg, &reg_window::register_button_clicked2, this, &MainWindow::registerUser);
    connect(&ui_Reg, &reg_window::returnToAuth, this, [this]() {
        ui_Reg.hide();
        ui_Auth.show();
    });
      // Connect the authSuccess signal to the onAuthSuccess slot
    connect(this, &MainWindow::authSuccess, this, &MainWindow::onAuthSuccess);
    
    // Connect the registerSuccess signal
    connect(this, &MainWindow::registerSuccess, this, &MainWindow::onRegisterSuccess);

    connect(ui->userListWidget, &QListWidget::itemClicked, this, &MainWindow::onUserSelected);

    // Скрываем главное окно до авторизации
    this->hide();
    
    // По умолчанию показываем окно авторизации
    ui_Auth.show();
}

MainWindow::~MainWindow()
{
    delete ui;
    // Контроллер удалится автоматически, так как имеет родителя this
}

void MainWindow::display()
{
    ui_Auth.hide();  // Hide authentication window
    this->show();    // Show main window
    updateWindowTitle();
}

void MainWindow::setLogin(const QString &login)
{
    m_username = login;
    updateWindowTitle();
}


void MainWindow::setPass(const QString &pass)
{
    m_userpass = pass;
}

QString MainWindow::getUsername() const
{
    return m_username;
}

QString MainWindow::getUserpass() const
{
    return m_userpass;
}

QString MainWindow::getCurrentUsername() const
{
    return m_username;
}

bool MainWindow::isLoginSuccessful() const
{
    return m_loginSuccesfull;
}

void MainWindow::setLoginSuccessful(bool success)
{
    m_loginSuccesfull = success;
    // Можно добавить логирование для отладки
    qDebug() << "Login status set to:" << (success ? "successful" : "failed");
}

void MainWindow::authorizeUser()
{
    QString login = ui_Auth.getLogin().trimmed(); // Trimmed the login input
    QString password = ui_Auth.getPass(); // Assuming password does not need trimming or is handled appropriately

    if (login.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "Ошибка авторизации", "Логин и пароль не могут быть пустыми");
        return;
    }    // Получаем существующий экземпляр контроллера через контроллер MainWindow
    ChatController *chatController = controller->getChatController();
    
    // Если уже авторизован с этими же учетными данными, просто показываем окно
    if (isLoginSuccessful() && chatController->getCurrentUsername() == login) {
        ui_Auth.hide();
        this->display();
        return;
    }

    // Сохраняем логин и пароль
    m_username = login;
    m_userpass = password;
    
    // Переподключаемся к серверу если нужно
    if (!chatController->isConnected()) {
        if (!chatController->reconnectToServer()) {
            QMessageBox::critical(this, "Ошибка соединения", "Не удалось подключиться к серверу");
            return;
        }
    }
    
    // Отключаем кнопку, чтобы предотвратить повторные нажатия
    ui_Auth.setButtonsEnabled(false);
    
    // Передаем запрос на авторизацию в контроллер
    emit requestAuthorizeUser(login, password);
}

void MainWindow::registerUser()
{
    QString username = ui_Reg.getLogin();
    QString password = ui_Reg.getPassword();
    
    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(&ui_Reg, "Ошибка", "Логин и пароль не должны быть пустыми");
        return;
    }
    
    // Сохраняем учетные данные
    m_username = username;
    m_userpass = password;
    
    // Получаем существующий экземпляр контроллера через контроллер MainWindow
    ChatController *chatController = controller->getChatController();
    
    // Переподключаемся к серверу если нужно
    if (!chatController->isConnected()) {
        if (!chatController->reconnectToServer()) {
            QMessageBox::critical(this, "Ошибка соединения", "Не удалось подключиться к серверу");
            return;
        }
    }
    
    // Отключаем кнопку, чтобы предотвратить повторные нажатия
    ui_Reg.setButtonsEnabled(false);
    
    // Отправляем запрос на регистрацию через контроллер
    emit requestRegisterUser(username, password);
}

void MainWindow::prepareForRegistration()
{
    ui_Auth.hide();
    ui_Reg.show();
}

void MainWindow::updateUserList(const QStringList &users)
{
    qDebug() << "Updating user list with users:" << users;

    // ВАЖНО: Сохраняем текущие счетчики уведомлений перед обновлением списка
    QMap<QString, int> savedUnreadMessageCounts = unreadMessageCounts;
    QMap<QString, int> savedUnreadGroupMessageCounts = unreadGroupMessageCounts;

    // Для отладки - выводим все статусы пользователей
    for (const QString &userInfo : users) {
        QStringList debug_parts = userInfo.split(":", Qt::SkipEmptyParts);
        if (debug_parts.size() >= 2) {
            QString dbg_username = debug_parts[0];
            QString dbg_status = debug_parts[1];
            QString dbg_type = (debug_parts.size() > 2) ? debug_parts.mid(2).join(":") : ""; // Join remaining parts for type
            qDebug() << "DEBUG RAW User:" << dbg_username << "status:" << dbg_status << "type:" << dbg_type << "(Original:" << userInfo << ")";
        } else {
            qDebug() << "DEBUG RAW User: Invalid format -" << userInfo;
        }
    }

    // Если список пользователей пустой, не обновляем UI (но все равно очищаем, чтобы убрать старых)
    // if (users.isEmpty()) {
    //     qDebug() << "User list is empty, skipping update";
    //     return;
    // }
    
    // Сохраняем копию списка пользователей (локальный userList член класса)
    this->userList = users;
    
    // Сохраняем текущий список друзей (чтобы не потерять друзей, которые оффлайн)
    QMap<QString, bool> existingFriends = userFriends; // userFriends is a member, captures state before this update
      // Получаем актуальный список друзей из ChatController (полученные от сервера)
    if (controller) {
        ChatController* chatController = controller->getChatController();
        if (chatController) {
            QStringList savedFriends = chatController->getFriendList();
            qDebug() << "Got friend list from ChatController, found" << savedFriends.size() << "friends from server";
            
            for (const QString &friend_name : savedFriends) {
                if (!existingFriends.contains(friend_name)) {
                    existingFriends[friend_name] = false; // По умолчанию оффлайн, если новый от сервера
                    qDebug() << "Added friend from controller's server list:" << friend_name;
                }
            }
        }
    }
    
    // Очищаем и заново заполняем список пользователей в UI
    ui->userListWidget->clear();
    // userFriends.clear(); // Не очищаем userFriends здесь, будем обновлять его статусы.
                         // existingFriends уже содержит предыдущее состояние userFriends + друзей из настроек.
                         // userFriends (член класса) будет перестроен ниже.
    
    // Создаем списки для онлайн/оффлайн пользователей и групповых чатов
    QStringList onlineFriendUsers; // Changed from onlineUsers to be specific
    QStringList offlineFriendUsers; // Changed from offlineUsers to be specific
    QStringList groupChats;
    
    QMap<QString, bool> currentSessionUserFriends; // Новый map для userFriends в этой сессии

    qDebug() << "Current username: " << m_username;
    for (const QString &userInfo : users) { // Iterate over the input list from ChatController
        qDebug() << "Processing user info:" << userInfo;
        
        QStringList parts = userInfo.split(":", Qt::SkipEmptyParts);
        if (parts.isEmpty()) {
            continue;
        }

        QString username = parts[0];
        bool isOnline = false;
        QString statusPart; // Для дебага статуса

        if (parts.size() > 1) {
            statusPart = parts[1];
            isOnline = (statusPart == "1" || statusPart == "online" || statusPart.toInt() == 1);
        }
        qDebug() << "Parsed status for" << username << ":" << statusPart << " -> isOnline=" << isOnline 
                 << " (Raw data:" << userInfo << ")";
        
        QString typeFromServer;
        QString groupChatName; // Для хранения названия группового чата
        
        if (parts.size() == 3) { // username:status:TYPE (e.g. F, G, U)
            typeFromServer = parts[2];
        } else if (parts.size() >= 4) { // chatId:status:G:chatName или username:status:U:F
            typeFromServer = parts[2];
            if (typeFromServer == "G" && parts.size() >= 4) {
                // Это групповой чат: chatId:1:G:chatName
                groupChatName = parts[3];
                typeFromServer = "G";
            } else if (parts.size() >= 4) {
                // Это пользователь с дополнительным типом: username:status:U:F
                typeFromServer = parts[2] + ":" + parts[3];
            }
        }
        qDebug() << "Parsed type for" << username << ":" << typeFromServer << (groupChatName.isEmpty() ? "" : " (Group name: " + groupChatName + ")");
        
        if (username == m_username) {
            qDebug() << "Skipping current user:" << username;
            continue;
        }
          // Определяем тип пользователя на основе typeFromServer
        bool isFriendByServerType = typeFromServer.endsWith(":F"); // e.g., "U:F"
        bool isGroupChatByServerType = typeFromServer == "G"; // Точное совпадение для групповых чатов

        if (isGroupChatByServerType || username.startsWith("GROUP_")) {
            // Для групповых чатов сохраняем как chatId:chatName для удобства
            QString displayName = groupChatName.isEmpty() ? username : groupChatName;
            QString groupEntry = username + ":" + displayName; // chatId:chatName
            groupChats.append(groupEntry);
            qDebug() << "Identified as group chat:" << username << "with name:" << displayName;
        } else if (isFriendByServerType || existingFriends.contains(username)) {
            // Либо сервер сказал, что это друг (U:F), либо он уже был в нашем списке друзей
            qDebug() << "User" << username << "is a friend (server type:" << typeFromServer << ", was in existingFriends:" << existingFriends.contains(username) << "). Status:" << (isOnline ? "online" : "offline");
            currentSessionUserFriends[username] = isOnline; // Обновляем статус в текущей сессии
            if (isOnline) {
                onlineFriendUsers.append(username);
            } else {
                offlineFriendUsers.append(username);
            }
        } else {
            // Обычный пользователь (U), не друг - не добавляем в список интерфейса
            // Поиск пользователей обрабатывается отдельно через showSearchResults
            qDebug() << "User" << username << "(type:" << typeFromServer << ") is not a friend, skipping from user list.";
        }
    }
    
    // Восстанавливаем оффлайн-друзей из existingFriends, если они не пришли в новом списке `users`
    // Это важно, чтобы не терять друзей, которые просто оффлайн и не были упомянуты в последнем USERLIST
    for (auto it = existingFriends.begin(); it != existingFriends.end(); ++it) {
        const QString& friendUsername = it.key();
        if (friendUsername == m_username) continue; // Не добавляем себя

        if (!currentSessionUserFriends.contains(friendUsername)) { // Если друг не был обработан из списка `users`
            bool isGroupChatByName = friendUsername.startsWith("GROUP_"); // Проверка, если это старый групповой чат в списке друзей
            bool isAlreadyInGroupList = groupChats.contains(friendUsername);

            if (isGroupChatByName && !isAlreadyInGroupList) {
                 // Это старый групповой чат, который мог быть в existingFriends как "друг"
                 // Добавляем его в список групповых чатов, если его там еще нет
                 groupChats.append(friendUsername);
                 qDebug() << "Restored offline group chat from existingFriends:" << friendUsername;
            } else if (!isGroupChatByName) {
                 // Это обычный друг, который оффлайн
                 qDebug() << "Restored offline friend from existingFriends:" << friendUsername;
                 currentSessionUserFriends[friendUsername] = false; // Статус оффлайн
                 offlineFriendUsers.append(friendUsername);
            }
        }
    }

    // Обновляем глобальный userFriends
    this->userFriends = currentSessionUserFriends;

    // Удаляем дубликаты из списков для UI
    onlineFriendUsers.removeDuplicates();
    offlineFriendUsers.removeDuplicates();
    groupChats.removeDuplicates();
    
    qDebug() << "After processing: Online friend users:" << onlineFriendUsers.size() 
             << ", Offline friend users:" << offlineFriendUsers.size() 
             << ", Group chats:" << groupChats.size();
    
    // Добавляем категорию "Групповые чаты"
    if (!groupChats.isEmpty()) {
        QListWidgetItem *groupChatHeader = new QListWidgetItem("ГРУППОВЫЕ ЧАТЫ");
        groupChatHeader->setFlags(Qt::NoItemFlags); 
        groupChatHeader->setBackground(Qt::lightGray);
        groupChatHeader->setForeground(Qt::white);
        ui->userListWidget->addItem(groupChatHeader);
        
        for (const QString &groupEntry : groupChats) {
            // groupEntry формат: "chatId:chatName"
            QStringList groupParts = groupEntry.split(":", Qt::SkipEmptyParts);
            QString chatId = groupParts[0];
            QString chatName = groupParts.size() > 1 ? groupParts[1] : chatId;
            
            // Отображаем название группового чата без префикса
            QString displayText = chatName;
            
            QListWidgetItem *item = new QListWidgetItem(displayText);
            item->setForeground(Qt::blue);
            item->setData(Qt::UserRole, chatId); // Сохраняем ID чата для обработки
            item->setData(Qt::UserRole + 1, "G"); // Тип элемента
            item->setData(Qt::UserRole + 2, chatName); // Название чата
            ui->userListWidget->addItem(item);
            qDebug() << "Added group chat to UI list:" << chatId;
        }
    }
    
    // Добавляем категорию "Друзья"
    if (!onlineFriendUsers.isEmpty() || !offlineFriendUsers.isEmpty()) {
        QListWidgetItem *friendsHeader = new QListWidgetItem("ДРУЗЬЯ");
        friendsHeader->setFlags(Qt::NoItemFlags); 
        friendsHeader->setBackground(Qt::lightGray);
        friendsHeader->setForeground(Qt::white);
        ui->userListWidget->addItem(friendsHeader);        
        
        // Добавляем онлайн пользователей (друзей)
        for (const QString &user : onlineFriendUsers) {
            QListWidgetItem *item = new QListWidgetItem(user);
            item->setBackground(Qt::green); 
            item->setForeground(Qt::black);
            item->setData(Qt::UserRole, true); // Статус онлайн
            ui->userListWidget->addItem(item);
            qDebug() << "Added online friend to UI list:" << user << "with green background";
        }
        
        // Добавляем оффлайн пользователей (друзей и недавних не-друзей)
        for (const QString &user : offlineFriendUsers) {
            QListWidgetItem *item = new QListWidgetItem(user);
            item->setBackground(Qt::white); 
            item->setForeground(Qt::gray);
            item->setData(Qt::UserRole, false); // Статус оффлайн
            ui->userListWidget->addItem(item);
            qDebug() << "Added offline user to UI list:" << user << "with white background";
        }
    }
    
    // Если после всех категорий список пустой, добавляем сообщение
    if (ui->userListWidget->count() == 0) {
        QListWidgetItem *emptyItem = new QListWidgetItem("Список пользователей пуст");
        emptyItem->setFlags(Qt::NoItemFlags);
        ui->userListWidget->addItem(emptyItem);    }    
    
    // ВАЖНО: Восстанавливаем сохраненные счетчики уведомлений ПЕРЕД применением к UI
    unreadMessageCounts = savedUnreadMessageCounts;
    unreadGroupMessageCounts = savedUnreadGroupMessageCounts;
    
    // Добавляем метки непрочитанных сообщений к элементам списка
    for (int i = 0; i < ui->userListWidget->count(); ++i) {
        QListWidgetItem *item = ui->userListWidget->item(i);
        if (!item || !(item->flags() & Qt::ItemIsEnabled))
            continue;
            
        QString username = item->text();
        
        // Проверяем приватные сообщения
        if (unreadMessageCounts.contains(username) && unreadMessageCounts[username] > 0) {
            item->setText(username + " (" + QString::number(unreadMessageCounts[username]) + ")");
            item->setBackground(Qt::yellow); // Жёлтый фон для пользователей с непрочитанными сообщениями
            item->setForeground(Qt::black);  // Чёрный текст для читаемости
        }
        
        // Проверяем групповые чаты
        QString itemType = item->data(Qt::UserRole + 1).toString();
        if (itemType == "G") {
            QString chatId = item->data(Qt::UserRole).toString();
            if (unreadGroupMessageCounts.contains(chatId) && unreadGroupMessageCounts[chatId] > 0) {
                QString currentText = item->text();
                // Убираем старый счетчик, если есть
                int parenIndex = currentText.indexOf(" (");
                if (parenIndex != -1) {
                    currentText = currentText.left(parenIndex);
                }
                // Добавляем новый счетчик
                item->setText(currentText + " (" + QString::number(unreadGroupMessageCounts[chatId]) + ")");
                item->setBackground(Qt::yellow);
                item->setForeground(Qt::black);
            }
        }
    }    
    qDebug() << "Final UI user list count:" << ui->userListWidget->count();
    qDebug() << "Applied unread message counts:" << unreadMessageCounts;
    qDebug() << "Applied unread group message counts:" << unreadGroupMessageCounts;
}

void MainWindow::onUserSelected(QListWidgetItem *item)
{
    if (!item || !(item->flags() & Qt::ItemIsEnabled)) {
        return;
    }
    
    QString selectedText = item->text();
    
    // Извлекаем чистое имя пользователя, удаляя счетчик непрочитанных сообщений если он есть
    QString cleanUsername = selectedText;
    int parenIndex = selectedText.indexOf(" (");
    if (parenIndex != -1) {
        cleanUsername = selectedText.left(parenIndex);
    }
    
    // Проверяем, является ли выбранный элемент групповым чатом по Qt::UserRole данным
    QString itemType = item->data(Qt::UserRole + 1).toString();
    if (itemType == "G") {
        // Это групповой чат - получаем chatId из Qt::UserRole
        QString chatId = item->data(Qt::UserRole).toString();
        emit groupChatSelected(chatId);
    } else {
        // Это обычный пользователь
        emit userSelected(cleanUsername);
        
        // Отключаем эту строку, пока не реализуем MVC полностью
        // emit requestPrivateMessageHistory(cleanUsername);
    }
}

void MainWindow::onUserDoubleClicked(QListWidgetItem *item) 
{
    if (item) {
        QString username = item->text();
        
        // Извлекаем чистое имя пользователя, удаляя счетчик непрочитанных сообщений если он есть
        QString cleanUsername = username;
        int parenIndex = username.indexOf(" (");
        if (parenIndex != -1) {
            cleanUsername = username.left(parenIndex);
        }
        
        // Проверяем, что это не категория
        if (!cleanUsername.startsWith("ДРУЗЬЯ") && !cleanUsername.startsWith("ГРУППОВЫЕ")) {
            // Проверяем, является ли выбранный элемент групповым чатом
            QString itemType = item->data(Qt::UserRole + 1).toString();
            if (itemType == "G") {
                // Это групповой чат - получаем chatId из Qt::UserRole
                QString chatId = item->data(Qt::UserRole).toString();
                emit groupChatSelected(chatId);
            } else {
                // Это обычный пользователь
                emit userDoubleClicked(cleanUsername);
            }
        }
    }
}

GroupChatWindow* MainWindow::findOrCreateGroupChatWindow(const QString &chatId, const QString &chatName)
{
    // Используем GroupChatController для создания окна
    if (controller && controller->getGroupChatController()) {
        GroupChatWindow *window = controller->getGroupChatController()->findOrCreateChatWindow(chatId, chatName);
        
        // Обновляем нашу карту для обратной совместимости
        if (window && !groupChatWindows.contains(chatId)) {
            groupChatWindows[chatId] = window;
            
            // Подключаем сигнал закрытия окна для удаления из карты
            connect(window, &GroupChatWindow::destroyed, this, [this, chatId]() {
                groupChatWindows.remove(chatId);
            });
        }
        
        return window;
    }
    
    // Fallback: создаем окно по старому методу, если контроллер недоступен
    if (groupChatWindows.contains(chatId)) {
        return groupChatWindows[chatId];
    }
    
    GroupChatWindow *chatWindow = new GroupChatWindow(chatId, chatName);
    
    if (controller) {
        chatWindow->setController(controller);
    }
    
    connect(chatWindow, &GroupChatWindow::destroyed, this, [this, chatId]() {
        groupChatWindows.remove(chatId);
    });
    
    groupChatWindows[chatId] = chatWindow;
    
    return chatWindow;
}

void MainWindow::on_pushButton_clicked()
{
    // Обработка нажатия кнопки "Создать групповой чат"
    // Используем TransitWindow для создания группового чата
    if (controller) {
        TransitWindow *transitWindow = new TransitWindow(controller, this);
        transitWindow->setModal(true);
        transitWindow->setAttribute(Qt::WA_DeleteOnClose);
        transitWindow->exec();
    } else {
        // Fallback к простому диалогу, если контроллер недоступен
        QString chatName = QInputDialog::getText(this, "Создание группового чата", "Введите название чата:");
        
        if (!chatName.isEmpty()) {
            emit requestCreateGroupChat(chatName);
        }
    }
}

void MainWindow::on_pushButton_2_clicked()
{
    // Обработка нажатия кнопки "Присоединиться к групповому чату"
    bool ok;
    QString chatId = QInputDialog::getText(this, "Присоединение к групповому чату",
                                         "Введите ID группового чата:", QLineEdit::Normal,
                                         "", &ok);
    
    if (ok && !chatId.isEmpty()) {
        emit requestJoinGroupChat(chatId);
    }
}

void MainWindow::on_lineEdit_returnPressed()
{
    // Обработка нажатия Enter в поле поиска
    QString searchQuery = ui->lineEdit->text().trimmed();
    
    if (!searchQuery.isEmpty()) {
        searchUsers();
    }
}

void MainWindow::searchUsers()
{
    QString searchQuery = ui->lineEdit->text().trimmed();
    if (searchQuery.isEmpty()) {
        qDebug() << "Поисковый запрос пустой, поиск не выполняется";
        return;
    }

    // Отправляем запрос на поиск пользователей через контроллер
    emit requestSearchUsers(searchQuery);
    qDebug() << "Отправлен запрос на поиск пользователей:" << searchQuery;
    
    // Отображаем индикатор загрузки или сообщение об ожидании
    ui->statusbar->showMessage("Выполняется поиск пользователей...", 3000);
    
    // Сбрасываем поле поиска
    ui->lineEdit->clear();
}

void MainWindow::showSearchResults(const QStringList &users)
{
    // Проверяем, не пустой ли список результатов
    if (users.isEmpty()) {
        QMessageBox::information(this, "Поиск", "Пользователи не найдены");
        return;
    }
    
    // Фильтруем список пользователей (исключаем текущего пользователя и уже добавленных друзей)
    QStringList filteredUsers;
    for (const QString &user : users) {
        QString username = user;
        // Убираем префиксы типов и статусов, если они есть
        if (username.contains(':')) {
            username = username.split(':').first();
        }
        
        // Пропускаем текущего пользователя и тех, кто уже в друзьях
        if (username != getCurrentUsername() && !userFriends.contains(username)) {
            filteredUsers.append(username);
        }
    }
    
    // Если после фильтрации список пуст
    if (filteredUsers.isEmpty()) {
        QMessageBox::information(this, "Поиск", "Все найденные пользователи уже в вашем списке друзей");
        return;
    }
    
    // Если диалог уже открыт, закрываем его перед созданием нового
    if (searchDialog && searchDialog->isVisible()) {
        searchDialog->close();
    }
    
    // Создаем диалог поиска каждый раз заново
    if (searchDialog) {
        delete searchDialog;
        searchDialog = nullptr;
    }
    
    if (searchListWidget) {
        delete searchListWidget;
        searchListWidget = nullptr;
    }
    
    searchDialog = new QDialog(this);
    searchDialog->setWindowTitle("Результаты поиска");
    searchDialog->setMinimumSize(300, 400);
    
    QVBoxLayout *layout = new QVBoxLayout(searchDialog);
    
    searchListWidget = new QListWidget(searchDialog);
    layout->addWidget(searchListWidget);
    
    QLabel *helpLabel = new QLabel("Выберите пользователя и нажмите 'Добавить' или дважды щелкните на пользователе", searchDialog);
    helpLabel->setWordWrap(true);
    layout->addWidget(helpLabel);
    
    // Добавляем кнопки для действий
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *addButton = new QPushButton("Добавить", searchDialog);
    QPushButton *closeButton = new QPushButton("Закрыть", searchDialog);
    buttonLayout->addWidget(addButton);
    buttonLayout->addWidget(closeButton);
    layout->addLayout(buttonLayout);
    
    // Подключаем сигналы
    connect(closeButton, &QPushButton::clicked, searchDialog, &QDialog::close);
    connect(addButton, &QPushButton::clicked, this, [this]() {
        if (searchListWidget && searchListWidget->currentItem()) {
            QString username = searchListWidget->currentItem()->text();
            addUserToFriends(username);
        }
    });
    connect(searchListWidget, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        if (item) {
            QString username = item->text();
            addUserToFriends(username);
        }
    });
    
    // Добавляем обработчик нажатия Enter
    QShortcut *shortcut = new QShortcut(QKeySequence(Qt::Key_Return), searchListWidget);
    connect(shortcut, &QShortcut::activated, this, [this]() {
        if (searchListWidget && searchListWidget->currentItem()) {
            QString username = searchListWidget->currentItem()->text();
            addUserToFriends(username);
        }
    });
    
    // Очищаем и заполняем список результатов
    searchResults.clear();
    searchListWidget->clear();
    searchResults = filteredUsers;
    
    for (const QString &user : searchResults) {
        searchListWidget->addItem(new QListWidgetItem(user));
    }
    
    // Выбираем первый элемент по умолчанию
    if (searchListWidget->count() > 0) {
        searchListWidget->setCurrentRow(0);
    }
    
    // Подключаем сигнал finished для безопасного удаления диалога
    connect(searchDialog, &QDialog::finished, this, [this]() {
        searchDialog = nullptr;
        searchListWidget = nullptr;
    });
    
    searchDialog->show();
    searchDialog->raise();
    searchDialog->activateWindow();
}


void MainWindow::addUserToFriends(const QString &username)
{
    // Проверяем, не добавляем ли мы самого себя
    if (username == getCurrentUsername()) {
        QMessageBox::warning(this, "Ошибка", "Вы не можете добавить себя в друзья");
        return;
    }
    
    // Проверяем, не добавлен ли уже этот пользователь в друзья
    if (userFriends.contains(username)) {
        QMessageBox::information(this, "Информация", "Пользователь " + username + " уже в вашем списке друзей");
        return;
    }
    
    // Отправляем запрос на добавление пользователя в друзья через контроллер
    emit requestAddUserToFriends(username);
    
    // Добавляем пользователя в список друзей локально для мгновенного отображения в UI
    userFriends[username] = true;
      // Также явно сохраняем друга в списке друзей в ChatController
    if (controller) {
        ChatController* chatController = controller->getChatController();
        if (chatController) {
            // Проверяем, есть ли уже такой друг в списке
            QStringList currentFriends = chatController->getFriendList();
            if (!currentFriends.contains(username)) {
                // Добавляем его в список друзей контроллера
                chatController->addUserToFriends(username);        }
            
            // Friend list is now managed entirely by server, no local persistence needed
            qDebug() << "Friend added for user" << m_username << "with new friend" << username << "- server will handle persistence";
        }
    }
    
    // Закрываем диалог поиска, если он открыт
    if (searchDialog) {
        searchDialog->close();
    }
    
    // Отображаем сообщение об успешном добавлении друга
    QMessageBox::information(this, "Добавление друга", 
                           "Пользователь " + username + " добавлен в список друзей");
    
    // Запрашиваем обновленный список пользователей от сервера
    if (controller) {
        ChatController* chatController = controller->getChatController();
        if (chatController) {
            chatController->requestUserList();
        }
    }
}

QStringList MainWindow::getDisplayedUsers() const
{
    QStringList displayed;
    
    for (int i = 0; i < ui->userListWidget->count(); ++i) {
        QListWidgetItem *item = ui->userListWidget->item(i);
        if (item && (item->flags() & Qt::ItemIsEnabled)) {
            displayed << item->text();
        }
    }
    
    return displayed;
}

void MainWindow::startAddUserToGroupMode(const QString &chatId)
{
    emit requestStartAddUserToGroupMode(chatId);
}

// Handle auth success slot
void MainWindow::onAuthSuccess()
{
    setLoginSuccessful(true);
    
    // Initialize friend list from ChatController
    if (controller) {
        ChatController* chatController = controller->getChatController();
        if (chatController) {
            QStringList friendsList = chatController->getFriendList();
            qDebug() << "Initializing userFriends from ChatController, found" << friendsList.size() << "friends";
            
            // Clear existing friend list and populate with loaded friends
            userFriends.clear();
            for (const QString &friend_name : friendsList) {
                userFriends[friend_name] = false; // Initially set as offline
                qDebug() << "Added friend from settings:" << friend_name;
            }
        }
    }
    
    // Clear login fields to prevent resubmission
    ui_Auth.LineClear();
    // No need to call display() here as it's already called by the controller
}

// Handle successful registration
void MainWindow::onRegisterSuccess()
{
    // Clear registration fields
    ui_Reg.setName("");
    ui_Reg.setPass("");
    
    // Return to authentication window
    ui_Reg.hide();
    ui_Auth.show();
}

void MainWindow::updateWindowTitle()
{
    if (!m_username.isEmpty()) {
        this->setWindowTitle("Текущий пользователь: " + m_username);
    } else {
        this->setWindowTitle("Чат приложение");
    }
}

// Метод для обновления счетчиков непрочитанных сообщений
void MainWindow::updateUnreadCounts(const QMap<QString, int> &counts)
{
    unreadMessageCounts = counts;
    
    // Обновляем список пользователей, чтобы отобразить счетчики
    updateUserList(this->userList);
}

// Метод для обновления счетчиков непрочитанных сообщений для групповых чатов
void MainWindow::updateGroupUnreadCounts(const QMap<QString, int> &counts)
{
    unreadGroupMessageCounts = counts;
      // Обновляем заголовки окон групповых чатов
    for (auto it = groupChatWindows.begin(); it != groupChatWindows.end(); ++it) {
        QString chatId = it.key();
        GroupChatWindow* window = it.value();
        
        if (window) {
            // ИСПРАВЛЕНИЕ: Всегда обновляем счетчик, даже если он равен 0
            int count = unreadGroupMessageCounts.value(chatId, 0);
            // Вызываем слот обновления счетчика в окне чата
            QMetaObject::invokeMethod(window, "onUnreadCountChanged", 
                                     Qt::QueuedConnection, Q_ARG(int, count));
        }
    }
      // Обновляем отображение списка групповых чатов в главном окне
    for (int i = 0; i < ui->userListWidget->count(); ++i) {
        QListWidgetItem *item = ui->userListWidget->item(i);
        if (!item || !(item->flags() & Qt::ItemIsEnabled))
            continue;
        
        // Проверяем, является ли элемент групповым чатом
        QString type = item->data(Qt::UserRole + 1).toString();
        if (type == "G") {
            QString chatId = item->data(Qt::UserRole).toString();
            QString chatName = item->data(Qt::UserRole + 2).toString();
            
            // Всегда получаем актуальное значение счетчика из карты
            int unreadCount = unreadGroupMessageCounts.value(chatId, 0);
            qDebug() << "Group chat" << chatName << "(" << chatId << ") has" << unreadCount << "unread messages";
            
            // Убираем старый счетчик, если он был
            QString cleanName = chatName;
            int parenIndex = cleanName.indexOf(" (");
            if (parenIndex != -1) {
                cleanName = cleanName.left(parenIndex);
            }
            
            if (unreadCount > 0) {
                // Устанавливаем новый текст с счетчиком
                item->setText(cleanName + " (" + QString::number(unreadCount) + ")");
                item->setBackground(Qt::yellow); // Жёлтый фон для чатов с непрочитанными сообщениями
                item->setForeground(Qt::black);  // Чёрный текст для читаемости
            } else {
                // Если непрочитанных сообщений нет, возвращаем обычный вид
                QString cleanName = chatName;
                int parenIndex = cleanName.indexOf(" (");
                if (parenIndex != -1) {
                    cleanName = cleanName.left(parenIndex);
                }
                
                item->setText(cleanName);
                item->setForeground(Qt::blue);
                item->setBackground(Qt::white);
            }
        }
    }
}
