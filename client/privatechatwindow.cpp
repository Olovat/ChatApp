#include "privatechatwindow.h"
#include "ui_privatechatwindow.h"
#include "mainwindow.h"
#include <QDebug>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTime>
#include <QShortcut>

PrivateChatWindow::PrivateChatWindow(const QString &username, MainWindow *mainWindow, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PrivateChatWindow), // Используем правильный UI класс
    username(username),
    mainWindow(mainWindow)
{
    ui->setupUi(this); // Стандартная настройка UI

    setWindowTitle("Чат с " + username);

    // Используем новый синтаксис для соединения сигналов и слотов
    connect(ui->sendButton, &QPushButton::clicked, this, &PrivateChatWindow::on_sendButton_clicked);

    // Настройка горячих клавиш для отправки сообщений
    QShortcut *shortcut = new QShortcut(QKeySequence(Qt::Key_Return), ui->messageEdit);
    connect(shortcut, &QShortcut::activated, this, &PrivateChatWindow::on_sendButton_clicked);

    if (mainWindow) {
        // Запрашиваем историю сообщений у сервера
        mainWindow->requestPrivateMessageHistory(username);
    }
}

PrivateChatWindow::~PrivateChatWindow()
{
    delete ui;
}

void PrivateChatWindow::on_sendButton_clicked()
{
    QString message = ui->messageEdit->text().trimmed();
    if (!message.isEmpty()) {
        sendMessage(message);
        ui->messageEdit->clear();
    }
}

void PrivateChatWindow::sendMessage(const QString &message)
{
    if (mainWindow) {
        qDebug() << "PrivateChatWindow: Отправка сообщения к" << username << ":" << message;
        
        // Отправляем сообщение через основное окно
        mainWindow->sendPrivateMessage(username, message);

        // Добавляем сообщение в свое окно чата
        QTime currentTime = QTime::currentTime();
        QString timeStr = currentTime.toString("hh:mm");
        ui->chatBrowser->append("[" + timeStr + "] Вы: " + message);
        
        qDebug() << "PrivateChatWindow: Сообщение отображено локально";
    } else {
        qDebug() << "PrivateChatWindow: ОШИБКА - mainWindow равен nullptr!";
    }
}

void PrivateChatWindow::receiveMessage(const QString &sender, const QString &message)
{
    qDebug() << "PrivateChatWindow: Получено сообщение от" << sender << ":" << message;
    
    QTime currentTime = QTime::currentTime();
    QString timeStr = currentTime.toString("hh:mm");
    ui->chatBrowser->append("[" + timeStr + "] " + sender + ": " + message);
}

void PrivateChatWindow::beginHistoryDisplay()
{
    //ui->chatBrowser->append("---------- ИСТОРИЯ ЛИЧНЫХ СООБЩЕНИЙ ----------");
    historyDisplayed = true;
}

void PrivateChatWindow::addHistoryMessage(const QString &formattedMessage)
{
    ui->chatBrowser->append(formattedMessage);
}

void PrivateChatWindow::endHistoryDisplay()
{
    //ui->chatBrowser->append("---------- КОНЕЦ ИСТОРИИ ЛИЧНЫХ СООБЩЕНИЙ ----------");
    ui->chatBrowser->append(""); // просто отделение истории от новых сообщений
}
