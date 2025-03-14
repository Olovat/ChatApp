#include <gtest/gtest.h>
#include "../../server/server.h"
#include <QDir>
#include <QCoreApplication>

class ServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Инициализация QApplication
        int argc = 0;
        char *argv[] = {nullptr};
        app = new QCoreApplication(argc, argv); // Изменено на QApplication
        server = new Server();
    }

    void TearDown() override {
        delete server;
        delete app;
    }

    QCoreApplication *app; // Изменено на QApplication
    Server *server;
};

// Тест подключения к базе данных
TEST_F(ServerTest, ConnectDB) {
    EXPECT_TRUE(server->connectDB());
}

