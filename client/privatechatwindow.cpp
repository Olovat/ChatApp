#include "privatechatwindow.h"
#include "ui_privatechatwindow.h"
#include "privatechat_controller.h"
#include <QDateTime>
#include <QScrollBar>
#include <QMessageBox>
#include <QDebug>
#include <QCloseEvent>
#include <QShowEvent>
#include <QTimer>

PrivateChatWindow::PrivateChatWindow(const QString &peerUsername, QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::PrivateChatWindow),
      m_controller(nullptr),
      m_peerUsername(peerUsername)  // Исправление порядка инициализации
{
    ui->setupUi(this);
    setupUi();
    updateWindowTitle();
    
    // Устанавливаем таймер для загрузки истории сообщений
    QTimer::singleShot(300, this, &PrivateChatWindow::requestInitialHistory);
    
    qDebug() << "Created private chat window for conversation with" << peerUsername;
}

PrivateChatWindow::~PrivateChatWindow()
{
    qDebug() << "Destroyed private chat window for conversation with" << m_peerUsername;
    delete ui;
}

void PrivateChatWindow::setController(PrivateChatController *controller)
{
    m_controller = controller;
}

void PrivateChatWindow::setupUi()
{
    // Создаем метку статуса в правом угле строки состояния
    m_statusLabel = new QLabel(this);
    ui->statusbar->addPermanentWidget(m_statusLabel);
    updateUserStatus(false); // По умолчанию оффлайн
    
    // Настройка для авто-прокрутки чата
    connect(ui->chatTextEdit->verticalScrollBar(), &QScrollBar::rangeChanged, 
            [this](int min, int max){ 
                Q_UNUSED(min);
                ui->chatTextEdit->verticalScrollBar()->setValue(max); 
            });
}

void PrivateChatWindow::updateUserStatus(bool isOnline)
{
    if (isOnline) {
        m_statusLabel->setText("В сети");
        m_statusLabel->setStyleSheet("color: green;");
    } else {
        m_statusLabel->setText("Не в сети");
        m_statusLabel->setStyleSheet("color: gray;");
    }
}

void PrivateChatWindow::showLoadingIndicator()
{
    // Показываем индикатор загрузки только если в чате пусто
    if (ui->chatTextEdit->toPlainText().isEmpty()) {
        ui->chatTextEdit->setPlainText("Загрузка истории сообщений...");
    }
}

void PrivateChatWindow::displayMessages(const QList<PrivateMessage> &messages)
{
    ui->chatTextEdit->clear();
    
    if (messages.isEmpty()) {
        ui->chatTextEdit->setPlainText("История сообщений пуста");
        return;
    }
    
    for (const PrivateMessage &message : messages) {
        addMessageToChat(message.sender, message.content, message.timestamp);
    }
    
    // Прокручиваем к последнему сообщению
    QScrollBar *scrollBar = ui->chatTextEdit->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void PrivateChatWindow::addMessageToChat(const QString &sender, const QString &content, const QString &timestamp)
{
    QString timeStr = timestamp.isEmpty() ? 
                      QDateTime::currentDateTime().toString("hh:mm:ss") : 
                      timestamp;
                      
    QString formattedMessage;
    if (sender == m_peerUsername) {
        formattedMessage = QString("<div style='margin-bottom: 5px;'><span style='color: blue; font-weight: bold;'>%1</span> (%2):<br>%3</div>")
                            .arg(sender, timeStr, content);
    } else {
        formattedMessage = QString("<div style='margin-bottom: 5px;'><span style='color: green; font-weight: bold;'>Вы</span> (%1):<br>%2</div>")
                            .arg(timeStr, content);
    }
    
    ui->chatTextEdit->append(formattedMessage);
}

void PrivateChatWindow::receiveMessage(const QString &sender, const QString &message, const QString &timestamp)
{
    addMessageToChat(sender, message, timestamp);
    
    // Если окно не активно, показать уведомление
    if (!this->isActiveWindow()) {
        this->activateWindow();
        this->raise();
    }
}

void PrivateChatWindow::on_sendButton_clicked()
{
    sendCurrentMessage();
}

void PrivateChatWindow::on_messageEdit_returnPressed()
{
    sendCurrentMessage();
}

void PrivateChatWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    
    qDebug() << "Window shown for" << m_peerUsername;
    
    // Отправляем сигнал о показе окна
    emit shown();
    emit markAsReadRequested(m_peerUsername);
    
    // Всегда запрашиваем историю при показе окна
    QTimer::singleShot(200, [this]() {
        requestInitialHistory();
    });
}

void PrivateChatWindow::requestInitialHistory()
{
    if (m_controller) {
        qDebug() << "Explicitly requesting initial history for" << m_peerUsername;
        showLoadingIndicator();
        m_initialHistoryRequested = true;
    } else {
        qDebug() << "ERROR: Controller is null in requestInitialHistory!";
    }
}

void PrivateChatWindow::sendCurrentMessage()
{
    QString message = ui->messageEdit->text().trimmed();
    if (message.isEmpty())
        return;
    
    // Защита от краша
    if (!m_controller) {
        qDebug() << "CRITICAL: Controller is null when trying to send message!";
        QMessageBox::warning(this, "Ошибка", "Соединение с сервером потеряно. Пожалуйста, перезапустите приложение.");
        return;
    }
    
    // Отправляем сообщение
    try {
        emit messageSent(m_peerUsername, message);
        ui->messageEdit->clear();
        
        // Явно устанавливаем фокус на поле ввода и активируем окно
        QTimer::singleShot(50, this, [this]() {
            this->activateWindow();
            this->raise();
            ui->messageEdit->setFocus();
        });
    } catch (const std::exception& e) {
        qDebug() << "Exception when sending message:" << e.what();
        QMessageBox::critical(this, "Ошибка", QString("Произошла ошибка при отправке сообщения: %1").arg(e.what()));
    } catch (...) {
        qDebug() << "Unknown exception when sending message";
        QMessageBox::critical(this, "Ошибка", "Произошла неизвестная ошибка при отправке сообщения");
    }
}

void PrivateChatWindow::onPeerStatusChanged(bool isOnline)
{
    updateUserStatus(isOnline);
}

void PrivateChatWindow::onUnreadCountChanged(int count)
{
    updateWindowTitle();
    if (count > 0 && this->isVisible()) {
        // Если окно видимо, помечаем как прочитанные
        emit markAsReadRequested(m_peerUsername);
    }
}

void PrivateChatWindow::clearChat()
{
    ui->chatTextEdit->clear();
}

void PrivateChatWindow::updateWindowTitle()
{
    setWindowTitle(QString("Чат с %1").arg(m_peerUsername));
}

void PrivateChatWindow::closeEvent(QCloseEvent *event)
{
    // Перед закрытием окна, отправляем сигнал о закрытии
    emit windowClosed(m_peerUsername);
    
    // Просто скрываем окно вместо закрытия
    hide();
    event->ignore();  // Игнорируем событие, чтобы окно не уничтожалось
    
    // Важно зафиксировать, что окно было скрыто
    qDebug() << "Chat window with" << m_peerUsername << "was hidden (not closed)";
}
