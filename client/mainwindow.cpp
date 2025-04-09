#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QApplication>
#include <QMessageBox>
#include <QListWidgetItem>
#include <QDataStream>
#include <QDebug>
#include "privatechatwindow.h"
#include "groupchatwindow.h"
#include "transitwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::MainWindow),
    mode(Mode::Production) // По умолчанию Production режим
{
    initializeCommon(); // Вынесем общую инициализацию в отдельный метод
}

MainWindow::MainWindow(Mode mode, QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::MainWindow),
    mode(mode)
{
    initializeCommon(); // Используем ту же функцию инициализации
}

void MainWindow::initializeCommon()
{
    ui->setupUi(this);

    // Инициализация сокета
    socket = new QTcpSocket(this);
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::slotReadyRead);
    connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &MainWindow::handleSocketError);


     // Создание таймера, чтобы клиент падал при падении сервера, а просто отключался через время.
    authTimeoutTimer = new QTimer(this);
    authTimeoutTimer->setSingleShot(true);
    connect(authTimeoutTimer, &QTimer::timeout, this, &MainWindow::handleAuthenticationTimeout);    
   


    nextBlockSize = 0;
    currentOperation = None;
    m_loginSuccesfull = false;

    // Подключение сигналов
    connect(&ui_Auth, &auth_window::login_button_clicked, this, &MainWindow::authorizeUser);
    connect(&ui_Auth, &auth_window::register_button_clicked, this, &MainWindow::prepareForRegistration);
    connect(&ui_Reg, &reg_window::register_button_clicked2, this, &MainWindow::registerUser);
    connect(&ui_Reg, &reg_window::returnToAuth, this, [this]() {
        ui_Reg.hide();
        ui_Auth.show();
    });

    this->hide();

    connect(ui->userListWidget, &QListWidget::itemClicked, this, &MainWindow::onUserSelected);

    this->hide();
}
    


MainWindow::~MainWindow()
{
    delete ui;
}

// Функция для обновления списка пользователей
void MainWindow::updateUserList(const QStringList &users)
{
    qDebug() << "Updating user list with users:" << users;

    // Создаем карту статусов для более удобного доступа
    QMap<QString, bool> userStatusMap;
    QString currentUsername = getCurrentUsername();
    
    // Очищаем и заново заполняем список пользователей
    ui->userListWidget->clear();

    // Создаем списки для онлайн/оффлайн пользователей и групповых чатов
    QStringList onlineUsers;
    QStringList offlineUsers;
    QStringList groupChats;

    
    // Разделяем пользователей на категории
    for (const QString &userInfo : users) {
        QStringList parts = userInfo.split(":");
        
        // Для групповых чатов: chatId:1:G:chatName
        // Для пользователей: username:status:U
        if (parts.size() >= 3) {
            QString id = parts[0];
            bool isOnline = (parts[1] == "1");
            QString type = parts[2];

            // Если это групповой чат
            if (type == "G" && parts.size() >= 4) {
                // Групповые чаты помещаем в отдельный список
                groupChats << userInfo;
                continue;
            }

            // Обычные пользователи
            if (type == "U") {
                // Сохраняем статус в карту для обновления приватных окон чата
                userStatusMap[id] = isOnline;

                // Пропускаем текущего пользователя для списков онлайн/оффлайн
                if (id == currentUsername) {
                    continue;
                }

                // Добавляем пользователей в соответствующие списки
                if (isOnline) {
                    onlineUsers << userInfo;
                } else {
                    offlineUsers << userInfo;
                }
            }
        }
    }
    
    // Обновляем статусы в открытых окнах чатов
    updatePrivateChatStatuses(userStatusMap);
    
    // Добавляем текущего пользователя (если он есть в списке)
    for (const QString &userInfo : users) {
        QStringList parts = userInfo.split(":");
        
        if (parts.size() >= 3) {
            QString username = parts[0];
            bool isOnline = parts[1] == "1";
            QString type = parts[2];
            
            // Только если это пользователь, не групповой чат
            if (type == "U" && username == currentUsername) {
                QListWidgetItem *item = new QListWidgetItem("Избранное");
                
                // Устанавливаем фон в зависимости от статусаs
                if (isOnline) {
                    item->setForeground(QBrush(QColor("black")));
                    item->setBackground(QBrush(QColor(200, 255, 200))); // Светло-зеленый фон
                    item->setData(Qt::UserRole, true);  // Статус онлайн
                    item->setData(Qt::UserRole + 1, "U"); // Тип - пользователь
                } else {
                    item->setForeground(QBrush(QColor("gray")));
                    item->setBackground(QBrush(QColor(240, 240, 240))); // Светло-серый фон
                    item->setData(Qt::UserRole, false); // Статус оффлайн
                    item->setData(Qt::UserRole + 1, "U"); // Тип - пользователь
                }

                item->setData(Qt::UserRole + 2, true);

                // Выделение текущего пользователя
                QFont font = item->font();
                font.setBold(true);
                item->setFont(font);
                ui->userListWidget->addItem(item);
                break;
            }
        }
    }

    // Добавляем групповые чаты (с отличительным оформлением)
    for (const QString &chatInfo : groupChats) {
        QStringList parts = chatInfo.split(":");
        if (parts.size() >= 4) {
            QString chatId = parts[0];
            QString chatName = parts[3];

            
            QListWidgetItem *item = new QListWidgetItem("Группа: " + chatName);
            
            // Устанавливаем фон и стиль для группового чата
            item->setForeground(QBrush(QColor("blue")));
            item->setBackground(QBrush(QColor(200, 200, 255))); // Светло-голубой фон
            
            // Сохраняем ID чата и другие данные
            item->setData(Qt::UserRole, true);   // Статус всегда онлайн
            item->setData(Qt::UserRole + 1, "G"); // Тип - группа
            item->setData(Qt::UserRole + 2, chatId); // ID чата
            item->setData(Qt::UserRole + 3, chatName); // Название чата

            // Выделяем жирным шрифтом
            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);            
            ui->userListWidget->addItem(item);
        }
    }
    
    // Добавляем всех онлайн пользователей
    for (const QString &userInfo : onlineUsers) {
        QStringList parts = userInfo.split(":");
        

        if (parts.size() >= 3) {
            QString username = parts[0];

            QListWidgetItem *item = new QListWidgetItem(username);

            // Устанавливаем фон для онлайн пользователя
            item->setForeground(QBrush(QColor("black")));
            item->setBackground(QBrush(QColor(200, 255, 200))); // Светло-зеленый фон
            item->setData(Qt::UserRole, true);  // Статус онлайн
            item->setData(Qt::UserRole + 1, "U"); // Тип - пользователь
            ui->userListWidget->addItem(item);
        }
    }

    // Добавляем всех оффлайн пользователей
    for (const QString &userInfo : offlineUsers) {
        QStringList parts = userInfo.split(":");
        if (parts.size() >= 3) {
            QString username = parts[0];

            QListWidgetItem *item = new QListWidgetItem(username);

            // Устанавливаем фон для оффлайн пользователя
            item->setForeground(QBrush(QColor("gray")));
            item->setBackground(QBrush(QColor(240, 240, 240))); // Светло-серый фон
            item->setData(Qt::UserRole, false); // Статус оффлайн
            item->setData(Qt::UserRole + 1, "U"); // Тип - пользователь
            ui->userListWidget->addItem(item);
        }
    }
}

