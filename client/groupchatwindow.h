#ifndef GROUPCHATWINDOW_H
#define GROUPCHATWINDOW_H

#include <QWidget>
#include <QDateTime>
#include <QList>

class MainWindowController;

namespace Ui {
class Form;
}

class GroupChatWindow : public QWidget
{
    Q_OBJECT

public:
    explicit GroupChatWindow(const QString &chatId, const QString &chatName, QWidget *parent = nullptr);
    void setController(MainWindowController *controller);
    ~GroupChatWindow();

    void receiveMessage(const QString &sender, const QString &message);
    void receiveMessage(const QString &sender, const QString &message, const QString &timestamp);
    
    void beginHistoryDisplay();
    void addHistoryMessage(const QString &formattedMessage);
    void endHistoryDisplay();
    
    void updateMembersList(const QStringList &members);
    void setCreator(const QString &creator); // Метод для установки создателя чата

signals:
    void messageSent(const QString &chatId, const QString &message); // Сигнал отправки сообщения в групповой чат
    void addUserRequested(const QString &chatId, const QString &username); // Сигнал запроса добавления пользователя
    void removeUserRequested(const QString &chatId, const QString &username); // Сигнал запроса удаления пользователя
    void deleteChatRequested(const QString &chatId); // Сигнал запроса удаления чата

private slots:
    void on_pushButton_2_clicked(); // Кнопка отправки сообщения
    void on_lineEdit_returnPressed(); // Обработка нажатия Enter в поле ввода
    void on_pushButton_clicked();  // Кнопка добавления пользователя в чат
    void on_pushButton_3_clicked(); // Кнопка удаления пользователя из чата
    void on_pushButton_4_clicked(); // Кнопка удаления чата

private:
    Ui::Form *ui;
    QString chatId;
    QString chatName;
    MainWindowController *controller;
    QString creator; // Имя создателя чата
    bool isCreator = false; // Флаг, указывающий, является ли текущий пользователь создателем
      void sendMessage(const QString &message);
    void updateWindowTitle();
    QString convertUtcToLocalTime(const QString &utcTimestamp);
    
    // Флаги для отображения истории сообщений
    bool historyDisplayed = false;
};

#endif // GROUPCHATWINDOW_H
