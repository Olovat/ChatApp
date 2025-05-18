#include "auth_window.h"
#include "ui_auth_window.h"
#include <QMessageBox>

auth_window::auth_window(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::auth_window)
{
    ui->setupUi(this);
    this->setWindowTitle("Авторизируйтесь");
    
    // Маска на пароль
    ui->lineEdit_2->setEchoMode(QLineEdit::Password);
    
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

void auth_window::LineClear()
{
    ui->lineEdit->clear();
    ui->lineEdit_2->clear();
}

void auth_window::setButtonsEnabled(bool enabled)
{
    if (ui->Login_button) {
        ui->Login_button->setEnabled(enabled);
        ui->Login_button->repaint();
    } 
    
    if (ui->Register_button) {
        ui->Register_button->setEnabled(enabled);
        ui->Register_button->repaint();
    }
    // Обновление UI
    QApplication::processEvents();
}
