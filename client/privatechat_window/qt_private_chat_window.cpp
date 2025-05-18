#include "qt_private_chat_window.h"
#include "ui_privatechatwindow.h"
#include "./mainwindow/qt_main_window.h"
#include <QDateTime>
#include <QShortcut>
#include <QDebug>
#include <QTimeZone>

QtPrivateChatWindow::QtPrivateChatWindow(const QString &username, QtMainWindow *mainWindow, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PrivateChatWindow),
    mainWindow(mainWindow)
{
    ui->setupUi(this);
    this->username = username.toStdString();
    updateWindowTitle();

    connect(ui->sendButton, &QPushButton::clicked, this, &QtPrivateChatWindow::on_sendButton_clicked);

    QShortcut *shortcut = new QShortcut(QKeySequence(Qt::Key_Return), ui->messageEdit);
    connect(shortcut, &QShortcut::activated, this, &QtPrivateChatWindow::on_sendButton_clicked);

    if (mainWindow) {
        mainWindow->requestPrivateMessageHistory(username.toStdString());
    }
}

QtPrivateChatWindow::~QtPrivateChatWindow()
{
    delete ui;
}

void QtPrivateChatWindow::setOfflineStatus(bool offline)
{
    if (isOffline != offline) {
        previousOfflineStatus = isOffline;
        isOffline = offline;
        updateWindowTitle();
        statusMessagePending = true;

        if (historyDisplayed) {
            addStatusMessage();
        }
    }
}

void QtPrivateChatWindow::addStatusMessage()
{
    if (!statusMessagePending) return;

    QString statusText;
    QString timeStr = QTime::currentTime().toString("hh:mm");

    if (isOffline) {
        statusText = "<i>[" + timeStr + "] Пользователь " + QString::fromStdString(username) +
                     " вышел из сети. Сообщения ниже будут доставлены, когда пользователь вернется.</i>";
    } else if (!previousOfflineStatus) {
        statusText = "<i>[" + timeStr + "] Пользователь " + QString::fromStdString(username) + " в сети.</i>";
    } else {
        statusText = "<i>[" + timeStr + "] Пользователь " + QString::fromStdString(username) + " вернулся в сеть.</i>";
    }

    ui->chatBrowser->append(statusText);
    statusMessagePending = false;
}

void QtPrivateChatWindow::updateWindowTitle()
{
    QString status = isOffline ? " (Не в сети)" : " (В сети)";
    setWindowTitle("Чат с " + QString::fromStdString(username) + status);
}

void QtPrivateChatWindow::on_sendButton_clicked()
{
    QString message = ui->messageEdit->text().trimmed();
    if (!message.isEmpty()) {
        sendMessage(message);
        ui->messageEdit->clear();

        QTime currentTime = QTime::currentTime();
        ui->chatBrowser->append("[" + currentTime.toString("hh:mm") + "] Вы: " + message);
    }
}

void QtPrivateChatWindow::sendMessage(const QString &message)
{
    if (mainWindow) {
        qDebug() << "PrivateChatWindow: Отправка сообщения к" << username << ":" << message;

        // Отправляем сообщение через основное окно
        mainWindow->sendPrivateMessage(username, message.toStdString());

        // Добавляем сообщение в свое окно чата
        QTime currentTime = QTime::currentTime();
        QString timeStr = currentTime.toString("hh:mm");
        ui->chatBrowser->append("[" + timeStr + "] Вы: " + message);

        qDebug() << "PrivateChatWindow: Сообщение отображено локально";
    } else {
        qDebug() << "PrivateChatWindow: ОШИБКА - mainWindow равен nullptr!";
    }
}

void QtPrivateChatWindow::receiveMessage(const std::string& sender, const std::string& message)
{
    receiveMessage(sender, message, QDateTime::currentDateTime().toString(Qt::ISODate).toStdString());
}

void QtPrivateChatWindow::receiveMessage(const std::string& sender, const std::string& message,
                                         const std::string& timestamp)
{
    QString localTime = convertUtcToLocalTime(QString::fromStdString(timestamp));
    QString qMessage = QString::fromStdString(message);

    if (qMessage.startsWith("[") && qMessage.contains("]")) {
        ui->chatBrowser->append(qMessage);
    } else {
        ui->chatBrowser->append("[" + localTime + "] " + QString::fromStdString(sender) + ": " + qMessage);
    }
}

void QtPrivateChatWindow::beginHistoryDisplay()
{
    historyDisplayed = false;
    ui->chatBrowser->clear();
}

void QtPrivateChatWindow::addHistoryMessage(const std::string& formattedMessage)
{
    ui->chatBrowser->append(QString::fromStdString(formattedMessage));
}

void QtPrivateChatWindow::endHistoryDisplay()
{
    //ui->chatBrowser->append("---------- КОНЕЦ ИСТОРИИ ЛИЧНЫХ СООБЩЕНИЙ ----------");
    ui->chatBrowser->append(""); // просто отделение истории от новых сообщений
    historyDisplayed = true;

    // Добавляем сообщение о статусе после загрузки истории
    if (statusMessagePending) {
        addStatusMessage();
    }

    emit historyDisplayCompleted(QString::fromStdString(username));
}

void QtPrivateChatWindow::markMessagesAsRead()
{
    if (mainWindow) {
        mainWindow->sendMessageToServer(QString("MARK_READ:%1").arg(QString::fromStdString(username)).toStdString());
    }
}

QString QtPrivateChatWindow::convertUtcToLocalTime(const QString& utcTimestamp)
{
    if (utcTimestamp.contains("-") && utcTimestamp.contains(":")) {
        QDateTime utcTime = QDateTime::fromString(utcTimestamp, "yyyy-MM-dd hh:mm:ss");
        utcTime.setTimeZone(QTimeZone::UTC);
        return utcTime.toLocalTime().toString("hh:mm");
    }
    return utcTimestamp;
}
