#include "qt_group_chat_window.h"
#include "ui_groupchatwindow.h"
#include "./mainwindow/qt_main_window.h"
#include <QDebug>
#include <QTime>
#include <QDateTime>
#include <QShortcut>
#include <QTimeZone>
#include <QMessageBox>

QtGroupChatWindow::QtGroupChatWindow(const QString &chatId, const QString &chatName,
                                     QtMainWindow *mainWindow, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Form),
    mainWindow(mainWindow)
{

    this->chatId = chatId.toStdString();
    this->chatName = chatName.toStdString();
    ui->setupUi(this);

    // Скрываем кнопку удаления по умолчанию
    ui->pushButton_4->setVisible(false);

    // Устанавливаем заголовок окна
    setWindowTitle(QString("Групповой чат: %1").arg(chatName));

    // Подключаем сигналы
    connect(ui->pushButton_2, &QPushButton::clicked, this, &QtGroupChatWindow::on_pushButton_2_clicked);
    connect(ui->lineEdit, &QLineEdit::returnPressed, this, &QtGroupChatWindow::on_lineEdit_returnPressed);

    // Горячая клавиша Enter для отправки сообщения
    QShortcut *shortcut = new QShortcut(QKeySequence(Qt::Key_Return), ui->lineEdit, nullptr, nullptr, Qt::WidgetShortcut);
    connect(shortcut, &QShortcut::activated, this, &QtGroupChatWindow::on_pushButton_2_clicked);

    // Настройка кнопок
    ui->pushButton->setEnabled(true);
    ui->pushButton_3->setEnabled(true);
    ui->pushButton->setToolTip(tr("Добавить пользователя в чат"));
    ui->pushButton_3->setToolTip(tr("Удалить выбранного пользователя из чата"));

    // Запрашиваем информацию о создателе чата
    if (mainWindow) {
        mainWindow->sendMessageToServer(
            QString("GROUP_GET_CREATOR:%1").arg(chatId).toStdString());
    }
}

QtGroupChatWindow::~QtGroupChatWindow()
{
    delete ui;
}

void QtGroupChatWindow::updateWindowTitle()
{
    setWindowTitle("Групповой чат: " + QString::fromStdString(chatName));
}

void QtGroupChatWindow::receiveMessage(const std::string& sender, const std::string& message)
{
    receiveMessage(sender, message, QTime::currentTime().toString("hh:mm").toStdString());
}

void QtGroupChatWindow::receiveMessage(const std::string& sender, const std::string& message,
                                       const std::string& timestamp)
{
    QString localTimeStr = convertUtcToLocalTime(QString::fromStdString(timestamp));
    QString qSender = QString::fromStdString(sender);
    QString qMessage = QString::fromStdString(message);

    if (qSender == "SYSTEM") {
        ui->textBrowser->append("<i>[" + localTimeStr + "] " + qMessage + "</i>");
    } else {
        ui->textBrowser->append("[" + localTimeStr + "] " + qSender + ": " + qMessage);
    }
}

void QtGroupChatWindow::beginHistoryDisplay()
{
    historyDisplayed = false;
    ui->textBrowser->clear();
}

void QtGroupChatWindow::addHistoryMessage(const std::string& formattedMessage)
{
    ui->textBrowser->append(QString::fromStdString(formattedMessage));
}

void QtGroupChatWindow::endHistoryDisplay()
{
    ui->textBrowser->append("");
    historyDisplayed = true;
}

void QtGroupChatWindow::updateMembersList(const std::vector<std::string>& members)
{
    ui->userListWidget->clear();
    for (const auto& member : members) {
        ui->userListWidget->addItem(QString::fromStdString(member));
    }
}

