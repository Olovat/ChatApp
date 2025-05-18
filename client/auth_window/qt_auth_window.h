#ifndef QT_AUTH_WINDOW_H
#define QT_AUTH_WINDOW_H
#include "auth_window_interface.h"
#include "ui_auth_window.h"
#include <QDialog>

namespace Ui {
class auth_window;
}

class QtAuthWindow : public QDialog, public AuthWindowInterface
{
    Q_OBJECT

public:
    explicit QtAuthWindow(QWidget *parent = nullptr);
    ~QtAuthWindow();

    std::string getLogin() const override { return m_username; }
    std::string getPass() const override { return m_userpass; }

    void show() override { QDialog::show(); }
    void hide() override { QDialog::hide(); }
    void LineClear() override;
    void setButtonsEnabled(bool enabled) override;
signals:
    void login_Button_Clicked();
    void register_Button_Clicked();

private slots:
    void on_lineEdit_textEdited(const QString &arg1);
    void on_lineEdit_2_textEdited(const QString &arg1);
    void on_Login_button_clicked();
    void on_Register_button_clicked();

private:
    Ui::auth_window *ui;
};
#endif // QT_AUTH_WINDOW_H
