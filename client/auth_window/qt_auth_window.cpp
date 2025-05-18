#include "qt_auth_window.h"
#include <QMessageBox>
#include <QApplication>

QtAuthWindow::QtAuthWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::auth_window)  // Оригинальное имя
{
    ui->setupUi(this);
    this->setWindowTitle("Авторизируйтесь");
    ui->lineEdit_2->setEchoMode(QLineEdit::Password);

    // Подключаем сигналы к слотам
    connect(ui->Login_button, &QPushButton::clicked,
            this, &QtAuthWindow::on_Login_button_clicked);
    connect(ui->Register_button, &QPushButton::clicked,
            this, &QtAuthWindow::on_Register_button_clicked);
}

QtAuthWindow::~QtAuthWindow()
{
    delete ui;
}

void QtAuthWindow::LineClear()
{
    ui->lineEdit->clear();
    ui->lineEdit_2->clear();
}

void QtAuthWindow::setButtonsEnabled(bool enabled)
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

void QtAuthWindow::on_lineEdit_textEdited(const QString &arg1)
{
    m_username = arg1.toStdString();
}

void QtAuthWindow::on_lineEdit_2_textEdited(const QString &arg1)
{
    m_userpass = arg1.toStdString();
}

void QtAuthWindow::on_Login_button_clicked()
{
    login_Button_Clicked();
}

void QtAuthWindow::on_Register_button_clicked()
{
    register_Button_Clicked();
}
