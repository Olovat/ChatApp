#ifndef QT_PRIVATE_CHAT_WINDOW_H
#define QT_PRIVATE_CHAT_WINDOW_H
#include "private_chat_interface.h"
#include <QWidget>

class QtMainWindow;
namespace Ui {
class PrivateChatWindow;
}

class QtPrivateChatWindow : public QWidget, public PrivateChatInterface
{
    Q_OBJECT

public:
    explicit QtPrivateChatWindow(const QString &username, QtMainWindow *mainWindow,
                                 QWidget *parent = nullptr);
    ~QtPrivateChatWindow();

    // Реализация PrivateChatInterface
    void receiveMessage(const std::string& sender, const std::string& message) override;
    void receiveMessage(const std::string& sender, const std::string& message,
                        const std::string& timestamp) override;
    void setOfflineStatus(bool offline) override;
    void beginHistoryDisplay() override;
    void addHistoryMessage(const std::string& formattedMessage) override;
    void endHistoryDisplay() override;
    void markMessagesAsRead() override;

signals:
    void historyDisplayCompleted(const QString &username);

private slots:
    void on_sendButton_clicked();

private:
    Ui::PrivateChatWindow *ui;
    QtMainWindow *mainWindow;

    void sendMessage(const QString& message);
    void updateWindowTitle();
    QString convertUtcToLocalTime(const QString& utcTimestamp);
    void addStatusMessage();
};
#endif // QT_PRIVATE_CHAT_WINDOW_H
