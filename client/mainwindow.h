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
#include "../tests/test_client/MockDatabase.h"
#include <QUuid>
#include <QListWidgetItem>
#include <QMap>
#include <gtest/gtest.h>
#include <QTimer>
#include "ui_mainwindow.h"


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
    enum class Mode { Production, Testing };

    explicit MainWindow(QWidget *parent = nullptr);
    explicit MainWindow(Mode mode, QWidget *parent = nullptr); // Новый конструктор
    ~MainWindow();
    // Для тестового режима
    void setTestCredentials(const QString& login, const QString& pass) {
        m_username = login;
        m_userpass = pass;
    }
    QTcpSocket* getSocket() const { return socket; }
    void display(); // прототип пользовательской функции отображения
    void setLogin(const QString &login);
    void setPass(const QString &pass);
    QString getUsername() const;
    QString getUserpass() const;
    bool connectToServer(const QString& host, quint16 port);
    void sendMessageToServer(const QString &message); // Метод для отправки сообщения на сервер
    void handlePrivateMessage(const QString &sender, const QString &message); // Обработка приватных сообщений
    void sendPrivateMessage(const QString &recipient, const QString &message);
    void requestPrivateMessageHistory(const QString &otherUser);
    // Новый метод для работы с групповыми чатами
    void createGroupChat(const QString &chatName, const QString &chatId);
    void joinGroupChat(const QString &chatId);
    // Новый метод для установки режима добавления пользователя в групповой чат
    void startAddUserToGroupMode(const QString &chatId);
    void testAuthorizeUser(const QString& username, const QString& password);
    bool testRegisterUser(const QString& username, const QString& password);
    bool isLoginSuccessful() const;
    QStringList getLastSentMessages() const {
        return recentSentMessages;
    }
    bool isConnected() const;
    QString getCurrentUsername() const;

    // Для тестов
    void setTestDatabase(std::unique_ptr<MockDatabase> db) { testDb = std::move(db);  }
    QStringList getOnlineUsers() const;
    QStringList getUserList() const;
    void testUpdateUserList(const QStringList &users) { updateUserList(users); }
    bool hasPrivateChatWith(const QString &username) const;
    int privateChatsCount() const;
    QStringList privateChatParticipants() const;
    void initializeCommon();
    QStringList getDisplayedUsers() const;

signals:
    void authSuccess();
    void registerSuccess();
    void userListUpdated();;
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

public:
    Mode mode;
    std::unique_ptr<MockDatabase> testDb; // Mock база данных


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

    void updateWindowTitle() {
        if (!m_username.isEmpty()) {
            setWindowTitle("Текущий пользователь: " + m_username);
        } else {
            setWindowTitle("Тестовый режим");
        }
    }

    QMap<QString, QString> lastPrivateChatTimestamps; 
    QMap<QString, QString> lastGroupChatTimestamps;   
    
    bool isMessageDuplicate(const QString &chatId, const QString &timestamp, bool isGroup);

public slots:

    void slotReadyRead();
};
#endif // MAINWINDOW_H
