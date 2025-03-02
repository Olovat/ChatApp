#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    
    // Connect to database
    if(!w.connectDB())
    {
        qDebug() << "Failed to connect to database";
        return -1;
    }
    
    // Show authentication window instead of main window directly
    w.display();
    
    return a.exec();
}
