#include "groupchatwindow.h"
#include "ui_groupchatwindow.h"
#include "mainwindow_controller.h"
#include "groupchat_controller.h"
#include "chat_controller.h"
#include <QDebug>
#include <QTime>
#include <QDateTime>
#include <QShortcut>
#include <QTimeZone>
#include <QMessageBox>
#include <QCloseEvent>
#include <QShowEvent>
#include <QTimer>
#include <QScrollBar>

GroupChatWindow::GroupChatWindow(const QString &chatId, const QString &chatName, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Form),
    chatId(chatId),
    chatName(chatName),
    controller(nullptr),
    groupChatController(nullptr),
    isCreator(false),  // Инициализируем переменную isCreator
    historyDisplayed(false),  // Порядок должен соответствовать порядку объявления в классе
    initialHistoryRequested(false)
{
    ui->setupUi(this);
    setupUi();
    updateWindowTitle();
    
    // Устанавливаем таймер для загрузки истории сообщений
    QTimer::singleShot(300, this, [this, chatId]() {
        if (!initialHistoryRequested && groupChatController) {
            groupChatController->requestMessageHistory(chatId);
            initialHistoryRequested = true;
        }
    });
    
    qDebug() << "Created group chat window for" << chatName << "with ID" << chatId;
}

GroupChatWindow::~GroupChatWindow()
{
    qDebug() << "Destroyed group chat window for" << chatName;
    delete ui;
}

void GroupChatWindow::updateWindowTitle()
{
    // Базовый заголовок окна
    QString title = "Групповой чат: " + chatName;
    
    // Добавляем метку создателя, если текущий пользователь - создатель
    if (isCreator) {
        title += " (Вы создатель)";
    }
    
    setWindowTitle(title);
}

void GroupChatWindow::on_pushButton_2_clicked()
{
    sendCurrentMessage();
}

void GroupChatWindow::on_lineEdit_returnPressed()
{
    sendCurrentMessage();
}

