#ifndef PRIVATECHATWINDOW_H
#define PRIVATECHATWINDOW_H

#include <QWidget>
#include <QDate>

class MainWindow;

namespace Ui {
class PrivateChatWindow;
}

class PrivateChatWindow : public QWidget
{
    Q_OBJECT

public:
    explicit PrivateChatWindow(const QString &username, MainWindow *mainWindow, QWidget *parent = nullptr);
    ~PrivateChatWindow();

    void receiveMessage(const QString &sender, const QString &message);
    void receiveMessage(const QString &sender, const QString &message, const QString &timestamp);
    void setOfflineStatus(bool offline);

    void beginHistoryDisplay();
    void addHistoryMessage(const QString &formattedMessage);
    void endHistoryDisplay();
    
    void markMessagesAsRead();

signals:
    void historyDisplayCompleted(const QString &username); // New signal

private slots:
    void on_sendButton_clicked();

private:
    Ui::PrivateChatWindow *ui;
    QString username;
    MainWindow *mainWindow;
    bool isOffline;
    
    void sendMessage(const QString &message);
    void updateWindowTitle();

    // Флаги для отображения истории сообщений
    bool historyDisplayed = false;
    bool statusMessagePending = false;
    bool previousOfflineStatus = false;
    
    void addStatusMessage();
    
    // Метод для конвертации времени UTC в локальное
    QString convertUtcToLocalTime(const QString &utcTimestamp);
};

#endif // PRIVATECHATWINDOW_H
