#include "mainwindow.h"
#include <QApplication>
#include <QScreen>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    
    w.display();
    
    return a.exec();
}