void GroupChatWindow::sendMessage(const QString &message)
{
    if (groupChatController) {
        qDebug() << "GroupChatWindow: Отправка сообщения в чат" << chatName << ":" << message;
        
        // Отправляем сообщение через групповой контроллер
        emit messageSent(chatId, message);
        
        // Сообщение отобразится в чате после получения ответа от сервера
    } else if (controller) {
        qDebug() << "GroupChatWindow: Отправка сообщения через основной контроллер" << chatName << ":" << message;
        
        // Отправляем сообщение через основной контроллер (для обратной совместимости)
        emit messageSent(chatId, message);
    } else {
        qDebug() << "GroupChatWindow: ОШИБКА - ни один контроллер не установлен!";
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
    
    // Если окно активно, отмечаем сообщение как прочитанное
    if (isActiveWindow() && isVisible() && !isMinimized()) {
        emit markAsReadRequested(chatId);
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
        
        // Выделяем создателя
        if (member == creator) {
            item->setText(member + " (создатель)");
            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
        }
        
        ui->userListWidget->addItem(item);
    }
    
    qDebug() << "Обновлен список участников группового чата:" << chatName 
             << ", всего участников:" << members.size();
}

// Добавляем метод для установки создателя чата
void GroupChatWindow::setCreator(const QString &creator)
{
    this->creator = creator;
    
    // Получаем текущее имя пользователя из контроллера
    QString currentUser;
    if (controller) {
        ChatController* chatController = controller->getChatController();
        if (chatController) {
            currentUser = chatController->getCurrentUsername();
        }
    } else if (groupChatController) {
        // Получаем из groupChatController, если он доступен
        currentUser = groupChatController->getCurrentUsername();
    }
    
    // Проверяем, является ли текущий пользователь создателем чата
    bool isCurrentUserCreator = !currentUser.isEmpty() && (currentUser == creator);
    this->isCreator = isCurrentUserCreator; // Сохраняем статус создателя
    
    // Делаем кнопку удаления видимой только для создателя
    if (ui->pushButton_4) {
        ui->pushButton_4->setVisible(isCurrentUserCreator);
    }
    
    // Обновляем список участников, чтобы отметить создателя
    if (ui->userListWidget) {
        for (int i = 0; i < ui->userListWidget->count(); i++) {
            QListWidgetItem* item = ui->userListWidget->item(i);
            if (item) {
                // Сначала удаляем метку "(создатель)" у всех
                QString itemText = item->text();
                if (itemText.endsWith(" (создатель)")) {
                    itemText = itemText.left(itemText.length() - 12);
                    item->setText(itemText);
                    QFont font = item->font();
                    font.setBold(false);
                    item->setFont(font);
                }
                
                // Добавляем метку только создателю
                if (itemText == creator) {
                    item->setText(creator + " (создатель)");
                    QFont font = item->font();
                    font.setBold(true);
                    item->setFont(font);
                }
            }
        }
    }
    
    // Обновляем заголовок окна, чтобы показать, что пользователь - создатель
    if (isCurrentUserCreator) {
        setWindowTitle("Групповой чат: " + chatName + " (Вы создатель)");
    } else {
        setWindowTitle("Групповой чат: " + chatName);
    }
    
    qDebug() << "Установлен создатель чата:" << creator 
             << "Текущий пользователь:" << currentUser
             << "Кнопка удаления " << (isCurrentUserCreator ? "показана" : "скрыта");
}

// Обработчик кнопки добавления пользователя в чат
void GroupChatWindow::on_pushButton_clicked()
{
    // Активируем режим добавления пользователя в группу
    if (controller) {
        // Запрашиваем активацию режима добавления пользователя
        controller->handleStartAddUserToGroupMode(chatId);
        
        // Показываем информационное сообщение
        QMessageBox::information(this, "Добавление пользователя", 
                              "Выберите пользователя в главном окне для добавления в чат");
        
        // Скрываем текущее окно, чтобы пользователь мог выбрать из списка
        this->hide();
    } else if (groupChatController) {
        // Если есть groupChatController, используем его
        groupChatController->startAddUserToGroupMode(chatId);
        
        QMessageBox::information(this, "Добавление пользователя", 
                              "Выберите пользователя в главном окне для добавления в чат");
        
        // Скрываем текущее окно, чтобы пользователь мог выбрать из списка
        this->hide();
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

void GroupChatWindow::setGroupChatController(GroupChatController *controller)
{
    this->groupChatController = controller;
    
    if (controller) {
        // Подключаем сигналы к групповому контроллеру
        connect(this, &GroupChatWindow::messageSent, controller, &GroupChatController::sendMessage);
        connect(this, &GroupChatWindow::addUserRequested, controller, &GroupChatController::handleAddUserToGroup);
        connect(this, &GroupChatWindow::removeUserRequested, controller, &GroupChatController::handleRemoveUserFromGroup);
        connect(this, &GroupChatWindow::deleteChatRequested, controller, &GroupChatController::handleDeleteGroupChat);
        connect(this, &GroupChatWindow::windowClosed, controller, &GroupChatController::handleChatWindowClosed);
        connect(this, &GroupChatWindow::markAsReadRequested, controller, &GroupChatController::markMessagesAsRead);
    }
}

void GroupChatWindow::setupUi()
{
    // Скрываем кнопку удаления по умолчанию
    ui->pushButton_4->setVisible(false);
    
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
    ui->pushButton_4->setToolTip("Удалить чат (только для создателя)");
    
    // Устанавливаем фокус на поле ввода
    ui->lineEdit->setFocus();
}

void GroupChatWindow::displayMessages(const QList<GroupMessage> &messages)
{
    ui->textBrowser->clear();
    
    for (const GroupMessage &message : messages) {
        addMessageToChat(message.sender, message.content, message.timestamp);
    }
    
    // Прокручиваем к концу
    QScrollBar *scrollBar = ui->textBrowser->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void GroupChatWindow::clearChat()
{
    ui->textBrowser->clear();
}

void GroupChatWindow::addMessageToChat(const QString &sender, const QString &content, const QString &timestamp)
{
    // Конвертируем время UTC в локальное, если необходимо
    QString localTimeStr = convertUtcToLocalTime(timestamp);
    
    // Проверяем, не системное ли это сообщение
    if (sender == "SYSTEM") {
        ui->textBrowser->append("<i>[" + localTimeStr + "] " + content + "</i>");
    } else {
        ui->textBrowser->append("[" + localTimeStr + "] " + sender + ": " + content);
    }
}

// Публичные слоты для обновления UI
void GroupChatWindow::onUnreadCountChanged(int count)
{
    qDebug() << "Unread count changed for chat" << chatName << ":" << count;
    
    // Обновляем заголовок окна с учетом непрочитанных сообщений
    if (count > 0) {
        setWindowTitle(QString("Групповой чат: %1 (%2)").arg(chatName).arg(count));
    } else {
        // Возвращаем обычный заголовок
        updateWindowTitle();
    }
}

void GroupChatWindow::onMembersUpdated(const QStringList &members)
{
    updateMembersList(members);
}

void GroupChatWindow::onCreatorChanged(const QString &creator)
{
    setCreator(creator);
}

// Переопределенные методы для обработки событий окна
void GroupChatWindow::closeEvent(QCloseEvent *event)
{
    qDebug() << "Group chat window closing for" << chatName;
    emit windowClosed(chatId);
    event->accept();
}

void GroupChatWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    
    // Отмечаем сообщения как прочитанные при показе окна
    QTimer::singleShot(500, this, [this]() {
        emit markAsReadRequested(chatId);
    });
    
    // Запрашиваем начальную историю, если еще не запрашивали
    if (!initialHistoryRequested && groupChatController) {
        groupChatController->requestMessageHistory(chatId);
        initialHistoryRequested = true;
    }
}

void GroupChatWindow::sendCurrentMessage()
{
    QString message = ui->lineEdit->text().trimmed();
    if (!message.isEmpty()) {
        sendMessage(message);
        ui->lineEdit->clear();
    }
}

bool GroupChatWindow::event(QEvent *e)
{
    // Перехватываем событие активации окна
    if (e->type() == QEvent::WindowActivate) {
        qDebug() << "GroupChatWindow: Window activated for chat" << chatName;
        
        // Отмечаем сообщения как прочитанные при активации окна
        QTimer::singleShot(100, this, [this]() {
            emit markAsReadRequested(chatId);
        });
    }
    return QWidget::event(e);
}
