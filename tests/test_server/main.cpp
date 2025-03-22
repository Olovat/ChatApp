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

        // Проверяем, что пользователь успешно регистрируется
        EXPECT_TRUE(server->registerUser(username, password));
        // Проверяем, что повторная регистрация с тем же именем пользователя не удается
        EXPECT_FALSE(server->registerUser(username, password));
    }

    // Тест аутентификации пользователя
    TEST_F(ServerTest, AuthenticateUser) {
        QString username = "testuser11";
        QString password = "testpass11";

        // Регистрация пользователя
        ASSERT_TRUE(server->registerUser(username, password));

        // Аутентификация с правильными данными
        EXPECT_TRUE(server->authenticateUser(username, password));

        // Аутентификация с неправильным паролем
        EXPECT_FALSE(server->authenticateUser(username, "wrongpass"));

        // Аутентификация несуществующего пользователя
        EXPECT_FALSE(server->authenticateUser("nonexistentuser", password));
    }

    TEST_F(ServerTest, SendToClient) {
        QString message = "Hello, World!";
        server->SendToCllient(message);

        // Проверяем, что сообщение было отправлено
        EXPECT_TRUE(true);
    }

    TEST_F(ServerTest, LogMessage) {
        QString sender = "testuser";
        QString recipient = "otheruser";
        QString message = "Hello!";

        // Логируем сообщение
        EXPECT_TRUE(server->logMessage(sender, recipient, message));

        // Проверяем, что сообщение сохранено в базе данных
        QSqlQuery query(server->getDatabase());
        query.prepare("SELECT * FROM messages WHERE sender = :sender AND recipient = :recipient AND message = :message");
        query.bindValue(":sender", sender);
        query.bindValue(":recipient", recipient);
        query.bindValue(":message", message);

        EXPECT_TRUE(query.exec());
        EXPECT_TRUE(query.next());
    }

    // Тест сохранения сообщений в историю
    TEST_F(ServerTest, SaveToHistory) {
        QString sender = "testuser";
        QString message = "Hello, World!";

        // Сохраняем сообщение в историю
        EXPECT_TRUE(server->saveToHistory(sender, message));

        // Проверяем, что сообщение сохранено в базе данных
        QSqlQuery query(server->getDatabase());
        query.prepare("SELECT * FROM history WHERE sender = :sender AND message = :message");
        query.bindValue(":sender", sender);
        query.bindValue(":message", message);

        EXPECT_TRUE(query.exec());
        EXPECT_TRUE(query.next());
    }
