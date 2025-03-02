/********************************************************************************
** Form generated from reading UI file 'auth_window.ui'
**
** Created by: Qt User Interface Compiler version 6.8.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_AUTH_WINDOW_H
#define UI_AUTH_WINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_auth_window
{
public:
    QWidget *gridLayoutWidget;
    QGridLayout *gridLayout;
    QLabel *label_2;
    QLineEdit *lineEdit_2;
    QLabel *label;
    QPushButton *Login_button;
    QLineEdit *lineEdit;
    QLabel *label_3;
    QPushButton *Register_button;

    void setupUi(QDialog *auth_window)
    {
        if (auth_window->objectName().isEmpty())
            auth_window->setObjectName("auth_window");
        auth_window->resize(565, 245);
        gridLayoutWidget = new QWidget(auth_window);
        gridLayoutWidget->setObjectName("gridLayoutWidget");
        gridLayoutWidget->setGeometry(QRect(0, 0, 561, 241));
        gridLayout = new QGridLayout(gridLayoutWidget);
        gridLayout->setObjectName("gridLayout");
        gridLayout->setContentsMargins(0, 0, 0, 0);
        label_2 = new QLabel(gridLayoutWidget);
        label_2->setObjectName("label_2");

        gridLayout->addWidget(label_2, 1, 0, 1, 1);

        lineEdit_2 = new QLineEdit(gridLayoutWidget);
        lineEdit_2->setObjectName("lineEdit_2");

        gridLayout->addWidget(lineEdit_2, 2, 1, 1, 1);

        label = new QLabel(gridLayoutWidget);
        label->setObjectName("label");

        gridLayout->addWidget(label, 2, 0, 1, 1);

        Login_button = new QPushButton(gridLayoutWidget);
        Login_button->setObjectName("Login_button");

        gridLayout->addWidget(Login_button, 5, 1, 1, 1);

        lineEdit = new QLineEdit(gridLayoutWidget);
        lineEdit->setObjectName("lineEdit");

        gridLayout->addWidget(lineEdit, 1, 1, 1, 1);

        label_3 = new QLabel(gridLayoutWidget);
        label_3->setObjectName("label_3");

        gridLayout->addWidget(label_3, 0, 0, 1, 2, Qt::AlignmentFlag::AlignHCenter);

        Register_button = new QPushButton(gridLayoutWidget);
        Register_button->setObjectName("Register_button");

        gridLayout->addWidget(Register_button, 6, 1, 1, 1);


        retranslateUi(auth_window);

        QMetaObject::connectSlotsByName(auth_window);
    } // setupUi

    void retranslateUi(QDialog *auth_window)
    {
        auth_window->setWindowTitle(QCoreApplication::translate("auth_window", "Dialog", nullptr));
        label_2->setText(QCoreApplication::translate("auth_window", "Login", nullptr));
        label->setText(QCoreApplication::translate("auth_window", "Password", nullptr));
        Login_button->setText(QCoreApplication::translate("auth_window", "Login", nullptr));
        label_3->setText(QCoreApplication::translate("auth_window", "Authentification", nullptr));
        Register_button->setText(QCoreApplication::translate("auth_window", "Register", nullptr));
    } // retranslateUi

};

namespace Ui {
    class auth_window: public Ui_auth_window {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_AUTH_WINDOW_H
