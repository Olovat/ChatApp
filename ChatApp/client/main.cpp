#include "mainwindow.h"
#include <QApplication>
#include <QScreen>
#include <QStyleFactory>
//#include <gtest/gtest.h>

#ifndef TESTING
int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    
    qApp->setStyle(QStyleFactory::create("Fusion"));
    QPalette lightPalette;
    lightPalette.setColor(QPalette::Window, QColor(240, 240, 240));
    lightPalette.setColor(QPalette::WindowText, Qt::black);
    lightPalette.setColor(QPalette::Base, QColor(255, 255, 255));
    lightPalette.setColor(QPalette::AlternateBase, QColor(245, 245, 245));
    lightPalette.setColor(QPalette::ToolTipBase, Qt::white);
    lightPalette.setColor(QPalette::ToolTipText, Qt::black);
    lightPalette.setColor(QPalette::Text, Qt::black);
    lightPalette.setColor(QPalette::Button, QColor(240, 240, 240));
    lightPalette.setColor(QPalette::ButtonText, Qt::black);
    lightPalette.setColor(QPalette::BrightText, Qt::red);
    lightPalette.setColor(QPalette::Link, QColor(0, 120, 215));
    lightPalette.setColor(QPalette::Highlight, QColor(0, 120, 215));
    lightPalette.setColor(QPalette::HighlightedText, Qt::white);
    qApp->setPalette(lightPalette);
      MainWindow w;
    // Do not call w.display() here - it will be called after successful authentication
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
