#include <gtest/gtest.h>
#include "chat_logic_server.h"
#include <memory>

class MockDatabase : public IDatabase {
public:
    bool connect(const std::string& dbPath) override { return true; }
    void disconnect() override {}


    DbResult fetchAll(const std::string& query) override {
        lastQuery = query;
        return {};
    }

    DbResult fetchAll(const std::string& query, const std::vector<std::any>& params) override {
        lastQuery = query;

        // Поддержка JOIN-запроса для получения друзей по имени
        if (query.find("SELECT u.username FROM users u JOIN friendships f ON u.id = f.friend_id WHERE f.user_id = ?;") != std::string::npos ||
            query.find("SELECT friend_id FROM friendships WHERE user_id = ?") != std::string::npos) {

            long long userId = std::any_cast<long long>(params[0]);
            DbResult result;

            // Найти пользователя по ID
            for (const auto& [username, userData] : users) {
                if (std::any_cast<long long>(userData.at("id")) == userId && friendships.count(username)) {
                    for (const auto& friendName : friendships[username]) {
                        result.push_back({{"username", friendName}});
                    }
                }
            }

            return result;
        }

        return {};
    }

    std::optional<DbRow> fetchOne(const std::string& query) override {
        lastQuery = query;
        return std::nullopt;
    }



    bool execute(const std::string& query) override {
        lastQuery = query;
        return true;
    }

    bool execute(const std::string& query, const std::vector<std::any>& params) override {
        lastQuery = query;

        // Регистрация пользователя
        if (query.find("INSERT INTO users") != std::string::npos) {
            std::string username = std::any_cast<std::string>(params[0]);
            if (users.count(username)) {
                LastError = "Username already exists";
                return false;
            }

            users[username] = {
                {"password", std::any_cast<std::string>(params[1])},
                {"id", static_cast<long long>(++lastUserId)}
            };
            return true;
        }

        // Добавление друга
        if (query.find("INSERT INTO friendships") != std::string::npos) {
            std::string user1 = std::any_cast<std::string>(params[0]);
            std::string user2 = std::any_cast<std::string>(params[1]);

            if (!users.count(user1) || !users.count(user2)) {
                LastError = "User not found";
                return false;
            }

            friendships[user1].insert(user2);
            friendships[user2].insert(user1);
            return true;
        }

        return true;
    }

    std::optional<DbRow> fetchOne(const std::string& query, const std::vector<std::any>& params) override {
        lastQuery = query;

        std::string username = std::any_cast<std::string>(params[0]);

        // Поддержка разных видов SELECT
        if (query.find("SELECT id, username, password FROM users") != std::string::npos ||
            query.find("SELECT password FROM users WHERE username = ?") != std::string::npos ||
            query.find("SELECT id FROM users WHERE username = ?") != std::string::npos) {

            if (users.count(username)) {
                return DbRow{
                    {"id", users[username]["id"]},
                    {"username", username},
                    {"password", users[username]["password"]}
                };
            }
        }

        return std::nullopt;
    }


    std::string lastError() const override {
        return LastError;
    }
    std::string lastQuery;
private:
    int lastUserId = 0;
    std::unordered_map<std::string, std::map<std::string, std::any>> users;
    std::unordered_map<std::string, std::set<std::string>> friendships;
    std::string LastError;
};

// Mock классы для тестирования (остаются без изменений)
class MockNetworkClient : public INetworkClient {
public:
    void sendMessage(const std::string& message) override {
        lastMessage = message;
    }

    std::string getClientId() const override {
        return "mock_client_" + std::to_string(clientId);
    }

    bool isConnected() const override { return connected; }
    void disconnectClient() override { connected = false; }

    std::string lastMessage;
    bool connected = true;
    static int clientId;
};

int MockNetworkClient::clientId = 0;

class MockNetworkServer : public INetworkServer {
public:
    bool start(int port) override { return true; }
    void stop() override {}

    void broadcastMessage(const std::string& message) override {
        lastBroadcast = message;
    }

    void setClientConnectedCallback(ClientConnectedCallback cb) override { onConnect = cb; }
    void setClientDisconnectedCallback(ClientDisconnectedCallback cb) override { onDisconnect = cb; }
    void setMessageReceivedCallback(MessageReceivedCallback cb) override { onMessage = cb; }

    std::string lastBroadcast;
    ClientConnectedCallback onConnect;
    ClientDisconnectedCallback onDisconnect;
    MessageReceivedCallback onMessage;
};

// Тестовый класс
class ChatLogicServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockServer = std::make_shared<MockNetworkServer>();
        mockClient = std::make_shared<MockNetworkClient>();
        mockDb = std::make_unique<MockDatabase>();  // Теперь MockDatabase полностью реализует IDatabase

        server = std::make_unique<ChatLogicServer>(std::move(mockDb));
        server->setNetworkServer(mockServer);
        server->initializeDatabase();
    }

    void TearDown() override {
        server->stopServer();
    }

    std::shared_ptr<MockNetworkServer> mockServer;
    std::shared_ptr<MockNetworkClient> mockClient;
    std::unique_ptr<MockDatabase> mockDb;
    std::unique_ptr<ChatLogicServer> server;
};

// Тесты
TEST_F(ChatLogicServerTest, DatabaseInitialization) {
    EXPECT_TRUE(server->TestInitUserTable());
    EXPECT_TRUE(server->TestInitMessageTable());
    EXPECT_TRUE(server->TestInitHistoryTable());
    EXPECT_TRUE(server->TestInitFriendshipsTable());
    EXPECT_TRUE(server->TestInitGroupChatTables());
    EXPECT_TRUE(server->TestInitReadMessageTable());
}

TEST_F(ChatLogicServerTest, UserRegistration) {
    // Успешная регистрация
    EXPECT_TRUE(server->TestRegisterUser("testuser", "password", mockClient));
    EXPECT_EQ(mockClient->lastMessage, "REGISTER_SUCCESS");

    // Повторная регистрация
    mockClient->lastMessage.clear();
    EXPECT_FALSE(server->TestRegisterUser("testuser", "password", mockClient));
    EXPECT_EQ(mockClient->lastMessage, "REGISTER_FAILED:Username already exists");
}

TEST_F(ChatLogicServerTest, UserAuthentication) {
    EXPECT_TRUE(server->TestRegisterUser("authuser", "authpass", mockClient));
    EXPECT_EQ(mockClient->lastMessage, "REGISTER_SUCCESS");

    mockClient->lastMessage.clear();

    EXPECT_TRUE(server->TestAuthenticateUser("authuser", "authpass", mockClient));
    EXPECT_EQ(mockClient->lastMessage, "AUTH_SUCCESS");

    mockClient->lastMessage.clear();

    EXPECT_FALSE(server->TestAuthenticateUser("authuser", "wrongpass", mockClient));
    EXPECT_EQ(mockClient->lastMessage, "AUTH_FAILED:Invalid password");
}


TEST_F(ChatLogicServerTest, MessageLogging) {
    server->TestRegisterUser("sender", "pass", mockClient);
    server->TestRegisterUser("receiver", "pass", mockClient);

    EXPECT_TRUE(server->TestLogMessage("sender", "receiver", "Hello"));
    EXPECT_TRUE(server->TestSaveToHistory("sender", "Test message"));
}


