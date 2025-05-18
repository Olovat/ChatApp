#include "qt_main_window.h"
#include "qdatetime.h"
#include "qtimezone.h"
#include "ui_mainwindow.h"
#include <QApplication>
#include <QMessageBox>
#include <QListWidgetItem>
#include <QDataStream>
#include <QDebug>
#include <QVBoxLayout>
#include <QShortcut>
#include <QKeySequence>
#include <QTimer>

QtMainWindow::QtMainWindow(QWidget *parent)
    : QtMainWindow(Mode::Production, parent) {}

QtMainWindow::QtMainWindow(Mode mode, QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::MainWindow),
    ui_Auth(new QtAuthWindow(this)),
    ui_Reg(new QtRegWindow(this)),
    m_mode(mode)
{
    initializeCommon();
}

void QtMainWindow::initializeCommon()
{
    ui->setupUi(this);

    // Инициализация сокета
    socket = new QTcpSocket(this);
    connect(socket, &QTcpSocket::readyRead, this, &QtMainWindow::slotReadyRead);
    connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &QtMainWindow::handleSocketError);

    // Подключение поля поиска
    connect(ui->lineEdit, &QLineEdit::returnPressed, this, &QtMainWindow::on_lineEdit_returnPressed);

    // Инициализация переменных для поиска
    searchDialog = nullptr;
    searchListWidget = nullptr;

    // Настройка таймера аутентификации
    authTimeoutTimer = new QTimer(this);
    authTimeoutTimer->setSingleShot(true);
    connect(authTimeoutTimer, &QTimer::timeout, this, &QtMainWindow::handleAuthenticationTimeout);

    // Инициализация счетчиков сообщений
    unreadPrivateMessageCounts.clear();
    unreadGroupMessageCounts.clear();

    nextBlockSize = 0;
    currentOperation = None;
    m_loginSuccesfull = false;

    // Подключение сигналов окон авторизации и регистрации
    connect(ui_Auth, &QtAuthWindow::login_Button_Clicked, this, &QtMainWindow::authorizeUser);
    connect(ui_Auth, &QtAuthWindow::register_Button_Clicked, this, &QtMainWindow::prepareForRegistration);
    connect(ui_Reg, &QtRegWindow::register_button_clicked2, this, &QtMainWindow::registerUser);
    connect(ui_Reg, &QtRegWindow::returnToAuth, this, [this]() {
        ui_Reg->hide();
        ui_Auth->show();
    });

    connect(ui->userListWidget, &QListWidget::itemClicked, this, &QtMainWindow::onUserSelected);

    this->hide();
}

QtMainWindow::~QtMainWindow()
{
    delete ui;
}

void QtMainWindow::setTestCredentials(const std::string& login, const std::string& pass)
{
    m_username = login;
    m_userpass = pass;
}

std::string QtMainWindow::getUsername() const
{
    return m_username;
}

std::string QtMainWindow::getUserpass() const
{
    return m_userpass;
}

bool QtMainWindow::connectToServer(const std::string& host, unsigned short port)
{
    if (socket && socket->state() != QAbstractSocket::UnconnectedState) {
        socket->disconnectFromHost();
        if (socket->state() != QAbstractSocket::UnconnectedState) {
            socket->waitForDisconnected();
        }
        socket->deleteLater();
    }

    socket = new QTcpSocket(this);
    socket->connectToHost(QString::fromStdString(host), port);

    if (!socket->waitForConnected(3000)) {
        qDebug() << "Failed to connect:" << socket->errorString();
        return false;
    }
    return true;
}

void QtMainWindow::sendMessageToServer(const std::string& message)
{
    Data.clear();
    QDataStream out(&Data, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_2);
    out << quint16(0) << QString::fromStdString(message);
    out.device()->seek(0);
    out << quint16(Data.size() - sizeof(quint16));

    if(socket && socket->isValid()) {
        socket->write(Data);
        socket->flush();
        qDebug() << "Sent message to server:" << QString::fromStdString(message);
    } else {
        qDebug() << "ERROR: Socket invalid, message not sent:" << QString::fromStdString(message);
    }
}

