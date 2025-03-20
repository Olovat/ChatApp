#ifndef PRIVATECHATWINDOW_H
#define PRIVATECHATWINDOW_H

#include <QWidget>

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

private slots:
    void on_sendButton_clicked();

private:
    Ui::PrivateChatWindow *ui;
    QString username;
    MainWindow *mainWindow;
    
    void sendMessage(const QString &message);
};

#endif // PRIVATECHATWINDOW_H
