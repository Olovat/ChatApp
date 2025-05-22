#include "groupchatwindow.h"
#include "ui_groupchatwindow.h"
#include "mainwindow_controller.h"
#include <QDebug>
#include <QTime>
#include <QDateTime>
#include <QShortcut>
#include <QTimeZone>
#include <QMessageBox>

GroupChatWindow::GroupChatWindow(const QString &chatId, const QString &chatName, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Form),
    chatId(chatId),
    chatName(chatName),
    controller(nullptr),
    isCreator(false),  // Инициализируем переменную isCreator
    historyDisplayed(false)  // Порядок должен соответствовать порядку объявления в классе
{
    ui->setupUi(this);
    
    // Скрываем кнопку удаления по умолчанию
    ui->pushButton_4->setVisible(false);
    
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
      // Сигналы будут подключены когда будет установлен контроллер
    
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
    if (controller) {
        qDebug() << "GroupChatWindow: Отправка сообщения в чат" << chatName << ":" << message;
        
        // Отправляем сообщение через сигнал
        emit messageSent(chatId, message);
        
        // Сообщение отобразится в чате после получения ответа от сервера
    } else {
        qDebug() << "GroupChatWindow: ОШИБКА - controller равен nullptr!";
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

void GroupChatWindow::beginHistoryDisplay()
{
    historyDisplayed = false;
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
      // Проверяем, является ли текущий пользователь создателем чата
    // TODO: Нужно получить текущее имя пользователя из контроллера
    // Временно устанавливаем false, будет обновлено при установке контроллера
    bool isCurrentUserCreator = false;
    this->isCreator = isCurrentUserCreator; // Сохраняем статус создателя
    
    // Делаем кнопку удаления видимой только для создателя
    if (ui->pushButton_4) {
        ui->pushButton_4->setVisible(isCurrentUserCreator);
    }
    
    // Обновляем список участников, чтобы отметить создателя
    if (ui->userListWidget) {
        for (int i = 0; i < ui->userListWidget->count(); i++) {
            QListWidgetItem* item = ui->userListWidget->item(i);
            if (item && item->text() == creator) {
                // Добавляем метку "(создатель)" если её ещё нет
                if (!item->text().endsWith(" (создатель)")) {
                    item->setText(creator + " (создатель)");
                    QFont font = item->font();
                    font.setBold(true);
                    item->setFont(font);
                }
                break;
            }
        }
    }
    
    // Обновляем заголовок окна, чтобы показать, что пользователь - создатель
    if (isCurrentUserCreator) {
        setWindowTitle("Групповой чат: " + chatName + " (Вы создатель)");
    } else {
        setWindowTitle("Групповой чат: " + chatName);
    }
      qDebug() << "Установлен создатель чата:" << creator << "Текущий пользователь:" 
             << (controller ? "через контроллер" : "unknown")
             << "Кнопка удаления " << (isCurrentUserCreator ? "показана" : "скрыта");
}

// Обработчик кнопки добавления пользователя в чат
void GroupChatWindow::on_pushButton_clicked()
{
    // Скрываем текущее окно
    this->hide();
      // Отправляем запрос на добавление пользователя серверу
    if (controller) {
        QMessageBox::information(this, "Добавление пользователя", 
                              "Выберите пользователя в главном окне для добавления в чат");
        
        // Отправляем сигнал для установки режима добавления пользователя
        emit addUserRequested(chatId, "");
        
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
        // Отправляем сигнал на удаление пользователя
        if (controller) {
            emit removeUserRequested(chatId, selectedUser);
        }
    }
}

void GroupChatWindow::on_pushButton_4_clicked()
{
    // Проверка безопасности - убедиться, что пользователь действительно создатель
    // Эта проверка заменена на проверку isCreator
    if (!isCreator) {
        QMessageBox::warning(this, "Ошибка", "У вас нет прав для удаления этого чата");
        return;
    }
    
    // Запрос подтверждения удаления
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, 
        "Подтверждение удаления", 
        "Вы уверены, что хотите удалить групповой чат \"" + chatName + "\"?\n"
        "Это действие нельзя отменить и чат будет удалён для всех участников.",
        QMessageBox::Yes | QMessageBox::No
    );      if (reply == QMessageBox::Yes) {
        // Отправляем сигнал на удаление чата
        if (controller) {
            emit deleteChatRequested(chatId);
            
            // Закрываем окно
            close();
            
            // Обновление будет запрошено через контроллер
        }
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

void GroupChatWindow::setController(MainWindowController *controller)
{
    this->controller = controller;
    
    if (controller) {
        // Подключаем сигналы к контроллеру
        connect(this, &GroupChatWindow::messageSent, controller, &MainWindowController::handleGroupMessageSend);
        connect(this, &GroupChatWindow::addUserRequested, controller, &MainWindowController::handleAddUserToGroup);
        connect(this, &GroupChatWindow::removeUserRequested, controller, &MainWindowController::handleRemoveUserFromGroup);
        connect(this, &GroupChatWindow::deleteChatRequested, controller, &MainWindowController::handleDeleteGroupChat);
    }
}
