#include <gtest/gtest.h>
#include <QTemporaryDir>
#include <QSignalSpy>
#include "mainwindow.h"
#include "mockdatabase.h"

class MainWindowTest : public ::testing::Test {
protected:
    void SetUp() override {
        mainWindow = new MainWindow(MainWindow::Mode::Testing);
        mockDb = std::make_unique<MockDatabase>();
        mainWindow->setTestDatabase(std::move(mockDb));
    }

    void TearDown() override {
        delete mainWindow;
    }

    MainWindow* mainWindow;
    std::unique_ptr<MockDatabase> mockDb;
};

TEST_F(MainWindowTest, TestInitialState) {
    EXPECT_FALSE(mainWindow->isLoginSuccessful());
    EXPECT_FALSE(mainWindow->isConnected());
    EXPECT_TRUE(mainWindow->getCurrentUsername().isEmpty());
}

TEST_F(MainWindowTest, TestAuthentication) {
    // Настроим mock базу данных
    mainWindow->testDb->registerUser("testuser", "password");

    // Тестируем успешную аутентификацию
    EXPECT_TRUE(mainWindow->testDb->authenticate("testuser", "password"));

    // Тестируем неудачную аутентификацию
    EXPECT_FALSE(mainWindow->testDb->authenticate("wrong", "wrong"));
}

TEST_F(MainWindowTest, TestRegistration) {
    // 1. Проверяем, что пользователя еще нет в базе
    EXPECT_FALSE(mainWindow->testDb->authenticate("newuser", "newpass"));

    // 2. Вызываем регистрацию (должен добавить в testDb)
    mainWindow->testRegisterUser("newuser", "newpass");

    // 3. Проверяем, что пользователь добавился
    EXPECT_TRUE(mainWindow->testDb->authenticate("newuser", "newpass"));

    // 4. Проверяем повторную регистрацию (должна завершиться ошибкой)
    EXPECT_FALSE(mainWindow->testRegisterUser("newuser", "newpass"));
}

TEST_F(MainWindowTest, TestUserListUpdate) {
    mainWindow->setTestCredentials("testuser", "pass");

    QStringList users = {
        "testuser:1:U",    // Текущий пользователь (не должен учитываться)
        "user1:1:U",       // Онлайн
        "user2:0:U",       // Оффлайн
        "chat1:1:G:Group"  // Группа (не должен учитываться)
    };

    mainWindow->testUpdateUserList(users);

    auto onlineUsers = mainWindow->getOnlineUsers();
    auto allUsers = mainWindow->getDisplayedUsers();

    // Должен быть только 1 онлайн пользователь (user1)
    ASSERT_EQ(onlineUsers.size(), 1);
    EXPECT_EQ(onlineUsers.first(), "user1");

    // Должно быть 2 пользователя в общем списке (user1 и user2)
    ASSERT_EQ(allUsers.size(), 2);
    EXPECT_TRUE(allUsers.contains("user1"));
    EXPECT_TRUE(allUsers.contains("user2"));
}

TEST_F(MainWindowTest, TestPrivateChatCreation) {
    mainWindow->setTestCredentials("testuser", "pass");

    // Проверяем, что изначально чатов нет
    EXPECT_EQ(mainWindow->privateChatsCount(), 0);

    // Симулируем входящее сообщение
    mainWindow->handlePrivateMessage("sender1", "Hello");

    // Проверяем, что окно чата создано
    EXPECT_EQ(mainWindow->privateChatsCount(), 1);
    EXPECT_TRUE(mainWindow->hasPrivateChatWith("sender1"));

    // Проверяем непрочитанные сообщения
    mainWindow->handlePrivateMessage("sender1", "Message 2");
    // Можно добавить проверку количества непрочитанных
}



