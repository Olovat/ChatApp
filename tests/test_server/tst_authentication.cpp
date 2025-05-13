#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "MockDatabase.h"
#include "MockNetworkClient.h"
#include "MockNetworkServer.h"
#include "../../server/chat_logic_server.h"
#include <memory>
#include <vector>
#include <string>

using ::testing::_;
using ::testing::Return;
using ::testing::NiceMock;
using ::testing::SaveArg;
using ::testing::Invoke;

class ChatLogicServerAuthenticationTests : public ::testing::Test {
protected:
    NiceMock<MockDatabase>* mockDbRawPtr;
    std::unique_ptr<NiceMock<MockDatabase>> mockDbOwnerPtr;
    std::shared_ptr<NiceMock<MockNetworkServer>> mockNetworkServer;
    std::shared_ptr<ChatLogicServer> chatLogicServer;

    INetworkServer::ClientConnectedCallback clientConnectedCb;
    INetworkServer::ClientDisconnectedCallback clientDisconnectedCb;
    INetworkServer::MessageReceivedCallback messageReceivedCb;

    void SetUp() override {
        mockDbOwnerPtr = std::make_unique<NiceMock<MockDatabase>>();
        mockDbRawPtr = mockDbOwnerPtr.get();

        chatLogicServer = std::make_shared<ChatLogicServer>(std::move(mockDbOwnerPtr));

        mockNetworkServer = std::make_shared<NiceMock<MockNetworkServer>>();

        EXPECT_CALL(*mockNetworkServer, setClientConnectedCallback(_))
            .WillOnce(SaveArg<0>(&clientConnectedCb));
        EXPECT_CALL(*mockNetworkServer, setClientDisconnectedCallback(_))
            .WillOnce(SaveArg<0>(&clientDisconnectedCb));
        EXPECT_CALL(*mockNetworkServer, setMessageReceivedCallback(_))
            .WillOnce(SaveArg<0>(&messageReceivedCb));

        chatLogicServer->setNetworkServer(mockNetworkServer);

        ON_CALL(*mockDbRawPtr, initTables()).WillByDefault(Return(true));
        ON_CALL(*mockDbRawPtr, getAllUsernames()).WillByDefault(Return(std::vector<std::string>{}));
        ON_CALL(*mockDbRawPtr, getAllGroupChatIds()).WillByDefault(Return(std::vector<std::string>{}));
        ON_CALL(*mockDbRawPtr, getUserId(_)).WillByDefault(Return(1L));
        ON_CALL(*mockDbRawPtr, getUserFriends(_)).WillByDefault(Return(std::vector<std::string>{}));
        ON_CALL(*mockDbRawPtr, getUserGroupChats(_)).WillByDefault(Return(std::vector<std::pair<std::string, std::string>>{}));
        ON_CALL(*mockDbRawPtr, getOfflineMessages(_)).WillByDefault(Return(std::vector<std::tuple<std::string, std::string, std::string, std::string>>{}));

        ASSERT_TRUE(chatLogicServer->initializeDatabase());
    }

    void TearDown() override {
    }

    std::shared_ptr<NiceMock<MockNetworkClient>> simulateClientConnect(const std::string& clientId) {
        auto mockClient = std::make_shared<NiceMock<MockNetworkClient>>();
        ON_CALL(*mockClient, getClientId()).WillByDefault(Return(clientId));
        ON_CALL(*mockClient, isConnected()).WillByDefault(Return(true));

        if (clientConnectedCb) {
            clientConnectedCb(mockClient);
        }
        return mockClient;
    }

    void simulateMessageReceived(std::shared_ptr<INetworkClient> client, const std::string& message) {
        if (messageReceivedCb) {
            messageReceivedCb(client, message);
        }
    }
};

TEST_F(ChatLogicServerAuthenticationTests, AuthenticatesValidUserAfterRegistration) {
    auto mockClient = simulateClientConnect("client1");
    std::string testUser = "testuser";
    std::string testPass = "testpass"; 

    EXPECT_CALL(*mockDbRawPtr, userExists(testUser))
        .WillOnce(Return(false));
    EXPECT_CALL(*mockDbRawPtr, addUser(testUser, testPass))
        .WillOnce(Return(true));
    EXPECT_CALL(*mockDbRawPtr, getUserId(testUser))
        .WillOnce(Return(1L));
    EXPECT_CALL(*mockClient, sendMessage("REGISTER_SUCCESS")); 

    simulateMessageReceived(mockClient, "REGISTER " + testUser + " " + testPass);
    ::testing::Mock::VerifyAndClearExpectations(mockClient.get());
    ::testing::Mock::VerifyAndClearExpectations(mockDbRawPtr);

    EXPECT_CALL(*mockDbRawPtr, userExists(testUser))
        .WillOnce(Return(true));
    EXPECT_CALL(*mockDbRawPtr, verifyPassword(testUser, testPass))
        .WillOnce(Return(true));
    EXPECT_CALL(*mockDbRawPtr, getUserId(testUser))
        .WillOnce(Return(1L));
    EXPECT_CALL(*mockDbRawPtr, getUserFriends(testUser))
        .WillOnce(Return(std::vector<std::string>{}));
    EXPECT_CALL(*mockDbRawPtr, getUserGroupChats(testUser))
        .WillOnce(Return(std::vector<std::pair<std::string, std::string>>{}));
    EXPECT_CALL(*mockDbRawPtr, getOfflineMessages(testUser))
        .WillOnce(Return(std::vector<std::tuple<std::string, std::string, std::string, std::string>>{}));
    EXPECT_CALL(*mockClient, sendMessage("AUTH_SUCCESS " + testUser));

    simulateMessageReceived(mockClient, "AUTH " + testUser + " " + testPass);
}

TEST_F(ChatLogicServerAuthenticationTests, RejectsInvalidPassword) {
    auto mockClient = simulateClientConnect("client2");
    std::string testUser = "existinguser";
    std::string wrongPass = "wrongpass";

    ON_CALL(*mockDbRawPtr, userExists(testUser))
        .WillByDefault(Return(true));
    ON_CALL(*mockDbRawPtr, getUserId(testUser))
        .WillByDefault(Return(2L));

    EXPECT_CALL(*mockDbRawPtr, userExists(testUser))
        .WillOnce(Return(true));
    EXPECT_CALL(*mockDbRawPtr, verifyPassword(testUser, wrongPass))
        .WillOnce(Return(false));
    EXPECT_CALL(*mockClient, sendMessage("AUTH_FAILURE Invalid username or password"));

    simulateMessageReceived(mockClient, "AUTH " + testUser + " " + wrongPass);
}

TEST_F(ChatLogicServerAuthenticationTests, RejectsNonExistentUser) {
    auto mockClient = simulateClientConnect("client3");
    std::string nonExistentUser = "iamghost";
    std::string anyPass = "anypass";

    EXPECT_CALL(*mockDbRawPtr, userExists(nonExistentUser))
        .WillOnce(Return(false));
    EXPECT_CALL(*mockClient, sendMessage("AUTH_FAILURE Invalid username or password"));
    EXPECT_CALL(*mockDbRawPtr, verifyPassword(nonExistentUser, anyPass))
        .Times(0);

    simulateMessageReceived(mockClient, "AUTH " + nonExistentUser + " " + anyPass);
}
