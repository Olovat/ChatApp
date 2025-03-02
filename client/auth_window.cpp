#include "auth_window.h"
#include "ui_auth_window.h"

auth_window::auth_window(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::auth_window)
{
    ui->setupUi(this);
    connect(ui->Login_button, &QPushButton::clicked, this, &auth_window::login_button_clicked);
    connect(ui->Register_button, &QPushButton::clicked, this, &auth_window::register_button_clicked);
}


auth_window::~auth_window()
{
    delete ui;
}
void auth_window::on_lineEdit_textEdited(const QString &arg1)
{
    auth_window::m_username = arg1;
}


void auth_window::on_lineEdit_2_textEdited(const QString &arg1)
{
    auth_window::m_userpass = arg1;
}




void auth_window::on_Login_button_clicked() {
    emit login_button_clicked();
}

void auth_window::on_Register_button_clicked() {
    emit register_button_clicked();
}


QString auth_window::getLogin()  // геттер логина
{
    return auth_window::m_username;
}

QString auth_window::getPass()  // геттер пароля
{
    return auth_window::m_userpass;
}
