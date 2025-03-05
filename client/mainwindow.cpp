#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QtDebug>
#include <QMessageBox>
#include <QtSql>
#include<QScreen>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    //делает окно по центру
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->geometry();
        int x = (screenGeometry.width() - this->width()) / 2;
        int y = (screenGeometry.height() - this->height()) / 2;
        this->move(x, y);
    }

    ui->setupUi(this);
    socket = new QTcpSocket(this);
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::slotReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &MainWindow::deleteLater);
    nextBlockSize = 0;
    
    // Создание авторизации и регистрации пользователя , сигналы с кнопок
    user_counter = 0;
    m_loginSuccesfull = false;
    connect(&ui_Auth, SIGNAL(login_button_clicked()),
            this, SLOT(authorizeUser()));
    connect(&ui_Auth, SIGNAL(destroyed()),
            this, SLOT(show()));
    connect(&ui_Auth, SIGNAL(register_button_clicked()),
            this, SLOT(registerWindowShow()));
    connect(&ui_Reg, SIGNAL(register_button_clicked2()),
            this, SLOT(registerUser()));
    connect(&ui_Reg, SIGNAL(destroyed()),
            &ui_Auth, SLOT(show()));
    if(!connectDB())
    {
        qDebug() << "Failed to connect DB";
    }
            
    // прячем главное окно
    this->hide();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushButton_clicked()
{
    socket->connectToHost("127.0.0.1", 5402);
}

void MainWindow::SendToServer(QString str)
{
    Data.clear();
    //создаем данные на вывод, передаем массив байтов(&Data) и режим работы(QIODevice::WriteOnly) только для записи
    QDataStream out(&Data, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_2);
    out << quint16(0) << str;
    out.device()->seek(0);
    out << quint16(Data.size() - sizeof(quint16));
    socket->write(Data);
    ui->lineEdit->clear();
}

void MainWindow::slotReadyRead()
{
    QDataStream in(socket);
    in.setVersion(QDataStream::Qt_6_2);
    if (in.status() == QDataStream::Ok){
        /*QString str;
        in >> str;
        ui->textBrowser->append(str);*/ //позже доделать
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
            ui->textBrowser->append(str);
        }
    }
    else{
        ui->textBrowser->append("read error");
    }
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
    m_username = ui_Auth.getLogin();
    m_userpass = ui_Auth.getPass();

    QString str_t = " SELECT * "
                    " FROM userlist "
                    " WHERE name = '%1'";

    QString username = "";
    QString userpass = "";

    int xPos = 0;
    int yPos = 0;
    int width = 0;
    int length = 0;
    db_input = str_t.arg(m_username);

    QSqlQuery query;
    QSqlRecord rec;

    if(!query.exec(db_input))
    {
        qDebug() << "Unable to create a table" << query.lastError() << " : " << query.lastQuery();
    }
    rec = query.record();
    query.next();
    user_counter = query.value(rec.indexOf("number")).toInt();
    username = query.value(rec.indexOf("name")).toString();
    userpass = query.value(rec.indexOf("pass")).toString();
    
    if(m_username != username || m_userpass != userpass)
    {
        qDebug() << "Password missmatch" << username << " " << userpass;
        m_loginSuccesfull = false;
    }
    else
    {
        m_loginSuccesfull = true;
        xPos = query.value(rec.indexOf("xpos")).toInt();
        yPos = query.value(rec.indexOf("ypos")).toInt();
        width = query.value(rec.indexOf("width")).toInt();
        length = query.value(rec.indexOf("length")).toInt();
        ui_Auth.close();
        ui_Reg.close();
        this->setGeometry(xPos, yPos, width, length);
        this->show();
        
        // после успешной авторизации - автоматическое подключение к серверу
        socket->connectToHost("127.0.0.1", 5402);
    }
}

bool MainWindow::connectDB()
{
    mw_db = QSqlDatabase::addDatabase("QSQLITE");
    QString dbPath = QCoreApplication::applicationDirPath() + "/data";
    QDir().mkpath(dbPath);
    mw_db.setDatabaseName(dbPath + "/authorisation.db");
    if(!mw_db.open())
    {
        qDebug() << "Cannot open database: " << mw_db.lastError();
        return false;
    }
    
    // Создание БД, если не существует, пока не понятно как синхронизировать данные с сервером
    QSqlQuery query;
    db_input = "CREATE TABLE IF NOT EXISTS userlist ( "
               "number INTEGER PRIMARY KEY NOT NULL,"
               "name VARCHAR(20), "
               "pass VARCHAR(12), "
               "xpos INTEGER, "
               "ypos INTEGER, "
               "width INTEGER, "
               "length INTEGER );";
    if(!query.exec(db_input))
    {
        qDebug() << "Unable to create table" << query.lastError();
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
    // Получение данных с окна
    m_username = ui_Reg.getName();
    m_userpass = ui_Reg.getPass();
    
    // повторная проверка паролей
    if(!ui_Reg.checkPass()) {
        QMessageBox::warning(this, "Registration Error", "Passwords don't match!");
        return;
    }
    
    // Очередь для создания пользователя
    QString str_t = "INSERT INTO userlist (name, pass) VALUES ('%1', '%2')";
    db_input = str_t.arg(m_username).arg(m_userpass);
    
    QSqlQuery query;
    if(!query.exec(db_input)) {
        qDebug() << "Unable to add user to table" << query.lastError() << " : " << query.lastQuery();
        QMessageBox::warning(this, "Registration Error", "Database error occurred");
    } else {
        QMessageBox::information(this, "Success", "Registration successful!");
        ui_Reg.hide();
        ui_Auth.show();
    }
}


