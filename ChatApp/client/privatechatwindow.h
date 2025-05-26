#ifndef PRIVATECHATWINDOW_H
#define PRIVATECHATWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QList>
#include <QShowEvent>
#include <QCloseEvent>

// Предварительное объявление для структуры PrivateMessage
struct PrivateMessage;
class PrivateChatController;

namespace Ui {
class PrivateChatWindow;
}

class PrivateChatWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit PrivateChatWindow(const QString &peerUsername, QWidget *parent = nullptr);
    ~PrivateChatWindow();

    void setController(PrivateChatController *controller);
    void updateUserStatus(bool isOnline);
    void receiveMessage(const QString &sender, const QString &message, const QString &timestamp);
    void displayMessages(const QList<PrivateMessage> &messages);
    void clearChat();
    void showLoadingIndicator();
    void requestInitialHistory(); // Новый метод для запроса начальной истории

public slots:
    void onPeerStatusChanged(bool isOnline);
    void onUnreadCountChanged(int count);
    void on_sendButton_clicked();
    void on_messageEdit_returnPressed();

signals:
    void messageSent(const QString &recipient, const QString &message);
    void windowClosed(const QString &username);
    void markAsReadRequested(const QString &username);
    void shown(); // Новый сигнал, который будет вызываться при отображении окна

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void setupUi();
    void addMessageToChat(const QString &sender, const QString &content, const QString &timestamp);
    void sendCurrentMessage();
    void updateWindowTitle();

    Ui::PrivateChatWindow *ui;
    PrivateChatController *m_controller;
    QString m_peerUsername;
    QLabel *m_statusLabel;
    bool m_initialHistoryRequested = false; // Флаг, показывающий была ли запрошена начальная история
};

#endif // PRIVATECHATWINDOW_H
