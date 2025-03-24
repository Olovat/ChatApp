#include "privatechatwindow.h"
#include "ui_privatechatwindow.h"
#include "mainwindow.h"
#include <QDebug>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTime>
#include <QDateTime>
#include <QShortcut>

PrivateChatWindow::PrivateChatWindow(const QString &username, MainWindow *mainWindow, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PrivateChatWindow),
    username(username),
    mainWindow(mainWindow),
    isOffline(false),
    statusMessagePending(false),
    previousOfflineStatus(false),
    lastMessageDate(QDate())
{
    ui->setupUi(this);

    updateWindowTitle();

    // Исправляем соединение сигналов и слотов
    connect(ui->sendButton, SIGNAL(clicked()), this, SLOT(on_sendButton_clicked()));

    // Настройка горячих клавиш для отправки сообщений
    QShortcut *shortcut = new QShortcut(QKeySequence(Qt::Key_Return), ui->messageEdit);
    connect(shortcut, SIGNAL(activated()), this, SLOT(on_sendButton_clicked()));

    if (mainWindow) {
        // Запрашиваем историю сообщений у сервера
        mainWindow->requestPrivateMessageHistory(username);
    }
}

void PrivateChatWindow::setOfflineStatus(bool offline)
{
    // Обновляем только если статус действительно изменился
    if (isOffline != offline) {
        previousOfflineStatus = isOffline;
        isOffline = offline;
        updateWindowTitle();
        
        // Отложим добавление сообщения о статусе, если история еще загружается
        statusMessagePending = true;
        
        // Если история уже загружена, добавим сообщение о статусе сразу
        if (historyDisplayed) {
            addStatusMessage();
        }
    }
}

// Новый метод для добавления сообщения о статусе
void PrivateChatWindow::addStatusMessage()
{
    if (!statusMessagePending) return;
    
    QTime currentTime = QTime::currentTime();
    QString timeStr = currentTime.toString("hh:mm");
    
    if (isOffline) {
        // Сообщение об уходе пользователя
        ui->chatBrowser->append("<i>[" + timeStr + "] Пользователь " + username + " вышел из сети. Сообщения ниже будут доставлены, когда пользователь вернется.</i>");
    } else if (!previousOfflineStatus) {
        // Сообщение о первом входе пользователя (избегаем дублирования)
        ui->chatBrowser->append("<i>[" + timeStr + "] Пользователь " + username + " в сети.</i>");
    } else {
        // Сообщение о возвращении пользователя
        ui->chatBrowser->append("<i>[" + timeStr + "] Пользователь " + username + " вернулся в сеть.</i>");
    }
    
    statusMessagePending = false;
}

void PrivateChatWindow::updateWindowTitle()
{
    QString statusIndicator = isOffline ? " (Не в сети)" : " (В сети)";
    setWindowTitle("Чат с " + username + statusIndicator);
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

// Метод для конвертации UTC времени в локальное
QString PrivateChatWindow::convertUtcToLocalTime(const QString &utcTimestamp)
{
    // Если строка времени содержит дату и время
    if (utcTimestamp.contains("-") && utcTimestamp.contains(":")) {
        QDateTime utcTime = QDateTime::fromString(utcTimestamp, "yyyy-MM-dd hh:mm:ss");
        utcTime.setTimeZone(QTimeZone::UTC);
        QDateTime localTime = utcTime.toLocalTime();
        return localTime.toString("hh:mm");
    }
    // Если строка времени содержит только время (уже в локальном формате)
    else if (utcTimestamp.contains(":")) {
        return utcTimestamp;
    }
    // Возвращаем исходное значение, если формат неизвестен
    return utcTimestamp;
}

void PrivateChatWindow::receiveMessage(const QString &sender, const QString &message)
{
    QTime currentTime = QTime::currentTime();
    QString timeStr = currentTime.toString("hh:mm");
    receiveMessage(sender, message, timeStr);
}

void PrivateChatWindow::receiveMessage(const QString &sender, const QString &message, const QString &timestamp)
{
    qDebug() << "PrivateChatWindow: Получено сообщение от" << sender << ":" << message << "в" << timestamp;
    
    // Конвертируем время UTC в локальное, если необходимо
    QString localTimeStr;
    QDateTime messageDateTime;
    
    // Если строка времени содержит дату и время
    if (timestamp.contains("-") && timestamp.contains(":")) {
        QDateTime utcTime = QDateTime::fromString(timestamp, "yyyy-MM-dd hh:mm:ss");
        utcTime.setTimeZone(QTimeZone::UTC);
        messageDateTime = utcTime.toLocalTime();
        localTimeStr = messageDateTime.toString("hh:mm");
    } else if (timestamp.contains(":")) {
        // Если строка содержит только время, создаем QDateTime с текущей датой
        QTime time = QTime::fromString(timestamp, "hh:mm");
        messageDateTime = QDateTime(QDate::currentDate(), time);
        localTimeStr = timestamp;
    } else {
        // Если формат неизвестен, используем текущее время
        messageDateTime = QDateTime::currentDateTime();
        localTimeStr = timestamp;
    }
    
    // Добавляем разделитель даты, если необходимо
    addDateSeparatorIfNeeded(messageDateTime);
    
    // Если сообщение уже содержит временную метку в формате [timestamp], используем его как есть без изменения выравнивания
    if (message.startsWith("[") && message.contains("]")) {
        ui->chatBrowser->append(message);
    } else {
        // Отображаем сообщение без специального выравнивания - оно будет слева по умолчанию
        ui->chatBrowser->append("[" + localTimeStr + "] " + sender + ": " + message);
    }
}

// Метод для добавления разделителя даты
void PrivateChatWindow::addDateSeparatorIfNeeded(const QDateTime &messageDateTime)
{
    QDate messageDate = messageDateTime.date();
    if (lastMessageDate != messageDate) {
        // Форматируем строку даты
        QString dateStr = messageDate.toString("d MMMM yyyy");
        
        // Добавляем красивый разделитель с датой, центрированный
        ui->chatBrowser->append("<div style='text-align:center'><span style='background-color: #e6e6e6; padding: 3px 10px; border-radius: 10px;'>" + dateStr + "</span></div>");
        
        // Обновляем последнюю дату
        lastMessageDate = messageDate;
    }
}

void PrivateChatWindow::beginHistoryDisplay()
{
    historyDisplayed = false;
    // Очищаем чат перед отображением истории
    ui->chatBrowser->clear();
    // Сбрасываем дату последнего сообщения
    lastMessageDate = QDate();
}

void PrivateChatWindow::addHistoryMessage(const QString &formattedMessage)
{
    // История уже форматируется в MainWindow с учетом временной зоны
    ui->chatBrowser->append(formattedMessage);
}

void PrivateChatWindow::endHistoryDisplay()
{
    //ui->chatBrowser->append("---------- КОНЕЦ ИСТОРИИ ЛИЧНЫХ СООБЩЕНИЙ ----------");
    ui->chatBrowser->append(""); // просто отделение истории от новых сообщений
    historyDisplayed = true;
    
    // Добавляем сообщение о статусе после загрузки истории
    if (statusMessagePending) {
        addStatusMessage();
    }
}
