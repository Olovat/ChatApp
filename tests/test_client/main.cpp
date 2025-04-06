#include <gtest/gtest.h>
#include <QTemporaryDir>  // Для временных директорий в тестах
#include <QSignalSpy>     // Для проверки сигналов Qt
#include "mainwindow.h"
#include "MockDatabase.h"  // Мок-объект базы данных для тестирования

// Тестовый класс для MainWindow
class MainWindowTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Создаем главное окно в тестовом режиме
        mainWindow = new MainWindow(MainWindow::Mode::Testing);
        // Создаем mock базу данных
        mockDb = std::make_unique<MockDatabase>();
        // Устанавливаем mock базу в главное окно
        mainWindow->setTestDatabase(std::move(mockDb));
    }

    void TearDown() override {
        // Очищаем ресурсы после каждого теста
        delete mainWindow;
    }

    MainWindow* mainWindow;               // Указатель на тестируемое окно
    std::unique_ptr<MockDatabase> mockDb; // Mock базы данных
};

// Тест начального состояния главного окна
TEST_F(MainWindowTest, TestInitialState) {
    // Проверяем, что изначально пользователь не авторизован
    EXPECT_FALSE(mainWindow->isLoginSuccessful());
    // Проверяем, что нет соединения с сервером
    EXPECT_FALSE(mainWindow->isConnected());
    // Проверяем, что имя пользователя пустое
    EXPECT_TRUE(mainWindow->getCurrentUsername().isEmpty());
}

// Тест аутентификации пользователя
TEST_F(MainWindowTest, TestAuthentication) {
    // 1. Регистрируем тестового пользователя в mock базе
    mainWindow->testDb->registerUser("testuser", "password");

    // 2. Проверяем успешную аутентификацию
    EXPECT_TRUE(mainWindow->testDb->authenticate("testuser", "password"));

    // 3. Проверяем случай с неверными учетными данными
    EXPECT_FALSE(mainWindow->testDb->authenticate("wrong", "wrong"));
}

// Тест регистрации нового пользователя
TEST_F(MainWindowTest, TestRegistration) {
    // 1. Проверяем, что пользователя еще нет в базе
    EXPECT_FALSE(mainWindow->testDb->authenticate("newuser", "newpass"));

    // 2. Регистрируем нового пользователя
    mainWindow->testRegisterUser("newuser", "newpass");

    // 3. Проверяем, что пользователь успешно зарегистрирован
    EXPECT_TRUE(mainWindow->testDb->authenticate("newuser", "newpass"));

    // 4. Проверяем повторную регистрацию (должна завершиться ошибкой)
    EXPECT_FALSE(mainWindow->testRegisterUser("newuser", "newpass"));
}

// Тест обновления списка пользователей
TEST_F(MainWindowTest, TestUserListUpdate) {
    // Устанавливаем тестовые учетные данные
    mainWindow->setTestCredentials("testuser", "pass");

    // Тестовые данные пользователей в формате "имя:статус:тип"
    QStringList users = {
        "testuser:1:U",
        "user1:1:U",
        "user2:0:U",
        "chat1:1:G:Group"
    };

    // Обновляем список пользователей
    mainWindow->testUpdateUserList(users);

    // Получаем списки пользователей
    auto onlineUsers = mainWindow->getOnlineUsers();
    auto allUsers = mainWindow->getDisplayedUsers();

    // Проверяем количество онлайн пользователей (должен быть только user1)
    ASSERT_EQ(onlineUsers.size(), 1);
    EXPECT_EQ(onlineUsers.first(), "user1");

    // Проверяем общее количество отображаемых пользователей (user1 + user2)
    ASSERT_EQ(allUsers.size(), 2);
    EXPECT_TRUE(allUsers.contains("user1"));
    EXPECT_TRUE(allUsers.contains("user2"));
}

// Тест создания приватного чата
TEST_F(MainWindowTest, TestPrivateChatCreation) {
    // Устанавливаем тестовые учетные данные
    mainWindow->setTestCredentials("testuser", "pass");

    // 1. Проверяем, что изначально чатов нет
    EXPECT_EQ(mainWindow->privateChatsCount(), 0);

    // 2. Симулируем входящее сообщение от нового пользователя
    mainWindow->handlePrivateMessage("sender1", "Hello");

    // 3. Проверяем, что окно чата было создано
    EXPECT_EQ(mainWindow->privateChatsCount(), 1);
    EXPECT_TRUE(mainWindow->hasPrivateChatWith("sender1"));

    // 4. Проверяем обработку нескольких сообщений
    mainWindow->handlePrivateMessage("sender1", "Message 2");
}