void QtGroupChatWindow::setCreator(const std::string& creator)
{
    this->creator = creator;
    bool isCurrentUserCreator = (mainWindow &&
                                 QString::fromStdString(mainWindow->getUsername()) == QString::fromStdString(creator));
    this->isCreator = isCurrentUserCreator;

    ui->pushButton_4->setVisible(isCurrentUserCreator);

    for (int i = 0; i < ui->userListWidget->count(); i++) {
        QListWidgetItem* item = ui->userListWidget->item(i);
        if (item && item->text().toStdString() == creator) {
            item->setText(QString::fromStdString(creator) + " (создатель)");
            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
            break;
        }
    }

    if (isCurrentUserCreator) {
        setWindowTitle("Групповой чат: " + QString::fromStdString(chatName) + " (Вы создатель)");
    }
}

void QtGroupChatWindow::on_pushButton_2_clicked()
{
    QString message = ui->lineEdit->text().trimmed();
    if (!message.isEmpty()) {
        sendMessage(message);
        ui->lineEdit->clear();
    }
}

void QtGroupChatWindow::on_lineEdit_returnPressed()
{
    on_pushButton_2_clicked();
}

void QtGroupChatWindow::sendMessage(const QString &message)
{
    if (mainWindow) {
        qDebug() << "GroupChatWindow: Отправка сообщения в чат" << QString::fromStdString(chatName) << ":" << message;

        QString groupMessage = QString("GROUP_MESSAGE:%1:%2").arg(QString::fromStdString(chatId), message);
        mainWindow->sendMessageToServer(groupMessage.toStdString());
    } else {
        qDebug() << "GroupChatWindow: ОШИБКА - mainWindow равен nullptr!";
    }
}

void QtGroupChatWindow::on_pushButton_clicked()
{
    this->hide();

    if (mainWindow) {
        QMessageBox::information(this, "Добавление пользователя",
                                 "Выберите пользователя в главном окне для добавления в чат");

        mainWindow->startAddUserToGroupMode(chatId);
        QApplication::processEvents();
    }
}

void QtGroupChatWindow::on_pushButton_3_clicked()
{
    QListWidgetItem *selectedItem = ui->userListWidget->currentItem();
    if (!selectedItem) {
        QMessageBox::warning(this, "Ошибка", "Выберите пользователя для удаления");
        return;
    }

    QString selectedUser = selectedItem->text();
    if (selectedUser == QString::fromStdString(creator) || selectedUser == QString::fromStdString(creator) + " (создатель)") {
        QMessageBox::warning(this, "Ошибка", "Невозможно удалить создателя чата");
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Подтверждение удаления",
        "Вы уверены, что хотите удалить пользователя " + selectedUser + " из чата?",
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes && mainWindow) {
        mainWindow->sendMessageToServer(
            QString("GROUP_REMOVE_USER:%1:%2")
                .arg(QString::fromStdString(chatId), selectedUser)
                .toStdString());
    }
}

void QtGroupChatWindow::on_pushButton_4_clicked()
{
    if (!mainWindow || QString::fromStdString(mainWindow->getUsername()) != QString::fromStdString(creator)) {
        QMessageBox::warning(this, "Ошибка", "У вас нет прав для удаления этого чата");
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Подтверждение удаления",
        "Вы уверены, что хотите удалить групповой чат \"" + QString::fromStdString(chatName) + "\"?\n"
                                                                                               "Это действие нельзя отменить и чат будет удалён для всех участников.",
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes && mainWindow) {
        mainWindow->sendMessageToServer(
            QString("DELETE_GROUP_CHAT:%1")
                .arg(QString::fromStdString(chatId))
                .toStdString());
        close();
        mainWindow->sendMessageToServer("GET_USERLIST");
    }
}

QString QtGroupChatWindow::convertUtcToLocalTime(const QString &utcTimestamp)
{
    if (utcTimestamp.contains("-") && utcTimestamp.contains(":")) {
        QDateTime utcTime = QDateTime::fromString(utcTimestamp, "yyyy-MM-dd hh:mm:ss");
        utcTime.setTimeZone(QTimeZone::UTC);
        QDateTime localTime = utcTime.toLocalTime();
        return localTime.toString("hh:mm");
    }
    else if (utcTimestamp.contains(":")) {
        return utcTimestamp;
    }
    return utcTimestamp;
}
