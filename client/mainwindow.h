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
#include <QUuid>
#include <QListWidgetItem>
#include <QMap>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class PrivateChatWindow;

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

private slots:
    void authorizeUser(); // пользовательские слоты

    void registerWindowShow();

    void registerUser();

    void on_pushButton_2_clicked();

    void on_lineEdit_returnPressed();

    void prepareForRegistration(); // Чтобы исправить баг при переходе с авторизации на регистрацию и множественном вызове окна регистрации

    void updateUserList(const QStringList &users);

    void onUserSelected(QListWidgetItem *item);

private:
    Ui::MainWindow *ui;

    QTcpSocket *socket;

    QByteArray Data;

    quint16 nextBlockSize;
    //переменные для работы с окнами авторизации и регистрации
    //----------------------------------------------
    auth_window ui_Auth;

    reg_window ui_Reg;

    QString m_username;

    QString m_userpass;

    bool m_loginSuccesfull;
    //----------------------------------------------
    void SendToServer(QString str); //создаем блок для хранения

    QString lastReceivedMessage;

    QStringList recentSentMessages;

    // создаем состояния операций
    enum OperationType {
        None,
        Auth,
        Register
    };

    // очищаем буфер сокета
    void clearSocketBuffer();
    OperationType currentOperation;

    void openPrivateChat(const QString &username);

    void SendPrivateMessage(const QString &recipient, const QString &message);

    QString lastAuthResponse; // Хранит последний ответ авторизации/регистрации
    
    QMap<QString, PrivateChatWindow*> privateChatWindows; // Map для хранения открытых приватных чатов

    void processJsonMessage(const QJsonDocument &jsonDoc);
    void handlePrivateMessage(const QString &sender, const QString &recipient, const QString &message);
    QString getCurrentUsername() const;
    PrivateChatWindow* findOrCreatePrivateChatWindow(const QString &username);

public slots:
    void slotReadyRead();
};
#endif // MAINWINDOW_H
