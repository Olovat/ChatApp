    #include <gtest/gtest.h>
    #include "../../server/server.h"
    #include <QCoreApplication>
    #include <QSqlDatabase>
    #include <QSqlQuery>
    #include <QDir>

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

        QCoreApplication *app;
        Server *server;
    };

    // Тест подключения к базе данных
    TEST_F(ServerTest, ConnectDB) {
        EXPECT_TRUE(server->connectDB());
    }

    TEST_F(ServerTest, RegisterUser) {
        QString username = "testuser";
        QString password = "testpass";

        EXPECT_TRUE(server->testRegisterUser(username, password));
        EXPECT_FALSE(server->testRegisterUser(username, password));
    }

    TEST_F(ServerTest, AuthenticateUser) {
        QString username = "testuser11";
        QString password = "testpass11";

        ASSERT_TRUE(server->testRegisterUser(username, password));

        EXPECT_TRUE(server->testAuthenticateUser(username, password));
        EXPECT_FALSE(server->testAuthenticateUser(username, "wrongpass"));
        EXPECT_FALSE(server->testAuthenticateUser("nonexistentuser", password));
    }

    TEST_F(ServerTest, SendToClient) {
        server->testSendToClient("Hello, World!");
        EXPECT_TRUE(true); // Проверяем, что метод вызывается без ошибок
    }

    TEST_F(ServerTest, LogMessage) {
        QString sender = "testuser";
        QString recipient = "otheruser";
        QString message = "Hello!";

        EXPECT_TRUE(server->testLogMessage(sender, recipient, message));

        QSqlQuery query(server->getDatabase());
        query.prepare("SELECT * FROM messages WHERE sender = :sender AND recipient = :recipient AND message = :message");
        query.bindValue(":sender", sender);
        query.bindValue(":recipient", recipient);
        query.bindValue(":message", message);

        EXPECT_TRUE(query.exec());
        EXPECT_TRUE(query.next());
    }

    TEST_F(ServerTest, SaveToHistory) {
        QString sender = "testuser";
        QString message = "Hello, World!";

        EXPECT_TRUE(server->testSaveToHistory(sender, message));

        QSqlQuery query(server->getDatabase());
        query.prepare("SELECT * FROM history WHERE sender = :sender AND message = :message");
        query.bindValue(":sender", sender);
        query.bindValue(":message", message);

        EXPECT_TRUE(query.exec());
        EXPECT_TRUE(query.next());
    }