// Новый метод для обновления статусов в окнах приватных чатов
void MainWindow::updatePrivateChatStatuses(const QMap<QString, bool> &userStatusMap)
{
    // Проходим по всем открытым окнам чатов
    for (auto it = privateChatWindows.begin(); it != privateChatWindows.end(); ++it) {
        QString chatUsername = it.key();
        PrivateChatWindow *chatWindow = it.value();

        // Если пользователь есть в карте статусов
        if (userStatusMap.contains(chatUsername)) {
            bool isOnline = userStatusMap[chatUsername];

            // Обновляем статус в окне чата
            chatWindow->setOfflineStatus(!isOnline);

            qDebug() << "Updated status for chat with" << chatUsername << "to"
                     << (isOnline ? "online" : "offline");
        }
    }
}

// Обработчик для выбора пользователя из списка
void MainWindow::onUserSelected(QListWidgetItem *item)
{

    if (!item) return;

    // Получаем тип выбранного элемента
    QString itemType = item->data(Qt::UserRole + 1).toString();
    bool isOnline = item->data(Qt::UserRole).toBool();
    
    // Если выбран обычный пользователь
    if (itemType == "U") {
        QString selectedUser = item->text();

        // Для элемента "Избранное" используем текущего пользователя
        if (item->data(Qt::UserRole + 2).toBool()) { // Если это "Избранное"
            selectedUser = getCurrentUsername();
        }
        // Удаляем "(Вы)" из имени, если это текущий пользователь
        if (selectedUser.endsWith(" (Вы)")) {
            selectedUser = selectedUser.left(selectedUser.length() - 5);
        }        
        qDebug() << "User selected:" << selectedUser << "(Online: " << (isOnline ? "yes" : "no") << ")";

        // Проверяем, находимся ли мы в режиме добавления пользователя в групповой чат
        if (!pendingGroupChatId.isEmpty()) {
            qDebug() << "В режиме добавления пользователя в групповой чат. ID чата:" << pendingGroupChatId;
            
            // Проверяем, не пытается ли пользователь добавить самого себя
            if (selectedUser == getCurrentUsername()) {
                QMessageBox::warning(this, "Ошибка", "Вы уже являетесь участником этого чата");
                pendingGroupChatId.clear(); // Очищаем режим добавления
                return;
            }
            
            // Отправляем запрос на добавление пользователя в групповой чат
            QString addUserRequest = QString("GROUP_ADD_USER:%1:%2").arg(pendingGroupChatId, selectedUser);
            qDebug() << "Отправляем запрос на добавление пользователя:" << addUserRequest;
            sendMessageToServer(addUserRequest);
            
            // Показываем обратно окно группового чата
            if (groupChatWindows.contains(pendingGroupChatId)) {
                QMessageBox::information(this, "Добавление пользователя", 
                                       "Запрос на добавление пользователя " + selectedUser + " отправлен");
                
                // Обновляем список пользователей в чате
                sendMessageToServer(QString("GET_USERLIST"));
                
                // Запрашиваем обновление информации о чате
                sendMessageToServer(QString("JOIN_GROUP_CHAT:%1").arg(pendingGroupChatId));
                
                groupChatWindows[pendingGroupChatId]->show();
                groupChatWindows[pendingGroupChatId]->activateWindow();
            }
            
            // Сбрасываем режим добавления пользователя
            pendingGroupChatId.clear();
            return;
        }

        // Если окно чата с этим пользователем уже открыто
        if (privateChatWindows.contains(selectedUser)) {
            // Добавляем отображение непрочитанных сообщений даже если окно уже существует
            if (unreadMessages.contains(selectedUser) && !unreadMessages[selectedUser].isEmpty()) {
                for (const UnreadMessage &msg : unreadMessages[selectedUser]) {
                    privateChatWindows[selectedUser]->receiveMessage(msg.sender, msg.message, msg.timestamp);
                }
                // Очищаем непрочитанные сообщения после отображения
                unreadMessages[selectedUser].clear();
            }
            privateChatWindows[selectedUser]->show();
            privateChatWindows[selectedUser]->activateWindow();
        } else {
            // Создаем новое окно чата с правильными параметрами
            PrivateChatWindow *chatWindow = new PrivateChatWindow(selectedUser, this, nullptr);
            
            connect(chatWindow, &PrivateChatWindow::historyDisplayCompleted, this, [this, selectedUser]() {
                if (unreadMessages.contains(selectedUser)) {
                    unreadMessages[selectedUser].clear();
                    qDebug() << "Unread messages for" << selectedUser << "cleared after history display";
                }
            });
            
            // Если пользователь оффлайн, показываем информацию об этом
            if (!isOnline) {
                chatWindow->setOfflineStatus(true);
            }
            
            privateChatWindows[selectedUser] = chatWindow;
            
            
            chatWindow->show();
        }
    }
    // Если выбран групповой чат
    else if (itemType == "G") {
        QString chatName = item->data(Qt::UserRole + 3).toString();
        QString chatId = item->data(Qt::UserRole + 2).toString();
        qDebug() << "Group chat selected: " << chatName << " (ID: " << chatId << ")";
        
        // Проверяем, не открыто ли уже окно для этого чата
        if (groupChatWindows.contains(chatId)) {
            groupChatWindows[chatId]->show();
            groupChatWindows[chatId]->activateWindow();
        } else {
            // Создаем новое окно группового чата
            GroupChatWindow *chatWindow = new GroupChatWindow(chatId, chatName, this);
            groupChatWindows[chatId] = chatWindow;
            
            // Отправляем запрос на присоединение к групповому чату
            QString joinRequest = QString("JOIN_GROUP_CHAT:%1").arg(chatId);
            sendMessageToServer(joinRequest);
            
            chatWindow->show();
        }

    }
}

