#ifndef AUTH_WINDOW_H
#define AUTH_WINDOW_H

#include <QDialog>
#include <QWidget>

namespace Ui {
class auth_window;
}

class auth_window : public QDialog
{
    Q_OBJECT

public:
    explicit auth_window(QWidget *parent = nullptr);
    ~auth_window();
    QString getLogin();
    QString getPass();
    void LineClear();
    void setButtonsEnabled(bool enabled);

signals:
    void login_button_clicked();
    void register_button_clicked();

private slots:
    void on_lineEdit_textEdited(const QString &arg1);
    void on_lineEdit_2_textEdited(const QString &arg1);
    void on_Login_button_clicked();
    void on_Register_button_clicked();

private:
    Ui::auth_window *ui;
    QString m_username;
    QString m_userpass;
};

#endif
