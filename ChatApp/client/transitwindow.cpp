#include "transitwindow.h"
#include "ui_transitwindow.h"
#include "mainwindow.h"
#include "mainwindow_controller.h"
#include <QMessageBox>
#include <QUuid>
#include <QDateTime>

TransitWindow::TransitWindow(MainWindow *mainWindow, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TransitWindow),
    mainWindow(mainWindow),
    controller(nullptr)
{
    ui->setupUi(this);
    setWindowTitle("Создание группового чата");
    setModal(true);
    
    // Устанавливаем фокус на поле ввода названия чата
    ui->chatNameLineEdit->setFocus();
}

TransitWindow::TransitWindow(MainWindowController *controller, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TransitWindow),
    mainWindow(nullptr),
    controller(controller)
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
    
    // Если используется новая архитектура с контроллером
    if (controller) {
        controller->handleCreateGroupChat(chatName);
        accept();
        return;
    }
    
    // Обратная совместимость со старой архитектурой
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