void MainWindow::SendToServer(QString str)
{
    Data.clear();
    QDataStream out(&Data, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_2);
    out << quint16(0) << str;
    out.device()->seek(0);
    out << quint16(Data.size() - sizeof(quint16));

    if (socket && socket->isValid()) {
        socket->write(Data);
        socket->flush();
    }
}

void MainWindow::sendMessageToServer(const QString &message)
{
    Data.clear();
    QDataStream out(&Data, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_2);
    out << quint16(0) << message;
    out.device()->seek(0);
    out << quint16(Data.size() - sizeof(quint16));

    if(socket && socket->isValid()) {
        socket->write(Data);
        socket->flush();
        qDebug() << "Sent message to server:" << message;
    } else {
        qDebug() << "ERROR: Socket invalid, message not sent:" << message;
    }
}

void MainWindow::slotReadyRead()
{
    // Если получено сообщение, сбрасываем таймер
    if (authTimeoutTimer->isActive()) {
        authTimeoutTimer->stop();
    }

    QDataStream in(socket);
    in.setVersion(QDataStream::Qt_6_2);
    if (in.status() == QDataStream::Ok){
        while(socket->bytesAvailable() > 0){
            if(nextBlockSize == 0){
                if(socket->bytesAvailable() < 2){
                    break;
                }
                in >> nextBlockSize;
            }
            if(socket->bytesAvailable() < nextBlockSize){
                break;
            }
            QString str;
            in >> str;
            nextBlockSize = 0;

            qDebug() << "Received from server:" << str;

            // Обработка начала истории
            if (str == "HISTORY_CMD:BEGIN") {
                qDebug() << "HISTORY_BEGIN received - starting history display";
            }
            // Обработка сообщений истории
            else if (str.startsWith("HISTORY_MSG:")) {
                QString historyData = str.mid(QString("HISTORY_MSG:").length());
                QStringList parts = historyData.split("|", Qt::SkipEmptyParts);

                qDebug() << "History message parts:" << parts.size() << "Raw data:" << historyData;

                if (parts.size() >= 3) {
                    QString timestamp = parts[0];
                    QString sender = parts[1];
                    QString message = parts.mid(2).join("|"); // На случай, если сообщение содержит символы |

                    // Преобразование времени из UTC в локальное
                    QDateTime utcTime = QDateTime::fromString(timestamp, "yyyy-MM-dd hh:mm:ss");
                    utcTime.setTimeZone(QTimeZone::UTC);
                    QDateTime localTime = utcTime.toLocalTime();

                    // Форматируем только время для отображения
                    QString timeOnly = localTime.toString("hh:mm");

                    // Выравниваем сообщение по левому краю
                    QString formattedMessage = QString("[%1] %2: %3").arg(
                        timeOnly,
                        sender,
                        message);
                    
                    qDebug() << "Added history message:" << formattedMessage << "(UTC time:" << timestamp << ", local time:" << timeOnly << ")";
                } else {
                    qDebug() << "Error: Invalid history format. Raw data:" << historyData;
                }
            }
            // Обработка конца истории
            else if (str == "HISTORY_CMD:END") {
                qDebug() << "HISTORY_END received - history display complete";
            }
            // Обработка остальных сообщений как раньше
            else if (str.startsWith("USERLIST:")) {
                QStringList users = str.mid(QString("USERLIST:").length()).split(",");
                qDebug() << "User list received:" << users;
                updateUserList(users);
            }
            else if(str.startsWith("AUTH_SUCCESS")) {  // Изменено на startsWith
                if (currentOperation == Auth || currentOperation == None) {
                    m_loginSuccesfull = true;
                    // Извлекаем имя пользователя, если оно есть
                    if (str.contains(':')) {
                        m_username = str.section(':', 1);
                    }
                    ui_Auth.close();
                    this->show();
                    
                    // Обновление имени окна как имя текущего пользователя
                    updateWindowTitle();
                    
                    currentOperation = None;
                    emit authSuccess();

                    // Отправляем запрос на получение списка пользователей без отображения в чате
                    sendMessageToServer("GET_USERLIST");
                }
                ui_Auth.setButtonsEnabled(true);
            }
            else if(str == "AUTH_FAILED") {
                if (currentOperation == Auth || currentOperation == None) {
                    // Прерываем попытки авторизации на случай дублирования
                    currentOperation = None;

                    // Временно отключаем сигналы/слоты для предотвращения удаления объекта
                    disconnect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);

                    QMessageBox::warning(this, "Authentication Error", "Invalid username or password!");

                    // Переподключаемся
                    connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);

                    m_loginSuccesfull = false;
                    ui_Auth.LineClear();
                    ui_Auth.setButtonsEnabled(true);


                    // Очищаем сокет чтобы избежать дублирования сообщений
                    clearSocketBuffer();
                }
            }
            else if(str == "REGISTER_SUCCESS") {
                if (currentOperation == Register || currentOperation == None) {
                    QMessageBox::information(this, "Success", "Registration successful!");
                    ui_Reg.hide();
                    ui_Auth.show();
                    currentOperation = None;
                    emit registerSuccess();
                    ui_Reg.setButtonsEnabled(true);
                    ui_Auth.setButtonsEnabled(true);
                }
            }
            else if(str == "REGISTER_FAILED") {
                if (currentOperation == Register || currentOperation == None) {
                    QMessageBox::warning(this, "Registration Error", "Username may already exist!");
                    
                    // Переход к окну авторизации после ошибки регистрации
                    ui_Reg.hide();
                    ui_Auth.show();
                    ui_Auth.LineClear(); // Очищаем поля ввода для нового входа
                    
                    currentOperation = None;
                    ui_Reg.setButtonsEnabled(true);
                    ui_Auth.setButtonsEnabled(true);
                }
            }
            else if(str.startsWith("PRIVATE:")) {
                QStringList parts = str.split(":", Qt::SkipEmptyParts);
                if (parts.size() >= 3) {
                    QString sender = parts[1];
                    QString privateMessage = parts.mid(2).join(":");
                    
                    // Пропускаем сообщения, адресованные самому себе
                    if (sender == getCurrentUsername()) {
                        return;
                    }

                    handlePrivateMessage(sender, privateMessage);
                }
            }
            else if (str.startsWith("PRIVATE_HISTORY_CMD:BEGIN:")) {
                currentPrivateHistoryRecipient = str.mid(QString("PRIVATE_HISTORY_CMD:BEGIN:").length());
                receivingPrivateHistory = true;

                if (privateChatWindows.contains(currentPrivateHistoryRecipient)) {
                    privateChatWindows[currentPrivateHistoryRecipient]->beginHistoryDisplay();
                }
            }
            else if (str.startsWith("PRIVATE_HISTORY_MSG:")) {
                if (receivingPrivateHistory && !currentPrivateHistoryRecipient.isEmpty() &&
                    privateChatWindows.contains(currentPrivateHistoryRecipient)) {

                    QString historyData = str.mid(QString("PRIVATE_HISTORY_MSG:").length());
                    QStringList parts = historyData.split("|", Qt::SkipEmptyParts);

                    if (parts.size() >= 4) {
                        QString timestamp = parts[0];
                        QString sender = parts[1];
                        QString recipient = parts[2];
                        QString message = parts.mid(3).join("|");

                        // Преобразование времени из UTC в локальное
                        QDateTime utcTime = QDateTime::fromString(timestamp, "yyyy-MM-dd hh:mm:ss");
                        utcTime.setTimeZone(QTimeZone::UTC);
                        QDateTime localTime = utcTime.toLocalTime();

                        // Форматируем только время для отображения
                        QString timeOnly = localTime.toString("hh:mm");

                        QString formattedMsg;
                        if (sender == getCurrentUsername()) {
                            formattedMsg = QString("[%1] Вы: %2").arg(timeOnly, message);
                        } else {
                            formattedMsg = QString("[%1] %2: %3").arg(timeOnly, sender, message);
                        }

                        privateChatWindows[currentPrivateHistoryRecipient]->addHistoryMessage(formattedMsg);
                    }
                }
            }
            else if (str == "PRIVATE_HISTORY_CMD:END") {
                if (receivingPrivateHistory && !currentPrivateHistoryRecipient.isEmpty() &&
                    privateChatWindows.contains(currentPrivateHistoryRecipient)) {

                    privateChatWindows[currentPrivateHistoryRecipient]->endHistoryDisplay();
                    
                    if (unreadMessages.contains(currentPrivateHistoryRecipient)) {
                        unreadMessages[currentPrivateHistoryRecipient].clear();
                    }
                }

                receivingPrivateHistory = false;
                currentPrivateHistoryRecipient.clear();
            }
            else if (str.startsWith("GROUP_CHAT_CREATED:")) {
                // Получаем ID и имя созданного чата
                QStringList parts = str.mid(QString("GROUP_CHAT_CREATED:").length()).split(":");
                if (parts.size() >= 2) {
                    QString chatId = parts[0];
                    QString chatName = parts[1];
                    qDebug() << "Group chat created: " << chatName << " (ID: " << chatId << ")";
                    
                    // Запрашиваем обновление списка пользователей и чатов
                    sendMessageToServer("GET_USERLIST");
                }
            }
            else if (str.startsWith("GROUP_MESSAGE:")) {
                QStringList parts = str.split(":", Qt::SkipEmptyParts);
                if (parts.size() >= 4) {
                    QString chatId = parts[1];
                    QString sender = parts[2];
                    QString message = parts.mid(3).join(":");
                   
                    // Если окно этого чата открыто, отправляем сообщение туда
                    if (groupChatWindows.contains(chatId)) {
                        groupChatWindows[chatId]->receiveMessage(sender, message);
                        groupChatWindows[chatId]->activateWindow();
                    }
                }
            }
            else if (str.startsWith("GROUP_CHAT_INFO:")) {
                QStringList parts = str.split(":", Qt::SkipEmptyParts);
                if (parts.size() >= 3) {
                    QString chatId = parts[1];
                    QString chatName = parts[2];
                    QStringList members;
                    
                    // Если в сообщении есть список участников
                    if (parts.size() >= 4) {
                        members = parts[3].split(",");
                    }
                    
                    // Если окно этого чата открыто, обновляем список участников
                    if (groupChatWindows.contains(chatId)) {
                        groupChatWindows[chatId]->updateMembersList(members);
                    }
                }
            }
            // Добавляем новую обработку для сообщения о создателе чата, которое приходит в формате "username: GROUP_GET_CREATOR:chatId"
            else if (str.contains("GROUP_GET_CREATOR:")) {
                QStringList messageParts = str.split("GROUP_GET_CREATOR:", Qt::SkipEmptyParts);
                if (messageParts.size() >= 2) {
                    QString chatId = messageParts[1].trimmed();
                    // Получаем имя создателя из первой части сообщения
                    QString creator = messageParts[0].split(":")[0].trimmed();
                    
                    qDebug() << "Обнаружен создатель чата:" << creator << "для чата ID:" << chatId;
                    
                    // Если окно этого чата открыто, устанавливаем создателя
                    if (groupChatWindows.contains(chatId)) {
                        groupChatWindows[chatId]->setCreator(creator);
                        qDebug() << "Установлен создатель чата для ID:" << chatId;
                    }
                }
            }
            else if (str.startsWith("GROUP_CHAT_CREATOR:")) {
                QStringList parts = str.split(":", Qt::SkipEmptyParts);
                if (parts.size() >= 3) {
                    QString chatId = parts[1];
                    QString creatorName = parts[2];
                    
                    // Обновляем информацию о создателе в окне чата
                    if (groupChatWindows.contains(chatId)) {
                        groupChatWindows[chatId]->setCreator(creatorName);
                    }
                }
            }
            else if (str.startsWith("GROUP_HISTORY_BEGIN:")) {
                QString chatId = str.mid(QString("GROUP_HISTORY_BEGIN:").length());
                
                if (groupChatWindows.contains(chatId)) {
                    groupChatWindows[chatId]->beginHistoryDisplay();
                }
            }
            else if (str.startsWith("GROUP_HISTORY_MSG:")) {
                // Изменяем разделение строки - используем вертикальную черту вместо двоеточия
                QString historyData = str.mid(QString("GROUP_HISTORY_MSG:").length());
                QStringList parts = historyData.split("|", Qt::SkipEmptyParts);
                
                if (parts.size() >= 4) { // chatId|timestamp|sender|message
                    QString chatId = parts[0];
                    QString timestamp = parts[1];
                    QString sender = parts[2];
                    QString message = parts[3];
                    
                    if (groupChatWindows.contains(chatId)) {
                        // Преобразование времени из UTC в локальное
                        QDateTime utcTime = QDateTime::fromString(timestamp, "yyyy-MM-dd hh:mm:ss");
                        utcTime.setTimeZone(QTimeZone::UTC);
                        QDateTime localTime = utcTime.toLocalTime();
                        
                        // Форматируем только время для отображения
                        QString timeOnly = localTime.toString("hh:mm");
                        
                        QString formattedMsg;
                        if (sender == "SYSTEM") {
                            formattedMsg = QString("<i>[%1] %2</i>").arg(timeOnly, message);
                        } else {
                            formattedMsg = QString("[%1] %2: %3").arg(timeOnly, sender, message);
                        }
                        
                        groupChatWindows[chatId]->addHistoryMessage(formattedMsg);
                    }
                }
            }
            else if (str.startsWith("GROUP_HISTORY_END:")) {
                QString chatId = str.mid(QString("GROUP_HISTORY_END:").length());
                
                if (groupChatWindows.contains(chatId)) {
                    groupChatWindows[chatId]->endHistoryDisplay();
                }
            }
            else if (str.startsWith("GROUP_CHAT_DELETED:")) {
                QString chatId = str.mid(QString("GROUP_CHAT_DELETED:").length());
                
                // Если окно этого чата ещё открыто, закрываем его
                if (groupChatWindows.contains(chatId)) {
                    groupChatWindows[chatId]->close();
                    groupChatWindows.remove(chatId); // Удаляем из списка окон
                }
                
                // Обновляем список чатов
                sendMessageToServer("GET_USERLIST");
                
                QMessageBox::information(this, "Чат удален", 
                                       "Групповой чат был успешно удален.");
            }
            else {
                bool isDuplicate = false;

                // Игнорировать системные сообщения
                if (str.startsWith("GET_") || str == "GET_USERLIST") {
                    isDuplicate = true;
                }

                if (recentSentMessages.contains(str)) {
                    isDuplicate = true;
                }

                if (!isDuplicate) {

                }
            }
        }
    }
    else{
        // ui->textBrowser->append("read error");
    }
}

