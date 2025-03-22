#include "../../client/mainwindow.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QTcpSocket>
#include <QSignalSpy>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QListWidget>

class MockTcpSocket : public QTcpSocket {
public:
    MOCK_METHOD(bool, waitForConnected, (int msecs)); // waitForConnected является виртуальным
    MOCK_METHOD(qint64, write, (const QByteArray &data)); // write является виртуальным
    MOCK_METHOD(bool, waitForBytesWritten, (int msecs)); // waitForBytesWritten является виртуальным
    MOCK_METHOD(QByteArray, readAll, ()); // readAll является виртуальным
    MOCK_METHOD(qint64, bytesAvailable, (), (const)); // bytesAvailable является виртуальным
};

class MainWindowTest : public ::testing::Test {
protected:
    void SetUp() override {
        mainWindow = new MainWindow();
        mockSocket = new MockTcpSocket();
        mainWindow->socket = mockSocket;  // Заменяем реальный сокет на мок
    }

    void TearDown() override {
        delete mainWindow;
        delete mockSocket;
    }

    MainWindow* mainWindow;
    MockTcpSocket* mockSocket;
};

TEST_F(MainWindowTest, AuthorizeUser) {
    EXPECT_CALL(*mockSocket, waitForConnected(testing::_)).WillOnce(testing::Return(true));
    EXPECT_CALL(*mockSocket, write(testing::_)).WillOnce(testing::Return(100));
    EXPECT_CALL(*mockSocket, waitForBytesWritten(testing::_)).WillOnce(testing::Return(true));

    mainWindow->authorizeUser();
    // Здесь можно добавить проверки на изменение состояния или вызовы методов
}

TEST_F(MainWindowTest, RegisterUser) {
    EXPECT_CALL(*mockSocket, waitForConnected(testing::_)).WillOnce(testing::Return(true));
    EXPECT_CALL(*mockSocket, write(testing::_)).WillOnce(testing::Return(100));
    EXPECT_CALL(*mockSocket, waitForBytesWritten(testing::_)).WillOnce(testing::Return(true));

    mainWindow->registerUser();
    // Проверки на корректность регистрации
}

TEST_F(MainWindowTest, HandlePrivateMessageTest) {
    QString sender = "user1";
    QString message = "Hello";
    mainWindow->handlePrivateMessage(sender, message);
    // Проверки на создание окна чата и обработку сообщения
}




