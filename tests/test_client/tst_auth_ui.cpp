#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../../client/auth_window.h"
#include "../../client/reg_window.h"
#include <QApplication>
#include <QTest>

class AuthUITests : public ::testing::Test {
protected:
    int argc = 1;
    char* argv[1] = {(char*)"test_client"};
    QApplication app{argc, argv};
    auth_window* authWindow;
    reg_window* regWindow;
    
    void SetUp() override {
        authWindow = new auth_window();
        regWindow = new reg_window();
    }
    
    void TearDown() override {
        delete authWindow;
        delete regWindow;
    }
};

TEST_F(AuthUITests, AuthWindowInputHandling) {
    // Ввод пользователя
    QTest::keyClicks(authWindow->findChild<QLineEdit*>("lineEdit"), "testuser");
    ASSERT_EQ(authWindow->getLogin(), "testuser");
    
    // Ввод пароля
    QTest::keyClicks(authWindow->findChild<QLineEdit*>("lineEdit_2"), "testpass");
    ASSERT_EQ(authWindow->getPass(), "testpass");
}

TEST_F(AuthUITests, RegWindowPasswordConfirmation) {
    // Совпадение вводов в поля пароля и поля подтверждения
    QTest::keyClicks(regWindow->findChild<QLineEdit*>("Password_line"), "testpass");
    QTest::keyClicks(regWindow->findChild<QLineEdit*>("Confirm_line"), "testpass");
    ASSERT_TRUE(regWindow->checkPass());
    
    // Несовпадение вводов в поля пароля и поля подтверждения
    regWindow->findChild<QLineEdit*>("Password_line")->clear();
    regWindow->findChild<QLineEdit*>("Confirm_line")->clear();
    QTest::keyClicks(regWindow->findChild<QLineEdit*>("Password_line"), "testpass");
    QTest::keyClicks(regWindow->findChild<QLineEdit*>("Confirm_line"), "different");
    ASSERT_FALSE(regWindow->checkPass());
}