void MainWindow::setLogin(const QString &login) {
    m_username = login;
}

bool MainWindow::isLoginSuccessful() const {
    return m_loginSuccesfull;
}

bool MainWindow::isConnected() const {
    return socket && socket->state() == QAbstractSocket::ConnectedState;
}

bool MainWindow::hasPrivateChatWith(const QString &username) const
{
    return privateChatWindows.contains(username);
}

int MainWindow::privateChatsCount() const
{
    return privateChatWindows.size();
}

QStringList MainWindow::privateChatParticipants() const
{
    return privateChatWindows.keys();
}

QStringList MainWindow::getOnlineUsers() const {
    QStringList onlineUsers;

    for (int i = 0; i < ui->userListWidget->count(); ++i) {
        QListWidgetItem* item = ui->userListWidget->item(i);
        if (!item) continue;

        bool isOnline = item->data(Qt::UserRole).toBool();
        QString type = item->data(Qt::UserRole + 1).toString();
        bool isFavorite = item->data(Qt::UserRole + 2).toBool();

        if (isOnline && type == "U" && !isFavorite) {
            onlineUsers << item->text();
        }
    }
    return onlineUsers;
}

QStringList MainWindow::getDisplayedUsers() const {
    QStringList users;

    for (int i = 0; i < ui->userListWidget->count(); ++i) {
        QListWidgetItem* item = ui->userListWidget->item(i);
        if (!item) continue;

        QString type = item->data(Qt::UserRole + 1).toString();
        bool isFavorite = item->data(Qt::UserRole + 2).toBool();

        if (type == "U" && !isFavorite) {
            users << item->text();
        }
    }
    return users;
}

