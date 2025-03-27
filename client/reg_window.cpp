#include "reg_window.h"
#include "build\Desktop_Qt_6_8_2_MinGW_64_bit-Debug\ui_reg_window.h"


reg_window::reg_window(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::reg_window)
{
    ui->setupUi(this);
}

reg_window::~reg_window()
{
    delete ui;
}
void reg_window::on_Login_line_textEdited(const QString &arg1)
{
    reg_window::m_userName = arg1;
}

void reg_window::on_Password_line_textEdited(const QString &arg1)
{
    reg_window::m_userPass = arg1;
}

void reg_window::setName(const QString& name) {
    ui->Login_line->setText(name);
}

void reg_window::setPass(const QString& pass) {
    ui->Password_line->setText(pass);
}

void reg_window::setConfirmPass(const QString& pass) {
    ui->Confirm_line->setText(pass);
}

void reg_window::on_Confirm_line_textEdited(const QString &arg1)
{
    reg_window::m_confirmation = arg1;
}

void reg_window::on_Register_button_clicked() {
    emit register_button_clicked2();
}

QString reg_window::getName()  //геттер логина
{
    return m_userName;
}

QString reg_window::getPass() //геттер пароль
{
    return m_userPass;
}

QString reg_window::getConfirmPass() //геттер пароль
{
    return m_confirmation;
}

// чек паролей
bool reg_window::checkPass()
{
    return (m_confirmation == m_userPass);
}

void reg_window::ConfirmClear()
{
    ui->Confirm_line->clear();
}

// Тоже самое, что и в auth_window.cpp

void reg_window::setButtonsEnabled(bool enabled)
{
    
    if (ui->Register_button) {
        ui->Register_button->setEnabled(enabled);
        ui->Register_button->repaint();
    }

    // Обновление UI тоже самое, что и в auth_window.cpp
    QApplication::processEvents();
}
