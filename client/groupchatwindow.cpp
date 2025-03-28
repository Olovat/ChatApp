#include "groupchatwindow.h"
#include "ui_groupchatwindow.h"
#include "mainwindow.h"
#include <QDebug>
#include <QTime>
#include <QDateTime>
#include <QShortcut>
#include <QTimeZone>
#include <QMessageBox>

GroupChatWindow::GroupChatWindow(const QString &chatId, const QString &chatName, MainWindow *mainWindow, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Form),
    chatId(chatId),
    chatName(chatName),
    mainWindow(mainWindow),
    isCreator(false),  // Инициализируем переменную isCreator
    historyDisplayed(false)  // Порядок должен соответствовать порядку объявления в классе
{
    ui->setupUi(this);
    
    // Устанавливаем заголовок окна
    updateWindowTitle();
    
    // Подключаем сигналы кнопки отправки и поля ввода
    connect(ui->pushButton_2, &QPushButton::clicked, this, &GroupChatWindow::on_pushButton_2_clicked);
    connect(ui->lineEdit, &QLineEdit::returnPressed, this, &GroupChatWindow::on_lineEdit_returnPressed);
    
    // Добавляем горячую клавишу Enter для отправки
    QShortcut *shortcut = new QShortcut(QKeySequence(Qt::Key_Return), ui->lineEdit);
    connect(shortcut, &QShortcut::activated, this, &GroupChatWindow::on_pushButton_2_clicked);
    
    // Всегда делаем кнопки активными сразу
    ui->pushButton->setEnabled(true);
    ui->pushButton_3->setEnabled(true);
    
    // Устанавливаем подсказки для кнопок
    ui->pushButton->setToolTip("Добавить пользователя в чат");
    ui->pushButton_3->setToolTip("Удалить выбранного пользователя из чата");
    
    // Запрашиваем информацию о создателе чата только для отображения
    if (mainWindow) {
        mainWindow->sendMessageToServer(QString("GROUP_GET_CREATOR:%1").arg(chatId));
    }
    
    qDebug() << "Создано окно группового чата:" << chatName << "(ID:" << chatId << ")";
}

GroupChatWindow::~GroupChatWindow()
{
    delete ui;
}

void GroupChatWindow::updateWindowTitle()
{
    setWindowTitle("Групповой чат: " + chatName);
}

void GroupChatWindow::on_pushButton_2_clicked()
{
    QString message = ui->lineEdit->text().trimmed();
    if (!message.isEmpty()) {
        sendMessage(message);
        ui->lineEdit->clear();
    }
}

void GroupChatWindow::on_lineEdit_returnPressed()
{
    on_pushButton_2_clicked();
}

void GroupChatWindow::sendMessage(const QString &message)
{
    if (mainWindow) {
        qDebug() << "GroupChatWindow: Отправка сообщения в чат" << chatName << ":" << message;
        
        // Формируем сообщение для отправки на сервер
        QString groupMessage = QString("GROUP_MESSAGE:%1:%2").arg(chatId, message);
        mainWindow->sendMessageToServer(groupMessage);
        
        // Сообщение отобразится в чате после получения ответа от сервера
    } else {
        qDebug() << "GroupChatWindow: ОШИБКА - mainWindow равен nullptr!";
    }
}

void GroupChatWindow::receiveMessage(const QString &sender, const QString &message)
{
    QTime currentTime = QTime::currentTime();
    QString timeStr = currentTime.toString("hh:mm");
    receiveMessage(sender, message, timeStr);
}

void GroupChatWindow::receiveMessage(const QString &sender, const QString &message, const QString &timestamp)
{
    qDebug() << "GroupChatWindow: Получено сообщение от" << sender << ":" << message << "в" << timestamp;
    
    // Конвертируем время UTC в локальное, если необходимо
    QString localTimeStr = convertUtcToLocalTime(timestamp);
    
    // Проверяем, не системное ли это сообщение
    if (sender == "SYSTEM") {
        ui->textBrowser->append("<i>[" + localTimeStr + "] " + message + "</i>");
    } else {
        ui->textBrowser->append("[" + localTimeStr + "] " + sender + ": " + message);
    }
}

// Метод для конвертации UTC времени в локальное
QString GroupChatWindow::convertUtcToLocalTime(const QString &utcTimestamp)
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

void GroupChatWindow::beginHistoryDisplay()
{
    historyDisplayed = false;
    // Очищаем чат перед отображением истории
    ui->textBrowser->clear();
}

void GroupChatWindow::addHistoryMessage(const QString &formattedMessage)
{
    // История уже форматируется в MainWindow с учетом временной зоны
    ui->textBrowser->append(formattedMessage);
}

void GroupChatWindow::endHistoryDisplay()
{
    ui->textBrowser->append(""); // просто отделение истории от новых сообщений
    historyDisplayed = true;
}

void GroupChatWindow::updateMembersList(const QStringList &members)
{
    ui->userListWidget->clear();
    
    qDebug() << "Обновление списка участников чата" << chatName << "- новый список:" << members;
    
    for (const QString &member : members) {
        QListWidgetItem *item = new QListWidgetItem(member);
        ui->userListWidget->addItem(item);
    }
    
    qDebug() << "Обновлен список участников группового чата:" << chatName;
}

// Добавляем метод для установки создателя чата
void GroupChatWindow::setCreator(const QString &creator)
{
    this->creator = creator;
    
    // Сохраняем информацию о том, является ли текущий пользователь создателем
    if (mainWindow) {
        isCreator = (mainWindow->getCurrentUsername() == creator);
        
        // Не меняем состояние включенности кнопок, они всегда активны
        // Но можем обновить подсказки или другую информацию
        
        qDebug() << "Статус создателя чата:" << isCreator << "для пользователя" << mainWindow->getCurrentUsername();
    }
}

// Обработчик кнопки добавления пользователя в чат
void GroupChatWindow::on_pushButton_clicked()
{
    // Скрываем текущее окно
    this->hide();
    
    // Отправляем запрос на добавление пользователя серверу
    if (mainWindow) {
        QMessageBox::information(this, "Добавление пользователя", 
                              "Выберите пользователя в главном окне для добавления в чат");
        
        // Устанавливаем режим добавления пользователя в главном окне
        mainWindow->startAddUserToGroupMode(chatId);
        
        // Важно! Сохраняем окно активным, чтобы его можно было вернуть
        QApplication::processEvents();
    }
}

// Обработчик кнопки удаления пользователя из чата
void GroupChatWindow::on_pushButton_3_clicked()
{
    // Убрана проверка на создателя чата
    
    // Получаем выбранный элемент в списке участников
    QListWidgetItem *selectedItem = ui->userListWidget->currentItem();
    if (!selectedItem) {
        QMessageBox::warning(this, "Ошибка", "Выберите пользователя для удаления");
        return;
    }
    
    QString selectedUser = selectedItem->text();
    
    // Оставляем проверку на удаление создателя чата
    if (selectedUser == creator || selectedUser == creator + " (создатель)") {
        QMessageBox::warning(this, "Ошибка", "Невозможно удалить создателя чата");
        return;
    }
    
    // Запрашиваем подтверждение
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Подтверждение удаления", 
                               "Вы уверены, что хотите удалить пользователя " + selectedUser + " из чата?",
                               QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        // Отправляем запрос на удаление пользователя
        if (mainWindow) {
            mainWindow->sendMessageToServer(QString("GROUP_REMOVE_USER:%1:%2").arg(chatId, selectedUser));
        }
    }
}