QString MainWindow::getCurrentUsername() const
{
    return m_username;
}

void MainWindow::setPass(const QString &pass) {
    m_userpass = pass;
}

QString MainWindow::getUsername() const {
    return m_username;
}


void MainWindow::on_pushButton_clicked()
{
    // Создаем экземпляр окна создания группового чата
    TransitWindow *transitWindow = new TransitWindow(this);
    transitWindow->setModal(true);
    transitWindow->exec();
    
    // TransitWindow удаляется автоматически после закрытия
    // благодаря установленному флагу Qt::WA_DeleteOnClose
    transitWindow->setAttribute(Qt::WA_DeleteOnClose);
}

void MainWindow::display()
{
    ui_Auth.show();
}

void MainWindow::testAuthorizeUser(const QString& username, const QString& password) {
    clearSocketBuffer();
    currentOperation = Auth;

    m_username = username;
    m_userpass = password;

    QString authString = QString("AUTH:%1:%2").arg(m_username, m_userpass);

    Data.clear();
    QDataStream out(&Data, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_2);
    out << quint16(0) << authString;
    out.device()->seek(0);
    out << quint16(Data.size() - sizeof(quint16));
    socket->write(Data);
    socket->waitForBytesWritten();
}

bool MainWindow::testRegisterUser(const QString& username, const QString& password) {
    if (mode == Mode::Testing) {
        return testDb->registerUser(username, password);
    }
    clearSocketBuffer();
    currentOperation = Register;

    m_username = username;
    m_userpass = password;

    QString regString = QString("REGISTER:%1:%2").arg(m_username, m_userpass);

    Data.clear();
    QDataStream out(&Data, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_2);
    out << quint16(0) << regString;
    out.device()->seek(0);
    out << quint16(Data.size() - sizeof(quint16));
    socket->write(Data);
    socket->waitForBytesWritten();

    return false;
}