void QtMainWindow::updateUserList(const QStringList &users)
{
    qDebug() << "Updating user list with users:" << users;

    // Очищаем и заново заполняем список пользователей
    ui->userListWidget->clear();

    // Получаем текущее имя пользователя через интерфейс
    QString currentUsername = QString::fromStdString(getUsername());

    // Создаем списки для онлайн/оффлайн пользователей и групповых чатов
    QStringList onlineUsers;
    QStringList offlineUsers;
    QStringList groupChats;

    // Разделяем пользователей на категории
    for (const QString &userInfo : users) {
        QStringList parts = userInfo.split(":");

        if (parts.size() >= 4) {
            QString id = parts[0];
            bool isOnline = (parts[1] == "1");
            QString type = parts[2];
            bool isFriend = (parts.size() >= 4 && parts[3] == "F");

            // Если это групповой чат
            if (type == "G") {
                groupChats << userInfo;
                continue;
            }

            // Только друзья и текущий пользователь
            if (type == "U" && (isFriend || id == currentUsername)) {
                if (id != currentUsername) {
                    if (isOnline) {
                        onlineUsers << userInfo;
                    } else {
                        offlineUsers << userInfo;
                    }
                }
            }
        }
    }

    // Добавляем текущего пользователя (Избранное)
    if (!currentUsername.isEmpty()) {
        QString displayName = currentUsername + " (Вы)";
        QListWidgetItem *item = new QListWidgetItem(displayName);
        item->setForeground(QBrush(QColor("black")));
        item->setBackground(QBrush(QColor(200, 255, 200)));
        item->setData(Qt::UserRole, true);
        item->setData(Qt::UserRole + 1, "U");

        QFont font = item->font();
        font.setBold(true);
        item->setFont(font);
        ui->userListWidget->addItem(item);
    }

    // Добавляем групповые чаты
    for (const QString &chatInfo : groupChats) {
        QStringList parts = chatInfo.split(":");
        if (parts.size() >= 4) {
            QString chatId = parts[0];
            QString chatName = parts[3];

            QString displayName = "Группа: " + chatName;
            QListWidgetItem *item = new QListWidgetItem(displayName);

            item->setForeground(QBrush(QColor("blue")));
            item->setBackground(QBrush(QColor(200, 200, 255)));
            item->setData(Qt::UserRole, true);
            item->setData(Qt::UserRole + 1, "G");
            item->setData(Qt::UserRole + 2, chatId);
            item->setData(Qt::UserRole + 3, chatName);

            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
            ui->userListWidget->addItem(item);
        }
    }

    // Добавляем онлайн пользователей
    for (const QString &userInfo : onlineUsers) {
        QStringList parts = userInfo.split(":");
        if (parts.size() >= 3) {
            QString username = parts[0];
            QListWidgetItem *item = new QListWidgetItem(username);
            item->setForeground(QBrush(QColor("black")));
            item->setBackground(QBrush(QColor(200, 255, 200)));
            item->setData(Qt::UserRole, true);
            item->setData(Qt::UserRole + 1, "U");
            ui->userListWidget->addItem(item);
        }
    }

    // Добавляем оффлайн пользователей
    for (const QString &userInfo : offlineUsers) {
        QStringList parts = userInfo.split(":");
        if (parts.size() >= 3) {
            QString username = parts[0];
            QListWidgetItem *item = new QListWidgetItem(username);
            item->setForeground(QBrush(QColor("gray")));
            item->setBackground(QBrush(QColor(240, 240, 240)));
            item->setData(Qt::UserRole, false);
            item->setData(Qt::UserRole + 1, "U");
            ui->userListWidget->addItem(item);
        }
    }

    // Обновляем статусы в окнах приватных чатов
    QMap<QString, bool> userStatusMap;
    for (const QString &userInfo : users) {
        QStringList parts = userInfo.split(":");
        if (parts.size() >= 3 && parts[2] == "U") {
            userStatusMap[parts[0]] = (parts[1] == "1");
        }
    }
    updatePrivateChatStatuses(userStatusMap);
}

void QtMainWindow::updatePrivateChatStatuses(const QMap<QString, bool> &userStatusMap)
{
    // Проходим по всем открытым окнам приватных чатов
    for (auto it = privateChatWindows.begin(); it != privateChatWindows.end(); ++it) {
        QString chatUsername = it.key();
        QtPrivateChatWindow *chatWindow = it.value();  // Используем QtPrivateChatWindow вместо PrivateChatWindow

        // Если пользователь есть в карте статусов
        if (userStatusMap.contains(chatUsername)) {
            bool isOnline = userStatusMap[chatUsername];

            // Обновляем статус в окне чата
            chatWindow->setOfflineStatus(!isOnline);  // Предполагая, что QtPrivateChatWindow имеет такой метод

            if (m_mode == Mode::Testing) {
                qDebug() << "[TEST] Updated status for chat with" << chatUsername << "to"
                         << (isOnline ? "online" : "offline");
            }
        }
    }
}

