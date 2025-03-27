#include "transitwindow.h"
#include "build\Desktop_Qt_6_8_2_MinGW_64_bit-Debug\ui_transitwindow.h"
#include "mainwindow.h"
#include <QMessageBox>
#include <QUuid>
#include <QDateTime>

TransitWindow::TransitWindow(MainWindow *mainWindow, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TransitWindow),
    mainWindow(mainWindow)
{
    ui->setupUi(this);
    setWindowTitle("Создание группового чата");
    setModal(true);
    
    // Устанавливаем фокус на поле ввода названия чата
    ui->chatNameLineEdit->setFocus();
}

TransitWindow::~TransitWindow()
{
    delete ui;
}

void TransitWindow::on_createButton_clicked()
{
    QString chatName = ui->chatNameLineEdit->text().trimmed();
    
    if (chatName.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Введите название чата");
        return;
    }
    
    // Отправляем запрос на создание чата через MainWindow
    if (mainWindow) {
        // Генерируем уникальный идентификатор для чата (UUID)
        QString chatId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        mainWindow->createGroupChat(chatName, chatId);
        
        // Закрываем окно после успешного создания
        accept();
    } else {
        QMessageBox::critical(this, "Ошибка", "Не удалось создать чат. Попробуйте позже.");
    }
}

void TransitWindow::on_cancelButton_clicked()
{
    reject();
}
