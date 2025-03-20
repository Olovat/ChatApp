#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QApplication>
#include <QMessageBox>
#include <QListWidgetItem>
#include <QDataStream>
#include <QDebug>
#include "privatechatwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    socket = new QTcpSocket(this);
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::slotReadyRead);
    connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    nextBlockSize = 0;
    currentOperation = None;

    m_loginSuccesfull = false;
    connect(&ui_Auth, &auth_window::login_button_clicked, this, &MainWindow::authorizeUser);
    connect(&ui_Auth, &auth_window::register_button_clicked, this, &MainWindow::prepareForRegistration);
    connect(&ui_Reg, &reg_window::register_button_clicked2, this, &MainWindow::registerUser);
    this->hide();
    lastAuthResponse = "";

    // Подключение сигнала для обработки выбора пользователя из списка
    connect(ui->userListWidget, &QListWidget::itemClicked, this, &MainWindow::onUserSelected);

    socket->connectToHost("127.0.0.1", 5402);
    if (!socket->waitForConnected(5000)) {
        qDebug() << "Failed to connect to server:" << socket->errorString();
    } else {
        qDebug() << "Connected to server.";
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

// Функция для обновления списка пользователей
void MainWindow::updateUserList(const QStringList &users)
{
    qDebug() << "Updating user list with users:" << users;
    ui->userListWidget->clear();
    for (const QString &user : users) {
        QListWidgetItem *item = new QListWidgetItem(user);
        ui->userListWidget->addItem(item);
    }
}

// Обработчик для выбора пользователя из списка
void MainWindow::onUserSelected(QListWidgetItem *item)
{
    QString selectedUser = item->text();
    qDebug() << "User selected:" << selectedUser;

    // Если окно чата с этим пользователем уже открыто
    if (privateChatWindows.contains(selectedUser)) {
        privateChatWindows[selectedUser]->show();
        privateChatWindows[selectedUser]->activateWindow();
    } else {
        // Создаем новое окно чата
        PrivateChatWindow *chatWindow = new PrivateChatWindow(selectedUser, this);
        privateChatWindows[selectedUser] = chatWindow;
        chatWindow->show();
    }
}

void MainWindow::SendToServer(QString str)
{
    QString messageId = QUuid::createUuid().toString();
    QString formattedMessage = m_username + ": " + str;
    ui->textBrowser->append(formattedMessage);

    recentSentMessages.append(formattedMessage);
    while (recentSentMessages.size() > 20) {
        recentSentMessages.removeFirst();
    }

    Data.clear();
    QDataStream out(&Data, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_2);
    out << quint16(0) << str;
    out.device()->seek(0);
    out << quint16(Data.size() - sizeof(quint16));

    if (socket && socket->isValid()) {
        socket->write(Data);
    }
    ui->lineEdit->clear();
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
    QDataStream in(socket);
    in.setVersion(QDataStream::Qt_6_2);
    if (in.status() == QDataStream::Ok){
        for(;;){
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

            // Удаляем обработку JSON сообщений
            if (str.startsWith("USERLIST:")) {
                QStringList users = str.mid(QString("USERLIST:").length()).split(",");
                qDebug() << "User list received:" << users;
                updateUserList(users);
            }
            else if(str == "AUTH_SUCCESS") {
                if (currentOperation == Auth || currentOperation == None) {
                    m_loginSuccesfull = true;
                    ui_Auth.close();
                    this->show();
                    currentOperation = None;

                    SendToServer("GET_USERLIST");
                }
                ui_Auth.setButtonsEnabled(true);
            }
            else if(str == "AUTH_FAILED") {
                if (currentOperation == Auth || currentOperation == None) {
                    QMessageBox::warning(this, "Authentication Error", "Invalid username or password!");
                    m_loginSuccesfull = false;
                    ui_Auth.LineClear();
                    currentOperation = None;
                    ui_Auth.setButtonsEnabled(true);
                }
            }
            else if(str == "REGISTER_SUCCESS") {
                if (currentOperation == Register || currentOperation == None) {
                    QMessageBox::information(this, "Success", "Registration successful!");
                    ui_Reg.hide();
                    ui_Auth.show();
                    currentOperation = None;
                    ui_Reg.setButtonsEnabled(true);
                    ui_Auth.setButtonsEnabled(true);
                }
            }
            else if(str == "REGISTER_FAILED") {
                if (currentOperation == Register || currentOperation == None) {
                    QMessageBox::warning(this, "Registration Error", "Username may already exist!");
                    currentOperation = None;
                    ui_Reg.setButtonsEnabled(true);
                }
            }
            else if(str.startsWith("PRIVATE:")) {
                QStringList parts = str.split(":", Qt::SkipEmptyParts);
                if (parts.size() >= 3) {
                    QString sender = parts[1];
                    QString privateMessage = parts.mid(2).join(":");
                    handlePrivateMessage(sender, privateMessage);
                }
            }
            else {
                if (!lastReceivedMessage.isEmpty() && str == lastReceivedMessage) {
                    continue;
                }

                lastReceivedMessage = str;

                if (recentSentMessages.contains(str)) {
                    continue;
                }

                bool isDuplicate = false;

                for (const QString &recentMsg : recentSentMessages) {
                    if (str.contains(recentMsg) || recentMsg == str) {
                        isDuplicate = true;
                        break;
                    }
                }

                if (!isDuplicate) {
                    ui->textBrowser->append(str);
                }
            }
        }
    }
    else{
        ui->textBrowser->append("read error");
    }
}

void MainWindow::setLogin(const QString &login) {
    m_username = login;
}

void MainWindow::setPass(const QString &pass) {
    m_userpass = pass;
}

QString MainWindow::getUsername() const {
    return m_username;
}

QString MainWindow::getUserpass() const {
    return m_userpass;
}

void MainWindow::on_pushButton_2_clicked()
{
    QString text = ui->lineEdit->text();
    SendToServer(text);
}

void MainWindow::on_lineEdit_returnPressed()
{
    SendToServer(ui->lineEdit->text());
}

void MainWindow::display()
{
    ui_Auth.show();
}

// авторизация и регистрация пользователя
void MainWindow::authorizeUser()
{
    clearSocketBuffer();
    currentOperation = Auth;

    m_username = ui_Auth.getLogin();
    m_userpass = ui_Auth.getPass();
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
    socket->waitForBytesWritten();
}

bool MainWindow::connectToServer()
{
    socket->connectToHost("127.0.0.1", 5402);
    if (!socket->waitForConnected(3000)) {
        qDebug() << "Failed to connect to server:" << socket->errorString();
        return false;
    }
    return true;
}

void MainWindow::registerWindowShow()
{
    ui_Auth.hide();
    ui_Reg.show();
}

void MainWindow::registerUser()
{
    clearSocketBuffer();
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
    socket->waitForBytesWritten();
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
    lastAuthResponse = "";

    if (socket && socket->bytesAvailable() > 0) {
        socket->readAll();
    }
    nextBlockSize = 0;
}

void MainWindow::handlePrivateMessage(const QString &sender, const QString &message)
{
    // Если окно чата с этим пользователем уже открыто
    if (privateChatWindows.contains(sender)) {
        privateChatWindows[sender]->receiveMessage(sender, message);
    } else {
        // Окно чата не открыто - создаем новое
        PrivateChatWindow *chatWindow = new PrivateChatWindow(sender, this);
        privateChatWindows[sender] = chatWindow;
        chatWindow->show();
        chatWindow->receiveMessage(sender, message);
    }
}

// Замена метода processJsonMessage на обработку обычных сообщений
void MainWindow::sendPrivateMessage(const QString &recipient, const QString &message)
{
    qDebug() << "MainWindow: Подготовка приватного сообщения для" << recipient << ":" << message;
    
    // Формируем строку в формате "PRIVATE:recipient:message"
    QString privateMessage = QString("PRIVATE:%1:%2").arg(recipient, message);
    
    qDebug() << "MainWindow: Сообщение для отправки:" << privateMessage;
    
    sendMessageToServer(privateMessage);
}

// Метод для получения имени текущего пользователя
QString MainWindow::getCurrentUsername() const
{
    return m_username;
}

// Метод для поиска или создания окна приватного чата
PrivateChatWindow* MainWindow::findOrCreatePrivateChatWindow(const QString &username)
{
    if (privateChatWindows.contains(username)) {
        privateChatWindows[username]->show();
        privateChatWindows[username]->activateWindow();
        return privateChatWindows[username];
    } else {
        PrivateChatWindow *chatWindow = new PrivateChatWindow(username, this);
        privateChatWindows[username] = chatWindow;
        chatWindow->show();
        return chatWindow;
    }
}

// Обновленный метод обработки приватных сообщений
void MainWindow::handlePrivateMessage(const QString &sender, const QString &recipient, const QString &message)
{
    qDebug() << "MainWindow: Обработка приватного сообщения от" << sender << "к" << recipient << ":" << message;
    qDebug() << "MainWindow: Текущий пользователь:" << getCurrentUsername();
    
    // Проверяем, кто мы в этом сообщении - отправитель или получатель
    if (sender == getCurrentUsername()) {
        qDebug() << "MainWindow: Мы отправитель сообщения";
        // Мы отправитель - находим или создаем окно чата с получателем
        PrivateChatWindow* chatWindow = findOrCreatePrivateChatWindow(recipient);
        // Не добавляем сообщение, т.к. оно уже было добавлено при отправке
    } 
    else if (recipient == getCurrentUsername()) {
        qDebug() << "MainWindow: Мы получатель сообщения";
        // Мы получатель - находим или создаем окно чата с отправителем
        PrivateChatWindow* chatWindow = findOrCreatePrivateChatWindow(sender);
        chatWindow->receiveMessage(sender, message);
    } else {
        qDebug() << "MainWindow: ОШИБКА - Сообщение не предназначено для текущего пользователя!";
    }
}