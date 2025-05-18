#include "qt_reg_window.h"
#include "ui_reg_window.h"
#include <QCloseEvent>
#include <QMessageBox>
#include <QApplication>

QtRegWindow::QtRegWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::reg_window)
{
    ui->setupUi(this);

    // Подключаем сигналы к слотам
    connect(ui->Register_button_2, &QPushButton::clicked,
            this, &QtRegWindow::on_Register_button_2_clicked);
    connect(ui->return_auth, &QPushButton::clicked,
            this, &QtRegWindow::on_return_auth_clicked);
}

QtRegWindow::~QtRegWindow()
{
    delete ui;
}

void QtRegWindow::closeEvent(QCloseEvent *event)
{
    returnToAuth();
    this->hide();
    event->ignore();
}

void QtRegWindow::on_Login_line_2_textEdited(const QString &arg1)
{
    m_userName = arg1.toStdString();
}

void QtRegWindow::on_Password_line_2_textEdited(const QString &arg1)
{
    m_userPass = arg1.toStdString();
}

void QtRegWindow::setName(const std::string& name)
{
    m_userName = name;
    ui->Login_line_2->setText(QString::fromStdString(name));
}

void QtRegWindow::setPass(const std::string& pass)
{
    m_userPass = pass;
    ui->Password_line_2->setText(QString::fromStdString(pass));
}

void QtRegWindow::on_Register_button_2_clicked()
{
    if (m_userPass.length() < 8) {
        QMessageBox::warning(this, "Ошибка", "Пароль должен содержать минимум 8 символов");
        return;
    }

    register_button_clicked2();
}

std::string QtRegWindow::getName() const
{
    return m_userName;
}

std::string QtRegWindow::getPass() const
{
    return m_userPass;
}

void QtRegWindow::setButtonsEnabled(bool enabled)
{
    if (ui->Register_button_2) {
        ui->Register_button_2->setEnabled(enabled);
        ui->Register_button_2->repaint();
    }

    QApplication::processEvents();
}

void QtRegWindow::on_return_auth_clicked()
{
    returnToAuth();

    this->hide();
    ui->Login_line_2->clear();
    ui->Password_line_2->clear();
    m_userName.clear();
    m_userPass.clear();
}
