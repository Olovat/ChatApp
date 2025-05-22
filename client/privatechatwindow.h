#ifndef PRIVATECHATWINDOW_H
#define PRIVATECHATWINDOW_H

#include <QWidget>
#include <QDate>

// Убираем прямую зависимость от MainWindow
class MainWindowController;

namespace Ui {
class PrivateChatWindow;
}

class PrivateChatWindow : public QWidget
{
    Q_OBJECT

public:
    explicit PrivateChatWindow(const QString &username, QWidget *parent = nullptr);
    void setController(MainWindowController *controller); // Новый метод для установки контроллера
    ~PrivateChatWindow();

    void receiveMessage(const QString &sender, const QString &message);
    void receiveMessage(const QString &sender, const QString &message, const QString &timestamp);
    void setOfflineStatus(bool offline);

    void beginHistoryDisplay();
    void addHistoryMessage(const QString &formattedMessage);
    void endHistoryDisplay();
    
    void markMessagesAsRead();

signals:
    void historyDisplayCompleted(const QString &username); // Сигнал завершения отображения истории
    void requestMessageHistory(const QString &username); // Сигнал запроса истории сообщений
    void messageSent(const QString &recipient, const QString &message); // Сигнал отправки сообщения

private slots:
    void on_sendButton_clicked();

private:
    Ui::PrivateChatWindow *ui;
    QString username;
    MainWindowController *controller;
    bool isOffline;
      void sendMessage(const QString &message);
    void updateWindowTitle();
    void addStatusMessage();
    QString convertUtcToLocalTime(const QString &utcTimestamp);// Флаги для отображения истории сообщений
    bool historyDisplayed = false;
    bool statusMessagePending = false;
    bool previousOfflineStatus = false;
};

#endif // PRIVATECHATWINDOW_H
