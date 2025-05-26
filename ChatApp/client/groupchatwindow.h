#ifndef GROUPCHATWINDOW_H
#define GROUPCHATWINDOW_H

#include <QWidget>
#include <QDateTime>
#include <QList>
#include "groupchat_model.h"

class MainWindowController;
class GroupChatController;

namespace Ui {
class Form;
}

class GroupChatWindow : public QWidget
{
    Q_OBJECT

public:
    explicit GroupChatWindow(const QString &chatId, const QString &chatName, QWidget *parent = nullptr);
    void setController(MainWindowController *controller);
    void setGroupChatController(GroupChatController *controller);
    ~GroupChatWindow();

    // Методы для отображения сообщений
    void receiveMessage(const QString &sender, const QString &message);
    void receiveMessage(const QString &sender, const QString &message, const QString &timestamp);
    void displayMessages(const QList<GroupMessage> &messages);
    void clearChat();
    
    // Методы для работы с историей
    void beginHistoryDisplay();
    void addHistoryMessage(const QString &formattedMessage);
    void endHistoryDisplay();
    
    // Методы для работы с участниками
    void updateMembersList(const QStringList &members);
    void setCreator(const QString &creator); // Метод для установки создателя чата

signals:
    void messageSent(const QString &chatId, const QString &message); // Сигнал отправки сообщения в групповой чат
    void addUserRequested(const QString &chatId, const QString &username); // Сигнал запроса добавления пользователя
    void removeUserRequested(const QString &chatId, const QString &username); // Сигнал запроса удаления пользователя
    void deleteChatRequested(const QString &chatId); // Сигнал запроса удаления чата
    void windowClosed(const QString &chatId); // Сигнал закрытия окна
    void markAsReadRequested(const QString &chatId); // Сигнал запроса отметки сообщений как прочитанных

public slots:
    void onUnreadCountChanged(int count);
    void onMembersUpdated(const QStringList &members);
    void onCreatorChanged(const QString &creator);

private slots:
    void on_pushButton_2_clicked(); // Кнопка отправки сообщения
    void on_lineEdit_returnPressed(); // Обработка нажатия Enter в поле ввода
    void on_pushButton_clicked();  // Кнопка добавления пользователя в чат
    void on_pushButton_3_clicked(); // Кнопка удаления пользователя из чата
    void on_pushButton_4_clicked(); // Кнопка удаления чата

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void setupUi();
    void addMessageToChat(const QString &sender, const QString &content, const QString &timestamp);
    void sendCurrentMessage();
    void updateWindowTitle();
    QString convertUtcToLocalTime(const QString &utcTimestamp);

    Ui::Form *ui;
    QString chatId;
    QString chatName;
    MainWindowController *controller;
    GroupChatController *groupChatController;
    QString creator; // Имя создателя чата
    bool isCreator = false; // Флаг, указывающий, является ли текущий пользователь создателем
    bool historyDisplayed = false; // Флаги для отображения истории сообщений
    bool initialHistoryRequested = false; // Флаг, показывающий была ли запрошена начальная история

    // Вспомогательные методы
    void sendMessage(const QString &message);
};

#endif // GROUPCHATWINDOW_H
