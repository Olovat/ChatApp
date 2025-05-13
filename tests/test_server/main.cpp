#include <gtest/gtest.h>
#include "../../server/chat_logic_server.h"
#include <QCoreApplication>
#include <QDir>
#include "MockDatabase.h"
#include "MockNetworkServer.h"
#include "MockNetworkClient.h"
#include <memory> 

// Тестовый класс для сервера
class ServerTest : public ::testing::Test {
protected:
    MockDatabase* mockDbRawPtr; 
    std::shared_ptr<MockNetworkServer> mockNetworkServer;

    void SetUp() override {
        // Инициализация QCoreApplication (необходимо для работы Qt)
        int argc = 0;
        char *argv[] = {nullptr};
        app = new QCoreApplication(argc, argv);
        
        auto mockDbUniquePtr = std::make_unique<MockDatabase>();
        mockDbRawPtr = mockDbUniquePtr.get(); 
        server = new ChatLogicServer(std::move(mockDbUniquePtr));

        mockNetworkServer = std::make_shared<MockNetworkServer>();
        server->setNetworkServer(mockNetworkServer);
    }

    void TearDown() override {
        delete server;
        delete app;
    }

    QCoreApplication *app;
    ChatLogicServer *server;
};

TEST_F(ServerTest, RegisterUser) {
    auto client = std::make_shared<MockNetworkClient>();
    std::string testUsername = "testuser";
    std::string testPassword = "testpassword";
    std::string registrationMessage = "REGISTER " + testUsername + " " + testPassword;
    std::string clientId = "client1";

    EXPECT_CALL(*client, getClientId()).WillRepeatedly(::testing::Return(clientId));
    server->handleClientConnected(client);

    EXPECT_CALL(*mockDbRawPtr, fetchOne(::testing::HasSubstr("SELECT id FROM users WHERE username = ?"), ::testing::_))
        .WillOnce(::testing::Return(std::nullopt));
    EXPECT_CALL(*mockDbRawPtr, execute(::testing::HasSubstr("INSERT INTO users"), ::testing::_))
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(*client, sendMessage("REGISTER_SUCCESS"));
    
    server->handleMessageReceived(client, registrationMessage);
    ::testing::Mock::VerifyAndClearExpectations(client.get());
    ::testing::Mock::VerifyAndClearExpectations(mockDbRawPtr);

    DbRow existingUserRow; 
    existingUserRow["id"] = 1; 
    EXPECT_CALL(*mockDbRawPtr, fetchOne(::testing::HasSubstr("SELECT id FROM users WHERE username = ?"), ::testing::_))
        .WillOnce(::testing::Return(existingUserRow));
    EXPECT_CALL(*client, sendMessage("REGISTER_FAIL_EXISTS"));

    server->handleMessageReceived(client, registrationMessage);
    ::testing::Mock::VerifyAndClearExpectations(client.get());
    ::testing::Mock::VerifyAndClearExpectations(mockDbRawPtr);

    server->handleClientDisconnected(client);
}
