#include "transitwindow.h"
#include "ui_transitwindow.h"
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
      // Отправляем запрос на создание чата через контроллер
    if (mainWindow && mainWindow->getController()) {
        // Генерируем уникальный идентификатор для чата (UUID)
        QString chatId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        // Используем контроллер для создания чата
        emit mainWindow->requestCreateGroupChat(chatName);
        
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