// авторизация и регистрация пользователя
void MainWindow::authorizeUser()
{
    //  Предотвращение множественных запросов авторизации
    if (currentOperation == Auth) {
        qDebug() << "Authentication already in progress, ignoring request";
        return;
    }

    // Отключаемся и подключаемся заново, чтобы обеспечить чистое состояние соединения
    if(socket && socket->state() == QAbstractSocket::ConnectedState) {
        clearSocketBuffer();
    } else {
        // Если сокет не подключен, переподключаемся
        socket->abort();
        socket->connectToHost("127.0.0.1", 5402);
        if(!socket->waitForConnected(3000)) {
            QMessageBox::critical(this, "Connection Error", "Could not connect to server");
            ui_Auth.setButtonsEnabled(true); // Включаем кнопки пользователю
            return;
        }
    }

    currentOperation = Auth;

    m_username = ui_Auth.getLogin();
    m_userpass = ui_Auth.getPass();

    ui_Auth.setButtonsEnabled(false);
    QApplication::processEvents();

    QString authString = QString("AUTH:%1:%2").arg(m_username, m_userpass);

    Data.clear();
    QDataStream out(&Data, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_2);
    out << quint16(0) << authString;
    out.device()->seek(0);
    out << quint16(Data.size() - sizeof(quint16));
    socket->write(Data);
    socket->flush();
    socket->waitForBytesWritten();

    // Если вдруг будем сервак ставить на хостинг то таймаут нужен, чтобы кнопки включить после 5 секунд отсутствия ответа от сервера
    authTimeoutTimer->start(5000);

    qDebug() << "Sent authentication request:" << authString;
}

