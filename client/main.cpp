#include "mainwindow.h"
#include <QApplication>
#include <QScreen>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    
    // Создаём MainWindow, но не показываем его
    MainWindow w;

    // подключение базы данных
    if(!w.connectDB())
    {
        qDebug() << "Failed to connect to database";
        return -1;
    }

    auth_window authWindow;

    // Связываем сигнал не совсем успешной авторизации с отображением MainWindow
    //надо ьудет делать проверку логина и пароля login_button_clicked
    QObject::connect(&authWindow, &auth_window::login_button_clicked, &w, &MainWindow::show);

    authWindow.show();
    
    return a.exec();
}