void QtMainWindow::onUserSelected(QListWidgetItem *item)
{
    if (!item) return;

    // Получаем тип выбранного элемента
    QString itemType = item->data(Qt::UserRole + 1).toString();
    bool isOnline = item->data(Qt::UserRole).toBool();

    // Если выбран обычный пользователь
    if (itemType == "U") {
        QString selectedUser = item->text();

        // Удаляем счетчик из текста, если он есть
        if (selectedUser.contains(" (")) {
            selectedUser = selectedUser.left(selectedUser.indexOf(" ("));
        }

        // Получаем текущего пользователя через интерфейс
        QString currentUsername = QString::fromStdString(getUsername());

        // Для элемента "Избранное" используем текущего пользователя
        if (item->data(Qt::UserRole + 2).toBool()) {
            selectedUser = currentUsername;
        } else if (item->data(Qt::UserRole + 4).toString().length() > 0) {
            selectedUser = item->data(Qt::UserRole + 4).toString();
        }

        // Удаляем "(Вы)" из имени, если это текущий пользователь
        if (selectedUser.endsWith(" (Вы)")) {
            selectedUser = currentUsername;
        }

        qDebug() << "User selected:" << selectedUser << "(Online: " << isOnline << ")";

        // Если окно чата уже открыто
        if (privateChatWindows.contains(selectedUser)) {
            privateChatWindows[selectedUser]->show();
            privateChatWindows[selectedUser]->activateWindow();
        } else {
            // Создаем новое окно чата
            QtPrivateChatWindow *chatWindow = findOrCreatePrivateChatWindow(selectedUser);

            // Если пользователь оффлайн
            if (!isOnline) {
                chatWindow->setOfflineStatus(true);
            }

            chatWindow->show();
        }
    }
    // Если выбран групповой чат
    else if (itemType == "G") {
        QString chatName = item->data(Qt::UserRole + 3).toString();
        QString chatId = item->data(Qt::UserRole + 2).toString();

        qDebug() << "Group chat selected:" << chatName << "(ID:" << chatId << ")";

        // Если окно уже открыто
        if (groupChatWindows.contains(chatId)) {
            groupChatWindows[chatId]->show();
            groupChatWindows[chatId]->activateWindow();
        } else {
            // Создаем новое окно группового чата
            QtGroupChatWindow *chatWindow = new QtGroupChatWindow(chatId, chatName, this);
            groupChatWindows[chatId] = chatWindow;

            // Отправляем запрос на присоединение
            sendMessageToServer(QString("JOIN_GROUP_CHAT:%1").arg(chatId).toStdString());

            chatWindow->show();
        }
    }
}

void QtMainWindow::SendToServer(QString str)
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

void QtMainWindow::setLogin(const std::string &login) {
    m_username = login;
}

