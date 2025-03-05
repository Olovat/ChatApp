#include "mainwindow.h"
#include <QApplication>
#include <QScreen>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    
    // подключение базы данных
    if(!w.connectDB())
    {
        qDebug() << "Failed to connect to database";
        return -1;
    }
    
    // показать главное окно
    w.display();
    
    return a.exec();
}
