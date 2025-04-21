#include "mainwindow.h"
#include <QApplication>
#include <QScreen>
//#include <gtest/gtest.h>

#ifndef TESTING
int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    MainWindow w;
    w.display();
    return a.exec();
}
#else
#include "../tests/test_client/MockDatabase.h"  // Добавляем для тестов
int main(int argc, char** argv) {
    QApplication a(argc, argv);  // Необходимо для Qt тестов
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif
