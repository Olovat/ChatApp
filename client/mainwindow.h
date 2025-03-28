#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QApplication>
#include <QMessageBox>
#include <QMainWindow>
#include <QTcpSocket>
#include <QString>
#include <QtSql/QtSql>
#include "auth_window.h"
#include "reg_window.h"
#include "transitwindow.h"
#include <QUuid>
#include <QListWidgetItem>
#include <QMap>
#include <QTimer>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class PrivateChatWindow;
class GroupChatWindow;
class TransitWindow;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void display(); // прототип пользовательской функции отображения
    void setLogin(const QString &login);
    void setPass(const QString &pass);
    QString getUsername() const;
    QString getUserpass() const;
    bool connectToServer();
    void sendMessageToServer(const QString &message); // Метод для отправки сообщения на сервер
    void handlePrivateMessage(const QString &sender, const QString &message); // Обработка приватных сообщений
    void sendPrivateMessage(const QString &recipient, const QString &message);
    void requestPrivateMessageHistory(const QString &otherUser);
    
    // Новый метод для работы с групповыми чатами
    void createGroupChat(const QString &chatName, const QString &chatId);
    void joinGroupChat(const QString &chatId);
    
    // Новый метод для установки режима добавления пользователя в групповой чат
    void startAddUserToGroupMode(const QString &chatId);
    
    // Перемещаем из private в public
    QString getCurrentUsername() const;

private slots:
    void authorizeUser(); // пользовательские слоты
    void registerUser();
    void on_pushButton_2_clicked();
    void on_lineEdit_returnPressed();
    void prepareForRegistration(); // Чтобы исправить баг при переходе с авторизации на регистрацию
    void updateUserList(const QStringList &users);
    void onUserSelected(QListWidgetItem *item);
    void handleAuthenticationTimeout();
    void handleSocketError(QAbstractSocket::SocketError socketError);

    void on_pushButton_clicked();

private:
    Ui::MainWindow *ui;
    QTcpSocket *socket;
    QByteArray Data;
    quint16 nextBlockSize;
    
    // Переменные для работы с окнами авторизации и регистрации
    auth_window ui_Auth;
    reg_window ui_Reg;
    QString m_username;
    QString m_userpass;
    bool m_loginSuccesfull;
    
    void SendToServer(QString str);
    QStringList recentSentMessages;

    // Состояния операций
    enum OperationType {
        None,
        Auth,
        Register
    };

    void clearSocketBuffer();
    OperationType currentOperation;

    PrivateChatWindow* findOrCreatePrivateChatWindow(const QString &username);
    
    QMap<QString, PrivateChatWindow*> privateChatWindows; // Map для хранения открытых приватных чатов
    QMap<QString, GroupChatWindow*> groupChatWindows; // Map для хранения открытых групповых чатов

    // Переменные для хранения истории сообщений
    QString currentPrivateHistoryRecipient;
    bool receivingPrivateHistory = false;

    void updatePrivateChatStatuses(const QMap<QString, bool> &userStatusMap);

    // Структура для хранения непрочитанных сообщений
    struct UnreadMessage {
        QString sender;
        QString message;
        QString timestamp;
    };
    QMap<QString, QList<UnreadMessage>> unreadMessages;

    QTimer *authTimeoutTimer;
    
    QString pendingGroupChatId; // ID группового чата для добавления пользователя

    public slots:
    void slotReadyRead();
};
#endif // MAINWINDOW_H
