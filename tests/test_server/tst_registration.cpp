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

class ChatLogicServerRegistrationTests : public ::testing::Test {
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

TEST_F(ChatLogicServerRegistrationTests, RegistersNewUserSuccessfully) {
    auto mockClient = simulateClientConnect("clientReg1");
    std::string newUser = "testnewuser";
    std::string newPass = "password123";

    EXPECT_CALL(*mockDbRawPtr, userExists(newUser))
        .WillOnce(Return(false));
    EXPECT_CALL(*mockDbRawPtr, addUser(newUser, newPass))
        .WillOnce(Return(true));
    EXPECT_CALL(*mockDbRawPtr, getUserId(newUser))
        .WillOnce(Return(100L));
    EXPECT_CALL(*mockClient, sendMessage("REGISTER_SUCCESS"));

    simulateMessageReceived(mockClient, "REGISTER " + newUser + " " + newPass);
}

TEST_F(ChatLogicServerRegistrationTests, RejectsDuplicateUsername) {
    auto mockClient = simulateClientConnect("clientReg2");
    std::string existingUser = "testduplicate";
    std::string anyPass = "password123";
    std::string differentPass = "differentpass";

    EXPECT_CALL(*mockDbRawPtr, userExists(existingUser))
        .WillOnce(Return(false)); 
    EXPECT_CALL(*mockDbRawPtr, addUser(existingUser, anyPass))
        .WillOnce(Return(true));
    EXPECT_CALL(*mockDbRawPtr, getUserId(existingUser))
        .WillOnce(Return(101L));
    EXPECT_CALL(*mockClient, sendMessage("REGISTER_SUCCESS"));

    simulateMessageReceived(mockClient, "REGISTER " + existingUser + " " + anyPass);
    ::testing::Mock::VerifyAndClearExpectations(mockClient.get());
    ::testing::Mock::VerifyAndClearExpectations(mockDbRawPtr);

    EXPECT_CALL(*mockDbRawPtr, userExists(existingUser))
        .WillOnce(Return(true));
    EXPECT_CALL(*mockDbRawPtr, addUser(existingUser, _))
        .Times(0);
    EXPECT_CALL(*mockClient, sendMessage("REGISTER_FAILURE Username already exists"));

    simulateMessageReceived(mockClient, "REGISTER " + existingUser + " " + differentPass);
}
