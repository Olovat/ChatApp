#include "reg_window.h"
#include "ui_reg_window.h"


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

// чек паролей
bool reg_window::checkPass()
{
    return (m_confirmation == m_userPass);
}

void reg_window::ConfirmClear()
{
    ui->Confirm_line->clear();
}