void QtMainWindow::slotReadyRead()
{
    // Если получено сообщение, сбрасываем таймер
    if (authTimeoutTimer->isActive()) {
        authTimeoutTimer->stop();
    }

    QDataStream in(socket);
    in.setVersion(QDataStream::Qt_6_2);
    if (in.status() == QDataStream::Ok) {
        while(socket->bytesAvailable() > 0) {
            if(nextBlockSize == 0) {
                if(socket->bytesAvailable() < 2) {
                    break;
                }
                in >> nextBlockSize;
            }
            if(socket->bytesAvailable() < nextBlockSize) {
                break;
            }
            QString str;
            in >> str;
            nextBlockSize = 0;

            qDebug() << "Received from server:" << str;

            // Обработка сообщений (сохранена исходная логика)
            if (str == "HISTORY_CMD:BEGIN") {
                qDebug() << "HISTORY_BEGIN received";
            }
            else if (str.startsWith("HISTORY_MSG:")) {
                QString historyData = str.mid(12);
                QStringList parts = historyData.split("|", Qt::SkipEmptyParts);

                if (parts.size() >= 3) {
                    QString timestamp = parts[0];
                    QString sender = parts[1];
                    QString message = parts.mid(2).join("|");

                    QDateTime utcTime = QDateTime::fromString(timestamp, "yyyy-MM-dd hh:mm:ss");
                    utcTime.setTimeZone(QTimeZone::UTC);
                    QString timeOnly = utcTime.toLocalTime().toString("hh:mm");

                    QString formattedMessage = QString("[%1] %2: %3")
                                                   .arg(timeOnly, sender, message);
                }
            }
            else if (str == "HISTORY_CMD:END") {
                qDebug() << "HISTORY_END received";
            }
            else if (str.startsWith("USERLIST:")) {
                QStringList users = str.mid(9).split(",");
                updateUserList(users);
                QTimer::singleShot(100, this, [this]() { requestUnreadCounts(); });
            }
            else if(str.startsWith("AUTH_SUCCESS")) {
                if (currentOperation == OperationType::Auth || currentOperation == OperationType::None) {
                    m_loginSuccesfull = true;
                    if (str.contains(':')) {
                        m_username = str.section(':', 1, 1).toStdString();
                    }
                    ui_Auth->hide();
                    this->show();
                    updateWindowTitle();
                    currentOperation = OperationType::None;
                    emit authSuccess();
                    sendMessageToServer("GET_USERLIST");
                    QTimer::singleShot(500, this, [this]() { requestUnreadCounts(); });
                }
                ui_Auth->setButtonsEnabled(true);
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
                    ui_Auth->LineClear();
                    ui_Auth->setButtonsEnabled(true);


                    // Очищаем сокет чтобы избежать дублирования сообщений
                    clearSocketBuffer();
                }
            }
            else if(str == "REGISTER_SUCCESS") {
                if (currentOperation == Register || currentOperation == None) {
                    QMessageBox::information(this, "Success", "Registration successful!");
                    ui_Reg->hide();
                    ui_Auth->show();
                    currentOperation = None;
                    emit registerSuccess();
                    ui_Reg->setButtonsEnabled(true);
                    ui_Auth->setButtonsEnabled(true);
                }
            }
            else if(str == "REGISTER_FAILED") {
                if (currentOperation == Register || currentOperation == None) {
                    QMessageBox::warning(this, "Registration Error", "Username may already exist!");

                    // Переход к окну авторизации после ошибки регистрации
                    ui_Reg->hide();
                    ui_Auth->show();
                    ui_Auth->LineClear(); // Очищаем поля ввода для нового входа

                    currentOperation = None;
                    ui_Reg->setButtonsEnabled(true);
                    ui_Auth->setButtonsEnabled(true);
                }
            }
            else if(str.startsWith("PRIVATE:")) {
                QStringList parts = str.split(":", Qt::SkipEmptyParts);
                if (parts.size() >= 3) {
                    QString sender = parts[1];
                    QString privateMessage = parts.mid(2).join(":");

                    // Пропускаем сообщения, адресованные самому себе
                    if (sender == getUsername()) {
                        return;
                    }

                    handlePrivateMessage(sender.toStdString(), privateMessage.toStdString());
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
                        if (sender == getUsername()) {
                            formattedMsg = QString("[%1] Вы: %2").arg(timeOnly, message);
                        } else {
                            formattedMsg = QString("[%1] %2: %3").arg(timeOnly, sender, message);
                        }

                        privateChatWindows[currentPrivateHistoryRecipient]->addHistoryMessage(formattedMsg.toStdString());
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

                    if (groupChatWindows.contains(chatId)) {
                        groupChatWindows[chatId]->receiveMessage(
                            sender.toStdString(),
                            message.toStdString());
                    }
                }
            }
            else if (str.startsWith("GROUP_CHAT_INFO:")) {
                QStringList parts = str.split(":", Qt::SkipEmptyParts);
                if (parts.size() >= 3) {
                    QString chatId = parts[1];
                    QString chatName = parts[2];

                    // Получаем список участников
                    QStringList qMembers;
                    if (parts.size() >= 4) {
                        qMembers = parts[3].split(",");
                    }

                    // Преобразуем QStringList -> std::vector<std::string>
                    std::vector<std::string> members;
                    members.reserve(qMembers.size()); // Оптимизация: резервируем память заранее
                    for (const QString& member : qMembers) {
                        members.push_back(member.toStdString());
                    }

                    // Обновляем список участников
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
                        groupChatWindows[chatId]->setCreator(creator.toStdString());
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
                        groupChatWindows[chatId]->setCreator(creatorName.toStdString());
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

                        groupChatWindows[chatId]->addHistoryMessage(formattedMsg.toStdString());
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
            else if (str.startsWith("UNREAD_COUNT:")) {
                QStringList parts = str.mid(QString("UNREAD_COUNT:").length()).split(":");
                if (parts.size() >= 2) {
                    // Преобразуем QString в std::string
                    std::string chatPartner = parts[0].toStdString();
                    int unreadCount = parts[1].toInt();

                    // Обновляем счетчик непрочитанных сообщений
                    unreadPrivateMessageCounts[chatPartner] = unreadCount;

                    // Обновляем список пользователей
                    sendMessageToServer("GET_USERLIST");

                    qDebug() << "Got counter of unread messages from"
                             << parts[0] << ":" << unreadCount;
                }
            }
            else if (str.startsWith("SEARCH_RESULTS:")) {
                QStringList users = str.mid(QString("SEARCH_RESULTS:").length()).split(",", Qt::SkipEmptyParts);
                qDebug() << "Получены результаты поиска:" << users;
                showSearchResults(users);
            }
            else if (str.startsWith("FRIEND_ADDED:")) {
                QString friendName = str.mid(QString("FRIEND_ADDED:").length());
                qDebug() << "Пользователь добавлен в друзья:" << friendName;
                userFriends[friendName.toStdString()] = true; // По умолчанию считаем онлайн
                sendMessageToServer("GET_USERLIST"); // Обновляем список друзей
            }
            else if (str.startsWith("FRIEND_REMOVED:")) {
                QString friendName = str.mid(QString("FRIEND_REMOVED:").length());
                userFriends.erase(friendName.toStdString());
                sendMessageToServer("GET_USERLIST"); // Обновляем список друзей
            }
            else {
                bool isDuplicate = false;

                // Игнорировать системные сообщения
                if (str.startsWith("GET_") || str == "GET_USERLIST") {
                    isDuplicate = true;
                }

                if (std::find(recentSentMessages.begin(), recentSentMessages.end(), str.toStdString()) != recentSentMessages.end()) {
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

void QtMainWindow::updateWindowTitle() {
    if (!m_username.empty()) {
        setWindowTitle("Текущий пользователь: " + QString::fromStdString(m_username));
    } else {
        setWindowTitle("Тестовый режим");
    }
}

bool QtMainWindow::isLoginSuccessful() const {
    return m_loginSuccesfull;
}

bool QtMainWindow::isConnected() const {
    return socket && socket->state() == QAbstractSocket::ConnectedState;
}

bool QtMainWindow::hasPrivateChatWith(const std::string &username) const
{
    return privateChatWindows.contains(QString::fromStdString(username));
}

int QtMainWindow::privateChatsCount() const
{
    return privateChatWindows.size();
}

std::vector<std::string> QtMainWindow::privateChatParticipants() const
{
    std::vector<std::string> participants;
    participants.reserve(privateChatWindows.size()); // Оптимизация: резервируем память

    for (const QString& key : privateChatWindows.keys()) {
        participants.push_back(key.toStdString());
    }

    return participants;
}

std::vector<std::string> QtMainWindow::getOnlineUsers() const {
    std::vector<std::string> onlineUsers;

    for (int i = 0; i < ui->userListWidget->count(); ++i) {
        QListWidgetItem* item = ui->userListWidget->item(i);
        if (!item) continue;

        bool isOnline = item->data(Qt::UserRole).toBool();
        QString type = item->data(Qt::UserRole + 1).toString();
        bool isFavorite = item->data(Qt::UserRole + 2).toBool();

        if (isOnline && type == "U" && !isFavorite) {
            onlineUsers.push_back(item->text().toStdString());
        }
    }
    return onlineUsers;
}

QStringList QtMainWindow::getDisplayedUsers() const {
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

void QtMainWindow::setPass(const std::string &pass) {
    m_userpass = pass;
}

void QtMainWindow::on_pushButton_clicked()
{
    // Создаем экземпляр окна создания группового чата
    TransitWindow *transitWindow = new TransitWindow(this);
    transitWindow->setModal(true);
    transitWindow->exec();

    // TransitWindow удаляется автоматически после закрытия
    // благодаря установленному флагу Qt::WA_DeleteOnClose
    transitWindow->setAttribute(Qt::WA_DeleteOnClose);
}

void QtMainWindow::display()
{
    ui_Auth->show();
}

// авторизация и регистрация пользователя
void QtMainWindow::authorizeUser()
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
            ui_Auth->setButtonsEnabled(true); // Включаем кнопки пользователю
            return;
        }
    }

    currentOperation = Auth;

    m_username = ui_Auth->getLogin();
    m_userpass = ui_Auth->getPass();

    ui_Auth->setButtonsEnabled(false);
    QApplication::processEvents();

    QString authString = QString("AUTH:%1:%2").arg(QString::fromStdString(m_username), QString::fromStdString(m_userpass));

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

void QtMainWindow::registerUser()
{
    // Тоже самое что для авторизации
    if(socket && socket->state() == QAbstractSocket::ConnectedState) {
        clearSocketBuffer();
    } else {
        socket->abort();
        socket->connectToHost("127.0.0.1", 5402);
        if(!socket->waitForConnected(3000)) {
            QMessageBox::critical(this, "Connection Error", "Could not connect to server");
            ui_Reg->setButtonsEnabled(true);
            return;
        }
    }
    currentOperation = Register;
    m_username = ui_Reg->getName();
    m_userpass = ui_Reg->getPass();

    ui_Reg->setButtonsEnabled(false);
    QApplication::processEvents();

    QString regString = QString("REGISTER:%1:%2").arg(QString::fromStdString(m_username), QString::fromStdString(m_userpass));

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

void QtMainWindow::prepareForRegistration()
{
    clearSocketBuffer();
    currentOperation = None;
    ui_Reg->show();
    ui_Auth->hide();
}

void QtMainWindow::clearSocketBuffer()
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

void QtMainWindow::handlePrivateMessage(const std::string &sender, const std::string &message)
{
    // Преобразуем std::string в QString
    QString qSender = QString::fromStdString(sender);

    // Добавляем отправителя в список пользователей, с которыми было общение
    recentChatPartners.insert(qSender);

    // Если отправитель не является другом, автоматически добавляем его в список друзей
    if (userFriends.find(qSender.toStdString()) == userFriends.end()) {
        qDebug() << "Автоматически добавляем отправителя" << qSender << "в список друзей";

        // Добавляем в локальный список друзей немедленно
        userFriends[qSender.toStdString()] = true;

        // Отправляем запрос на сервер для добавления пользователя в список друзей
        sendMessageToServer("ADD_FRIEND:" + sender);

        // Запрашиваем обновление списка пользователей
        sendMessageToServer("GET_USERLIST");
    }

    QTime currentTime = QTime::currentTime();
    QString timeStr = currentTime.toString("hh:mm");

    // Создаём окно чата если его ещё нет
    if (!privateChatWindows.contains(qSender)) {
        QtPrivateChatWindow* chatWindow = new QtPrivateChatWindow(qSender, this);
        connect(chatWindow, &QtPrivateChatWindow::destroyed, this, &QtMainWindow::onPrivateChatClosed);
        privateChatWindows[qSender] = chatWindow;
    }

    // Проверяем, просто видимо ли окно чата
    if (privateChatWindows[qSender]->isVisible()) {
        // Окно видимо - сразу показываем сообщение
        privateChatWindows[qSender]->receiveMessage(sender, message);

        // Отметить сообщения как прочитанные на сервере
        privateChatWindows[qSender]->markMessagesAsRead();
        return;
    }

    // Если окно не активно, увеличиваем счётчик и сохраняем сообщение
    unreadPrivateMessageCounts[qSender.toStdString()]++;

    // Сохраняем непрочитанное сообщение
    UnreadMessage unread;
    unread.sender = QString::fromStdString(sender);
    unread.message = QString::fromStdString(message);
    unread.timestamp = QString::fromStdString(timeStr.toStdString());
    unreadMessages[qSender].append(unread);

    // Обновляем список пользователей
    sendMessageToServer("GET_USERLIST");
}

// Замена метода processJsonMessage на обработку обычных сообщений
void QtMainWindow::sendPrivateMessage(const std::string &recipient, const std::string &message)
{
    if (recipient == getUsername()) {
        return;
    }

    // Добавляем получателя в список пользователей, с которыми было общение
    recentChatPartners.insert(QString::fromStdString(recipient));

    qDebug() << "MainWindow: Подготовка приватного сообщения для" << recipient << ":" << message;

    // Формируем строку в формате "PRIVATE:получатель:сообщение"
    QString privateMessage = QString("PRIVATE:%1:%2").arg(QString::fromStdString(recipient), QString::fromStdString(message));

    qDebug() << "MainWindow: Сообщение для отправки:" << privateMessage;

    sendMessageToServer(privateMessage.toStdString());
}

// Метод для поиска или создания окна приватного чата
QtPrivateChatWindow* QtMainWindow::findOrCreatePrivateChatWindow(const QString &username)
{
    if (privateChatWindows.contains(username)) {
        privateChatWindows[username]->show();
        privateChatWindows[username]->activateWindow();
        return privateChatWindows[username];
    } else {
        // Исправляем создание окна с правильными параметрами
        QtPrivateChatWindow *chatWindow = new QtPrivateChatWindow(username, this, nullptr);

        connect(chatWindow, &QtPrivateChatWindow::historyDisplayCompleted, this, [this, username]() {
            if (unreadMessages.contains(username)) {
                unreadMessages[username].clear();
            }
        });

        privateChatWindows[username] = chatWindow;
        chatWindow->show();
        return chatWindow;
    }
}

void QtMainWindow::requestPrivateMessageHistory(const std::string &otherUser)
{
    QString request = QString("GET_PRIVATE_HISTORY:%1").arg(QString::fromStdString(otherUser));
    sendMessageToServer(request.toStdString());
}

void QtMainWindow::handleAuthenticationTimeout()
{
    qDebug() << "Authentication/Registration timed out";


    if (currentOperation == Auth) {
        QMessageBox::warning(this, "Authentication Error",
                             "Server did not respond in time. Please try again.");
        ui_Auth->setButtonsEnabled(true);
    }
    else if (currentOperation == Register) {
        QMessageBox::warning(this, "Registration Error",
                             "Server did not respond in time. Please try again.");
        ui_Reg->setButtonsEnabled(true);
    }
    currentOperation = None;
}

// Ошибки сокета
void QtMainWindow::handleSocketError(QAbstractSocket::SocketError socketError)
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
        ui_Auth->setButtonsEnabled(true);
    }
    else if (currentOperation == Register) {
        QMessageBox::critical(this, "Connection Error",
                              "Error during registration: " + errorMessage + "\nPlease try again.");
        ui_Reg->setButtonsEnabled(true);
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
void QtMainWindow::createGroupChat(const std::string &chatName, const std::string &chatId)
{
    // Отправляем запрос на сервер о создании группового чата
    // Меняем порядок параметров: сначала имя чата, потом ID
    QString createChatMessage = QString("CREATE_GROUP_CHAT:%1:%2").arg(QString::fromStdString(chatName), QString::fromStdString(chatId));
    sendMessageToServer(createChatMessage.toStdString());

    QMessageBox::information(this, "Создание чата",
                             "Запрос на создание группового чата \"" + QString::fromStdString(chatName) + "\" отправлен на сервер.");
}

// Добавим метод для присоединения к групповому чату
void QtMainWindow::joinGroupChat(const std::string &chatId)
{
    QString joinRequest = QString("JOIN_GROUP_CHAT:%1").arg(QString::fromStdString(chatId));
    sendMessageToServer(joinRequest.toStdString());
}

// Добавляем метод для установки режима добавления пользователя в групповой чат
void QtMainWindow::startAddUserToGroupMode(const std::string &chatId)
{
    pendingGroupChatId = chatId;
    qDebug() << "Активирован режим добавления пользователя для группового чата с ID:" << chatId;

    // Обновляем список пользователей, чтобы быть уверенными, что он актуален
    sendMessageToServer("GET_USERLIST");

    // Активируем главное окно, чтобы пользователь мог выбрать участника
    this->activateWindow();
    this->raise();
}

void QtMainWindow::on_pushButton_2_clicked()
{
    // Пустая реализация, но метод необходим для Qt auto-connection
}

void QtMainWindow::on_lineEdit_returnPressed()
{
    searchUsers();
}

void QtMainWindow::searchUsers()
{
    QString searchQuery = ui->lineEdit->text().trimmed();
    if (searchQuery.isEmpty()) {
        qDebug() << "Поисковый запрос пустой, поиск не выполняется";
        return;
    }

    // Отправляем запрос на поиск пользователей
    QString requestMessage = "SEARCH_USERS:" + searchQuery;
    sendMessageToServer(requestMessage.toStdString());
    qDebug() << "Отправлен запрос на поиск пользователей:" << requestMessage;

    // Сбрасываем поле поиска
    ui->lineEdit->clear();
}


void QtMainWindow::showSearchResults(const QStringList &users)
{
    qDebug() << "Отображение результатов поиска, найдено пользователей:" << users.size();

    // Если сервер вернул пустой список
    if (users.isEmpty()) {
        QMessageBox::information(this, "Поиск пользователей", "Пользователи не найдены");
        return;
    }

    // Создаем список пользователей, которых нет в друзьях
    QStringList filteredUsers;
    for (const QString &user : users) {
        if (user != getUsername() && userFriends.find(user.toStdString()) == userFriends.end()) {
            filteredUsers.append(user);
        }
    }

    // Если после фильтрации список пуст
    if (filteredUsers.isEmpty()) {
        QMessageBox::information(this, "Поиск пользователей",
                                 "Все найденные пользователи уже в вашем списке друзей");
        return;
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

    QLabel *helpLabel = new QLabel("Выберите пользователя и нажмите Enter, чтобы добавить его в друзья", searchDialog);
    helpLabel->setWordWrap(true);
    layout->addWidget(helpLabel);

    QPushButton *closeButton = new QPushButton("Закрыть", searchDialog);
    layout->addWidget(closeButton);

    connect(closeButton, &QPushButton::clicked, searchDialog, &QDialog::close);
    connect(searchListWidget, &QListWidget::itemDoubleClicked, this, &QtMainWindow::onSearchItemSelected);

    // Добавляем обработчик нажатия Enter
    QShortcut *shortcut = new QShortcut(QKeySequence(Qt::Key_Return), searchListWidget);
    connect(shortcut, &QShortcut::activated, [this]() {
        if (searchListWidget && searchListWidget->currentItem()) {
            onSearchItemSelected(searchListWidget->currentItem());
        }
    });

    // Очищаем и заполняем список результатов
    searchListWidget->clear();
    for (const QString &user : filteredUsers) {
        searchListWidget->addItem(new QListWidgetItem(user));
    }

    // Подключаем сигнал finished для безопасного удаления диалога
    connect(searchDialog, &QDialog::finished, [this]() {
        searchDialog = nullptr;
        searchListWidget = nullptr;
    });

    searchDialog->show();
    searchDialog->raise();
    searchDialog->activateWindow();
}


void QtMainWindow::onSearchItemSelected(QListWidgetItem *item)
{
    if (!item || !searchListWidget || !searchDialog)
        return;

    QString username = item->text();
    qDebug() << "Выбран пользователь для добавления в друзья:" << username;

    // Добавляем пользователя в друзья
    addUserToFriends(username);

    // Безопасно закрываем диалог
    searchDialog->accept();
}

void QtMainWindow::addUserToFriends(const QString &username)
{
    // Отправляем запрос на добавление пользователя в друзья
    sendMessageToServer("ADD_FRIEND:" + username.toStdString());
    qDebug() << "Отправлен запрос на добавление в друзья:" << username;

    // Немедленно добавляем пользователя в локальный список друзей
    // (не дожидаясь ответа от сервера)
    userFriends[username.toStdString()] = true;

    // Показываем всплывающее сообщение
    QMessageBox::information(this, "Добавление друга",
                             "Пользователь " + username + " добавлен в список друзей");

    // Запрашиваем обновление списка пользователей
    sendMessageToServer("GET_USERLIST");
}


void QtMainWindow::requestUnreadCounts()
{
    // Отправляем запросы на получение непрочитанных сообщений для всех пользователей
    for (int i = 0; i < ui->userListWidget->count(); ++i) {
        QListWidgetItem* item = ui->userListWidget->item(i);
        if (item && item->data(Qt::UserRole + 1).toString() == "U") {
            QString username = item->text();

            if (username.contains(" (")) {
                username = username.left(username.indexOf(" ("));
            }

            if (item->data(Qt::UserRole + 2).toBool()) {
                continue;
            }

            if (item->data(Qt::UserRole + 4).toString().length() > 0) {
                username = item->data(Qt::UserRole + 4).toString();
            }

            sendMessageToServer(QString("GET_UNREAD_COUNT:%1").arg(username).toStdString());
        }
    }
}


void QtMainWindow::onPrivateChatClosed()
{
    // Находим указатель на отправителя сигнала
    QObject* senderObj = sender();
    if (senderObj) {
        QtPrivateChatWindow* chatWindow = qobject_cast<QtPrivateChatWindow*>(senderObj);
        if (chatWindow) {
            // Получаем имя пользователя, с которым был чат
            QString username = chatWindow->property("username").toString();
            if (!username.isEmpty() && privateChatWindows.contains(username)) {
                privateChatWindows.remove(username);

                // Запрашиваем обновленный список пользователей, чтобы получить актуальные статусы
                QTimer::singleShot(100, this, [this]() {
                    sendMessageToServer("GET_USERLIST");
                });
            }
        }
    }
}
