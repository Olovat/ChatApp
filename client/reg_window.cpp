#include "reg_window.h"
#include "ui_reg_window.h"
#include <QCloseEvent>


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

void reg_window::closeEvent(QCloseEvent *event)
{
    emit returnToAuth();
    this->hide();    
    event->ignore();
}

void reg_window::on_Login_line_2_textEdited(const QString &arg1)
{
    reg_window::m_userName = arg1;
}

void reg_window::on_Password_line_2_textEdited(const QString &arg1)
{
    reg_window::m_userPass = arg1;
}

void reg_window::setName(const QString& name) 
{
    ui->Login_line_2->setText(name);
}

void reg_window::setPass(const QString& pass) {
    ui->Password_line_2->setText(pass);
}

void reg_window::setConfirmPass(const QString& pass) {
    ui->Confirm_line_2->setText(pass);
}

void reg_window::on_Confirm_line_2_textEdited(const QString &arg1)
{
    reg_window::m_confirmation = arg1;
}

void reg_window::on_Register_button_2_clicked() { 
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
       ui->Confirm_line_2->clear();
}

// Тоже самое, что и в auth_window.cpp

void reg_window::setButtonsEnabled(bool enabled)
{
    
    if (ui->Register_button_2) {
        ui->Register_button_2->setEnabled(enabled);
        ui->Register_button_2->repaint();
    }

    // Обновление UI тоже самое, что и в auth_window.cpp
    QApplication::processEvents();
}

void reg_window::on_return_auth_clicked()
{
    //Эмитируем сигнал для возврата к окну авторизации
    emit returnToAuth();

    //Скрываем текущее окно регистрации
    this->hide();

    // Очищаем поля ввода
    ui->Login_line_2->clear();
    ui->Password_line_2->clear();
    ui->Confirm_line_2->clear();

    // Сбрасываем внутренние переменные
    m_userName.clear();
    m_userPass.clear();
    m_confirmation.clear();
}

