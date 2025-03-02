/********************************************************************************
** Form generated from reading UI file 'reg_window.ui'
**
** Created by: Qt User Interface Compiler version 6.8.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_REG_WINDOW_H
#define UI_REG_WINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_reg_window
{
public:
    QWidget *gridLayoutWidget;
    QGridLayout *gridLayout;
    QLabel *Password;
    QPushButton *Register_button;
    QLabel *Confirm;
    QLineEdit *Login_line;
    QLineEdit *Confirm_line;
    QLabel *Login;
    QLineEdit *Password_line;
    QLabel *labe_Register;

    void setupUi(QDialog *reg_window)
    {
        if (reg_window->objectName().isEmpty())
            reg_window->setObjectName("reg_window");
        reg_window->resize(400, 300);
        reg_window->setMaximumSize(QSize(400, 300));
        gridLayoutWidget = new QWidget(reg_window);
        gridLayoutWidget->setObjectName("gridLayoutWidget");
        gridLayoutWidget->setGeometry(QRect(0, 0, 401, 301));
        gridLayout = new QGridLayout(gridLayoutWidget);
        gridLayout->setObjectName("gridLayout");
        gridLayout->setContentsMargins(0, 0, 0, 0);
        Password = new QLabel(gridLayoutWidget);
        Password->setObjectName("Password");

        gridLayout->addWidget(Password, 3, 0, 1, 1);

        Register_button = new QPushButton(gridLayoutWidget);
        Register_button->setObjectName("Register_button");

        gridLayout->addWidget(Register_button, 5, 1, 1, 1);

        Confirm = new QLabel(gridLayoutWidget);
        Confirm->setObjectName("Confirm");

        gridLayout->addWidget(Confirm, 4, 0, 1, 1);

        Login_line = new QLineEdit(gridLayoutWidget);
        Login_line->setObjectName("Login_line");

        gridLayout->addWidget(Login_line, 2, 1, 1, 1);

        Confirm_line = new QLineEdit(gridLayoutWidget);
        Confirm_line->setObjectName("Confirm_line");

        gridLayout->addWidget(Confirm_line, 4, 1, 1, 1);

        Login = new QLabel(gridLayoutWidget);
        Login->setObjectName("Login");

        gridLayout->addWidget(Login, 2, 0, 1, 1);

        Password_line = new QLineEdit(gridLayoutWidget);
        Password_line->setObjectName("Password_line");

        gridLayout->addWidget(Password_line, 3, 1, 1, 1);

        labe_Register = new QLabel(gridLayoutWidget);
        labe_Register->setObjectName("labe_Register");

        gridLayout->addWidget(labe_Register, 1, 1, 1, 1, Qt::AlignmentFlag::AlignHCenter|Qt::AlignmentFlag::AlignVCenter);


        retranslateUi(reg_window);

        QMetaObject::connectSlotsByName(reg_window);
    } // setupUi

    void retranslateUi(QDialog *reg_window)
    {
        reg_window->setWindowTitle(QCoreApplication::translate("reg_window", "Dialog", nullptr));
        Password->setText(QCoreApplication::translate("reg_window", "Password:", nullptr));
        Register_button->setText(QCoreApplication::translate("reg_window", "Register", nullptr));
        Confirm->setText(QCoreApplication::translate("reg_window", "Confirm:", nullptr));
        Login->setText(QCoreApplication::translate("reg_window", "Login:", nullptr));
        labe_Register->setText(QCoreApplication::translate("reg_window", "Register yourself", nullptr));
    } // retranslateUi

};

namespace Ui {
    class reg_window: public Ui_reg_window {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_REG_WINDOW_H