bool MainWindow::connectToServer(const QString& host, quint16 port)
{
    if (socket && socket->state() != QAbstractSocket::UnconnectedState) {
        socket->disconnectFromHost();
        if (socket->state() != QAbstractSocket::UnconnectedState) {
            socket->waitForDisconnected();
        }
        socket->deleteLater();
    }

    socket = new QTcpSocket(this);
    socket->connectToHost(host, port);

    if (!socket->waitForConnected(3000)) {
        qDebug() << "Failed to connect:" << socket->errorString();
        return false;
    }
    return true;
}

void MainWindow::registerUser()
{
    // Тоже самое что для авторизации
    if(socket && socket->state() == QAbstractSocket::ConnectedState) {
        clearSocketBuffer();
    } else {
        socket->abort();
        socket->connectToHost("127.0.0.1", 5402);
        if(!socket->waitForConnected(3000)) {
            QMessageBox::critical(this, "Connection Error", "Could not connect to server");
            ui_Reg.setButtonsEnabled(true);
            return;
        }
    }
    currentOperation = Register;

    if(!ui_Reg.checkPass()) {
        QMessageBox::warning(this, "Registration Error", "Passwords don't match!");
        ui_Reg.ConfirmClear();
        return;
    }

    m_username = ui_Reg.getName();
    m_userpass = ui_Reg.getPass();

    ui_Reg.setButtonsEnabled(false);
    QApplication::processEvents();

    QString regString = QString("REGISTER:%1:%2").arg(m_username, m_userpass);

    Data.clear();
    QDataStream out(&Data, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_2);
    out << quint16(0) << regString;
    out.device()->seek(0);
    out << quint16(Data.size() - sizeof(quint16));
    socket->write(Data);
    socket->flush();
    socket->waitForBytesWritten();

    authTimeoutTimer->start(5000);


    qDebug() << "Sent registration request:" << regString;
}

void MainWindow::prepareForRegistration()
{
    clearSocketBuffer();
    currentOperation = None;
    ui_Reg.show();
    ui_Auth.hide();
}

void MainWindow::clearSocketBuffer()
{
    if (socket && socket->bytesAvailable() > 0) {
        qDebug() << "Clearing socket buffer with " << socket->bytesAvailable() << " bytes available";
        socket->readAll();
        socket->flush();
    }
    nextBlockSize = 0;

    if (currentOperation != Auth && currentOperation != Register) {
        currentOperation = None;
    }
}

void MainWindow::handlePrivateMessage(const QString &sender, const QString &message) {
    QTime currentTime = QTime::currentTime();
    QString timeStr = currentTime.toString("hh:mm");

    // Создаем окно чата, если его нет
    if (!privateChatWindows.contains(sender)) {
        PrivateChatWindow *chatWindow = new PrivateChatWindow(sender, this, nullptr);
        privateChatWindows[sender] = chatWindow;
        qDebug() << "Создано новое окно чата с" << sender;
    }

    // Доставляем сообщение
    if (privateChatWindows[sender]->isVisible()) {
        privateChatWindows[sender]->receiveMessage(sender, message);
    } else {
        UnreadMessage unread;
        unread.sender = sender;
        unread.message = message;
        unread.timestamp = timeStr;
        unreadMessages[sender].append(unread);
        qDebug() << "Сохранено непрочитанное сообщение от" << sender << ":" << message;
    }
}

