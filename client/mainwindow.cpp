#include <QApplication>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QtDebug>
#include <QMessageBox>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    
    ui->setupUi(this);
    socket = new QTcpSocket(this);
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::slotReadyRead);
    connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    nextBlockSize = 0;
    currentOperation = None; // Начальное состояние, потом будет изменено на Auth или Register, в завивимости от процесса.
    
    // Подключение к серверу
    socket->connectToHost("127.0.0.1", 5402);
    if (!socket->waitForConnected(5000)) {
        qDebug() << "Failed to connect to server:" << socket->errorString();
    }
    
    // Создание авторизации и регистрации пользователя, сигналы с кнопок
    m_loginSuccesfull = false;
    
    // Инициализация окон авторизации и регистрации
    connect(&ui_Auth, &auth_window::login_button_clicked, this, &MainWindow::authorizeUser);
    connect(&ui_Auth, &auth_window::register_button_clicked, this, &MainWindow::prepareForRegistration);
    
    // Инициализация окна регистрации
    connect(&ui_Reg, &reg_window::register_button_clicked2, this, &MainWindow::registerUser);            
    // прячем главное окно
    this->hide();
    
    // Инициализация переменной для отслеживания последнего ответа авторизации
    lastAuthResponse = "";
}

MainWindow::~MainWindow()
{
    delete ui;
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
    
    //Отправка данных на сервер
    if (socket && socket->isValid()) {
        socket->write(Data);
    }
    ui->lineEdit->clear();
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
            
            // Проверяем, не является ли строка ответом авторизации/регистрации
            bool isAuthResponse = (str == "AUTH_SUCCESS" || str == "AUTH_FAILED" || 
                                  str == "REGISTER_SUCCESS" || str == "REGISTER_FAILED");
            
            // Если это ответ авторизации/регистрации и мы уже обрабатывали такой же ответ, пропускаем его
            if (isAuthResponse && str == lastAuthResponse) {
                continue;
            }
            
            // Сохраняем текущий ответ авторизации/регистрации
            if (isAuthResponse) {
                lastAuthResponse = str;
            }
            
            // Обработка ответа сервера в зависимости от текущей операции
            if(str == "AUTH_SUCCESS") {
                if (currentOperation == Auth || currentOperation == None) {
                    m_loginSuccesfull = true;
                    ui_Auth.close();
                    this->show();
                    currentOperation = None;
                }
                // Включаем кнопки после обработки ответа
                ui_Auth.setButtonsEnabled(true);
            }
            else if(str == "AUTH_FAILED") {
                if (currentOperation == Auth || currentOperation == None) {
                    QMessageBox::warning(this, "Authentication Error", "Invalid username or password!");
                    m_loginSuccesfull = false;
                    ui_Auth.LineClear();
                    currentOperation = None;
                    // Включаем кнопки после обработки ответа
                    ui_Auth.setButtonsEnabled(true);
                }
            }
            else if(str == "REGISTER_SUCCESS") {
                if (currentOperation == Register || currentOperation == None) {
                    QMessageBox::information(this, "Success", "Registration successful!");
                    ui_Reg.hide();
                    ui_Auth.show();
                    currentOperation = None;
                    // Включаем кнопки после обработки ответа
                    ui_Reg.setButtonsEnabled(true);
                    ui_Auth.setButtonsEnabled(true);
                }
            }
            else if(str == "REGISTER_FAILED") {
                if (currentOperation == Register || currentOperation == None) {
                    QMessageBox::warning(this, "Registration Error", "Username may already exist!");
                    currentOperation = None;
                    // Включаем кнопки после обработки ответа
                    ui_Reg.setButtonsEnabled(true);
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
    // Очистка любых ожидающих данных и установка состояния операции
    clearSocketBuffer();
    currentOperation = Auth;
    
    m_username = ui_Auth.getLogin();
    m_userpass = ui_Auth.getPass();
    m_userpass = ui_Auth.getPass();
    
    // Делаем кнопки неактивными, чтобы пользователь не спамил запросами в БД!
    ui_Auth.setButtonsEnabled(false);
    QApplication::processEvents(); // Форсировать немедленное обновление UI
    
    // Отправка запроса на авторизацию на сервер
    QString authString = QString("AUTH:%1:%2").arg(m_username, m_userpass);
    
    // Отправка данных на сервер
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
    // Подключение к серверу
    socket->connectToHost("127.0.0.1", 5402);
    if (!socket->waitForConnected(3000)) {
        qDebug() << "Failed to connect to server:" << socket->errorString();
        return false;
    }
    return true;
}

void MainWindow::registerWindowShow()
{
    // Прячем окно авторизации, показ окна регистрации
    ui_Auth.hide();
    ui_Reg.show();
}

void MainWindow::registerUser()
{    
    // Очистка любых ожидающих данных и установка состояния операции
    clearSocketBuffer();
    currentOperation = Register;
    
    if(!ui_Reg.checkPass()) {
        QMessageBox::warning(this, "Registration Error", "Passwords don't match!");
        ui_Reg.ConfirmClear();
        return;
    }

    m_username = ui_Reg.getName();
    m_userpass = ui_Reg.getPass();
    
    // Отключаем кнопки, чтобы пользователь не спамил запросами в БД!
    ui_Reg.setButtonsEnabled(false);
    QApplication::processEvents(); //Форсировать немедленное обновление UI
    
    // Отправка запроса на регистрацию на сервер
    QString regString = QString("REGISTER:%1:%2").arg(m_username, m_userpass);
    
    // Отправка данных на сервер
    Data.clear();
    QDataStream out(&Data, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_2);
    out << quint16(0) << regString;
    out.device()->seek(0);
    out << quint16(Data.size() - sizeof(quint16));
    socket->write(Data);
    socket->waitForBytesWritten();
}

// Что нужно сделать при подготовке к регистрации
void MainWindow::prepareForRegistration()
{
    clearSocketBuffer();
    currentOperation = None;
    ui_Reg.show();
    ui_Auth.hide();
}

// Очистка буфера сокета
void MainWindow::clearSocketBuffer()
{
    // Очищаем отслеживание последнего ответа авторизации при новом запросе
    lastAuthResponse = "";
    
    if (socket && socket->bytesAvailable() > 0) {
        socket->readAll();
    }
    nextBlockSize = 0;
}
