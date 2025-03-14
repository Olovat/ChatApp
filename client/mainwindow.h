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


QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

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

private slots:
    void authorizeUser(); // пользовательские слоты

    void registerWindowShow();

    void registerUser();


    void on_pushButton_2_clicked();

    void on_lineEdit_returnPressed();

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
    
public slots:
    void slotReadyRead();
};
#endif // MAINWINDOW_H