// Замена метода processJsonMessage на обработку обычных сообщений
void MainWindow::sendPrivateMessage(const QString &recipient, const QString &message)
{
    if (recipient == getCurrentUsername()) {
        return;
    }

    qDebug() << "MainWindow: Подготовка приватного сообщения для" << recipient << ":" << message;

    // Формируем строку в формате "PRIVATE:получатель:сообщение"
    QString privateMessage = QString("PRIVATE:%1:%2").arg(recipient, message);

    qDebug() << "MainWindow: Сообщение для отправки:" << privateMessage;

    sendMessageToServer(privateMessage);
}

// Метод для поиска или создания окна приватного чата
PrivateChatWindow* MainWindow::findOrCreatePrivateChatWindow(const QString &username)
{
    if (privateChatWindows.contains(username)) {
        privateChatWindows[username]->show();
        privateChatWindows[username]->activateWindow();
        return privateChatWindows[username];
    } else {
        // Исправляем создание окна с правильными параметрами
        PrivateChatWindow *chatWindow = new PrivateChatWindow(username, this, nullptr);
        
        connect(chatWindow, &PrivateChatWindow::historyDisplayCompleted, this, [this, username]() {
            if (unreadMessages.contains(username)) {
                unreadMessages[username].clear();
            }
        });
        
        privateChatWindows[username] = chatWindow;
        chatWindow->show();
        return chatWindow;
    }
}

void MainWindow::requestPrivateMessageHistory(const QString &otherUser)
{
    QString request = QString("GET_PRIVATE_HISTORY:%1").arg(otherUser);
    sendMessageToServer(request);
}

// Новые методы обработки отсутсвия ответа от сервера и ошибок сокета
void MainWindow::handleAuthenticationTimeout()
{
    qDebug() << "Authentication/Registration timed out";


    if (currentOperation == Auth) {
        QMessageBox::warning(this, "Authentication Error",
                             "Server did not respond in time. Please try again.");
        ui_Auth.setButtonsEnabled(true);
    }
    else if (currentOperation == Register) {
        QMessageBox::warning(this, "Registration Error",
                             "Server did not respond in time. Please try again.");
        ui_Reg.setButtonsEnabled(true);
    }
    currentOperation = None;
}

// Ошибки сокета
void MainWindow::handleSocketError(QAbstractSocket::SocketError socketError)
{
    QString errorMessage;

    switch (socketError) {
    case QAbstractSocket::RemoteHostClosedError:
        errorMessage = "The server closed the connection.";
        break;
    case QAbstractSocket::HostNotFoundError:
        errorMessage = "The server was not found.";
        break;
    case QAbstractSocket::ConnectionRefusedError:
        errorMessage = "The connection was refused by the server.";
        break;
    default:
        errorMessage = "Socket error: " + socket->errorString();
    }

    qDebug() << "Socket error:" << errorMessage;

    // Управление кнопками если произошла ошибка
    if (currentOperation == Auth) {
        QMessageBox::critical(this, "Connection Error",
                              "Error during authentication: " + errorMessage + "\nPlease try again.");
        ui_Auth.setButtonsEnabled(true);
    }
    else if (currentOperation == Register) {
        QMessageBox::critical(this, "Connection Error",
                              "Error during registration: " + errorMessage + "\nPlease try again.");
        ui_Reg.setButtonsEnabled(true);
    }
    else {
        QMessageBox::critical(this, "Connection Error",
                              "Connection to server failed: " + errorMessage);
    }

    // Сброс текущей операции
    currentOperation = None;

    if (authTimeoutTimer->isActive()) {
        authTimeoutTimer->stop();
    }
}

// Метод для создания группового чата - отправляем запрос на сервер
void MainWindow::createGroupChat(const QString &chatName, const QString &chatId)
{
    // Отправляем запрос на сервер о создании группового чата
    // Меняем порядок параметров: сначала имя чата, потом ID
    QString createChatMessage = QString("CREATE_GROUP_CHAT:%1:%2").arg(chatName, chatId);
    sendMessageToServer(createChatMessage);
    
    QMessageBox::information(this, "Создание чата", 
        "Запрос на создание группового чата \"" + chatName + "\" отправлен на сервер.");
}

// Добавим метод для присоединения к групповому чату
void MainWindow::joinGroupChat(const QString &chatId)
{
    QString joinRequest = QString("JOIN_GROUP_CHAT:%1").arg(chatId);
    sendMessageToServer(joinRequest);
}

// Добавляем метод для установки режима добавления пользователя в групповой чат
void MainWindow::startAddUserToGroupMode(const QString &chatId)
{
    pendingGroupChatId = chatId;
    qDebug() << "Активирован режим добавления пользователя для группового чата с ID:" << chatId;
    
    // Обновляем список пользователей, чтобы быть уверенными, что он актуален
    sendMessageToServer("GET_USERLIST");
    
    // Активируем главное окно, чтобы пользователь мог выбрать участника
    this->activateWindow();
    this->raise();
}

void MainWindow::on_pushButton_2_clicked()
{
    // Пустая реализация, но метод необходим для Qt auto-connection
}

void MainWindow::on_lineEdit_returnPressed()
{
    // Пустая реализация, но метод необходим для Qt auto-connection
}
