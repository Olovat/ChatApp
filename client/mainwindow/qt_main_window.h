#ifndef QT_MAIN_WINDOW_H
#define QT_MAIN_WINDOW_H
#include "main_window_interface.h"
#include "./auth_window/qt_auth_window.h"
#include "./reg_window/qt_reg_window.h"
#include"./privatechat_window/qt_private_chat_window.h"
#include "./groupchat_window/qt_group_chat_window.h"
#include "./transit_window/transitwindow.h"
#include "ui_mainwindow.h"
#include <QListWidgetItem>
#include <QTcpSocket>
#include <QMainWindow>
#include <QTcpSocket>
#include <QMap>
#include <QSet>

namespace Ui {
class MainWindow;
}

class QtPrivateChatWindow;
class QtGroupChatWindow;
class TransitWindow;
class QtAuthWindow;
class QtRegWindow;

enum class Mode { Production, Testing };

class QtMainWindow : public QMainWindow, public MainWindowInterface
{
    Q_OBJECT

public:

    explicit QtMainWindow(QWidget *parent = nullptr);
    explicit QtMainWindow(Mode mode, QWidget *parent = nullptr);
    ~QtMainWindow();

    // Реализация MainWindowInterface
    void setTestCredentials(const std::string& login, const std::string& pass) override;
    std::string getUsername() const override;
    std::string getUserpass() const override;
    bool connectToServer(const std::string& host, unsigned short port) override;
    void sendMessageToServer(const std::string& message) override;
    void handlePrivateMessage(const std::string& sender, const std::string& message) override;
    void sendPrivateMessage(const std::string& recipient, const std::string& message) override;
    void requestPrivateMessageHistory(const std::string& otherUser) override;
    void createGroupChat(const std::string& chatName, const std::string& chatId) override;
    void joinGroupChat(const std::string& chatId) override;
    void startAddUserToGroupMode(const std::string& chatId) override;
    bool isLoginSuccessful() const override;
    bool isConnected() const override;
    std::vector<std::string> getOnlineUsers() const override;
    bool hasPrivateChatWith(const std::string& username) const override;
    int privateChatsCount() const override;
    std::vector<std::string> privateChatParticipants() const override;
    void requestUnreadCounts() override;
    void initializeCommon();
    void setLogin(const std::string &login);
    bool hasPrivateChatWith(const std::string &username);
    QStringList getDisplayedUsers() const;
    void setPass(const std::string &pass);
    void display();
signals:
    void authSuccess();
    void registerSuccess();
    void userListUpdated();

private slots:
    void authorizeUser();
    void registerUser();
    void on_pushButton_2_clicked();
    void on_lineEdit_returnPressed();
    void prepareForRegistration();
    void updateUserList(const QStringList &users);
    void onUserSelected(QListWidgetItem *item);
    void handleAuthenticationTimeout();
    void handleSocketError(QAbstractSocket::SocketError socketError);
    void on_pushButton_clicked();
    void onPrivateChatClosed();
    void searchUsers();
    void showSearchResults(const QStringList &users);
    void onSearchItemSelected(QListWidgetItem *item);
    void addUserToFriends(const QString &username);
    void slotReadyRead();

private:
    enum OperationType {
        None,
        Auth,
        Register
    };
    Ui::MainWindow *ui;
    QTcpSocket *socket;
    QByteArray Data;
    quint16 nextBlockSize;
    QtAuthWindow *ui_Auth;
    QtRegWindow *ui_Reg;
    OperationType currentOperation;
    QMap<QString, QtPrivateChatWindow*> privateChatWindows;
    QMap<QString, QtGroupChatWindow*> groupChatWindows;
    // Структура для хранения непрочитанных сообщений
    struct UnreadMessage {
        QString sender;
        QString message;
        QString timestamp;
    };
    QMap<QString, QList<UnreadMessage>> unreadMessages;
    QStringList searchResults;
    QDialog* searchDialog;
    QListWidget* searchListWidget;
    QString currentPrivateHistoryRecipient;
    bool receivingPrivateHistory;
    QTimer *authTimeoutTimer;
    QSet<QString> recentChatPartners;

    void SendToServer(QString str);
    void clearSocketBuffer();
    QtPrivateChatWindow* findOrCreatePrivateChatWindow(const QString &username);
    void updatePrivateChatStatuses(const QMap<QString, bool> &userStatusMap);
    void updateWindowTitle();
    bool isMessageDuplicate(const QString &chatId, const QString &timestamp, bool isGroup);
    Mode m_mode;
};
#endif // QT_MAIN_WINDOW_H
