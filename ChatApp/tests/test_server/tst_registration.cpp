#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../../../server/server.h"
#include <QSqlQuery>

class ServerRegistrationTests : public ::testing::Test {
protected:
    Server server;
    
    void SetUp() override {
        server.connectDB();
        // Очистка после предыдущих тестов
        QSqlQuery cleanup("DELETE FROM userlist WHERE name LIKE 'test%'");
    }
};

TEST_F(ServerRegistrationTests, RegistersNewUser) {
    // Создание нового пользователя
    ASSERT_TRUE(server.registerUser("testnewuser", "password123"));
    
    // Проверка, что пользователь добавлен в бд, посредством авторизации
    ASSERT_TRUE(server.authenticateUser("testnewuser", "password123"));
}

TEST_F(ServerRegistrationTests, RejectsDuplicateUsername) {
    // Тест на дубликат
    server.registerUser("testduplicate", "password123");
    
    // Попатка зарегистрировать пользователя с тем же именем
    ASSERT_FALSE(server.registerUser("testduplicate", "differentpass"));
}
