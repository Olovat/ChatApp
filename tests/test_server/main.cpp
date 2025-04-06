#include <gtest/gtest.h>
#include "../../server/server.h"
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDir>

// Тестовый класс для сервера
class ServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Инициализация QCoreApplication (необходимо для работы Qt)
        int argc = 0;
        char *argv[] = {nullptr};
        app = new QCoreApplication(argc, argv);
        server = new Server();
    }

    void TearDown() override {
        // Очистка после каждого теста
        delete server;
        delete app;

        // Удаление тестовой базы данных
        QFile::remove("test_database.db");
    }

    // Очистка тестовых данных из базы
    void cleanDatabase() {
        if (server->connectDB()) {
            QSqlDatabase db = server->getDatabase();
            QSqlQuery query(db);

            // Очищаем все тестовые таблицы
            query.exec("DELETE FROM users");
            query.exec("DELETE FROM messages");
            query.exec("DELETE FROM history");
        }
    }

    QCoreApplication *app;
    Server *server;
};

// Тест подключения к базе данных
TEST_F(ServerTest, ConnectDB) {
    EXPECT_TRUE(server->connectDB());
}

// Тест регистрации пользователя
TEST_F(ServerTest, RegisterUser) {
    QString username = "testuser";
    QString password = "testpass";

    // Первая попытка регистрации должна быть успешной
    EXPECT_TRUE(server->testRegisterUser(username, password));

    // Вторая попытка с теми же данными должна вернуть false (пользователь уже существует)
    EXPECT_FALSE(server->testRegisterUser(username, password));

    // Очищаем тестовые данные
    cleanDatabase();
}

// Тест аутентификации пользователя
TEST_F(ServerTest, AuthenticateUser) {
    QString username = "testuser11";
    QString password = "testpass11";

    // Сначала регистрируем тестового пользователя
    ASSERT_TRUE(server->testRegisterUser(username, password));

    // Проверяем успешную аутентификацию
    EXPECT_TRUE(server->testAuthenticateUser(username, password));

    // Проверяем неправильный пароль
    EXPECT_FALSE(server->testAuthenticateUser(username, "wrongpass"));

    // Проверяем несуществующего пользователя
    EXPECT_FALSE(server->testAuthenticateUser("nonexistentuser", password));

    // Очищаем тестовые данные
    cleanDatabase();
}

// Тест отправки сообщения клиенту (заглушка)
TEST_F(ServerTest, SendToClient) {
    server->testSendToClient("Hello, World!");
    EXPECT_TRUE(true); // Проверяем, что функция выполняется без ошибок
}

// Тест логирования сообщений
TEST_F(ServerTest, LogMessage) {
    QString sender = "Ярик-начальник";
    QString recipient = "otheruser";
    QString message = "Hello!";

    // Логируем сообщение
    EXPECT_TRUE(server->testLogMessage(sender, recipient, message));

    // Проверяем, что сообщение сохранилось в базе
    QSqlQuery query(server->getDatabase());
    query.prepare("SELECT * FROM messages WHERE sender = :sender AND recipient = :recipient AND message = :message");
    query.bindValue(":sender", sender);
    query.bindValue(":recipient", recipient);
    query.bindValue(":message", message);

    EXPECT_TRUE(query.exec());
    EXPECT_TRUE(query.next());

    // Очищаем тестовые данные
    cleanDatabase();
}

// Тест сохранения в историю
TEST_F(ServerTest, SaveToHistory) {
    QString sender = "Ваня-хулиган";
    QString message = "Hello, World!";

    // Сохраняем сообщение в историю
    EXPECT_TRUE(server->testSaveToHistory(sender, message));

    // Проверяем, что сообщение сохранилось в базе
    QSqlQuery query(server->getDatabase());
    query.prepare("SELECT * FROM history WHERE sender = :sender AND message = :message");
    query.bindValue(":sender", sender);
    query.bindValue(":message", message);

    EXPECT_TRUE(query.exec());
    EXPECT_TRUE(query.next());

    // Очищаем тестовые данные
    cleanDatabase();
}
