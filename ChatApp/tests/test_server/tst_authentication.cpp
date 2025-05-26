#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../../../server/server.h"

class ServerAuthenticationTests : public ::testing::Test {
protected:
    Server server;
    
    void SetUp() override {
        //Подготовка среды
        server.connectDB();
    }
    
    void TearDown() override {
        //Очистка после тестов так как тут используются те же пользователи что и при регистрации, я думаю что пока останется в виде заглушки 
    }
};

TEST_F(ServerAuthenticationTests, AuthenticatesValidUser) {
    ASSERT_TRUE(server.registerUser("testuser", "testpass"));
    
    ASSERT_TRUE(server.authenticateUser("testuser", "testpass"));
}

TEST_F(ServerAuthenticationTests, RejectsInvalidCredentials) {
    // Если пароль неверен
    ASSERT_FALSE(server.authenticateUser("testuser", "wrongpass"));
    
    // Если пользователя не существует
    ASSERT_FALSE(server.authenticateUser("thisusernametotalydonotexist", "anypass"));
}
