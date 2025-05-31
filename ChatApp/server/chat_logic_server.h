#ifndef CHAT_LOGIC_SERVER_H
#define CHAT_LOGIC_SERVER_H

#include "network_interface.h"
#include "database_interface.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <unordered_map>
#include <set>          

// Объявляно заранее чтобы избежать ошибок компиляции
class INetworkClient;

class ChatLogicServer {
public:
    ChatLogicServer(std::unique_ptr<IDatabase> db);
    ~ChatLogicServer();

    void setNetworkServer(std::shared_ptr<INetworkServer> network); 

    bool initializeDatabase(); // Инициализация базы данных
    void startServer(int port);
    void stopServer();

    void handleClientConnected(std::shared_ptr<INetworkClient> client);
    void handleClientDisconnected(std::shared_ptr<INetworkClient> client);
    void handleMessageReceived(std::shared_ptr<INetworkClient> client, const std::string& message);

    bool TestInitUserTable();
    bool TestInitMessageTable();
    bool TestInitHistoryTable();
    bool TestInitGroupChatTables();
    bool TestInitReadMessageTable();
    bool TestInitFriendshipsTable();
    bool TestLogMessage(const std::string &sender, const std::string &recipient, const std::string &message);
    std::vector<std::string> TestGetUserFriends(const std::string &username);
    bool TestSaveToHistory(const std::string &sender, const std::string &message);
    bool TestRegisterUser(const std::string& username, const std::string& password, std::shared_ptr<INetworkClient> client);
    bool TestAuthenticateUser(const std::string& username, const std::string& password, std::shared_ptr<INetworkClient> client);
    bool TestAddFriend(const std::string &username, const std::string &friendName);
    bool TestRemoveFriend(const std::string &username, const std::string &friendName);
private:
    bool authenticateUser(const std::string& username, const std::string& password, std::shared_ptr<INetworkClient> client);
    bool registerUser(const std::string& username, const std::string& password, std::shared_ptr<INetworkClient> client);
    void sendUserList(std::shared_ptr<INetworkClient> client);
    void broadcastUserList();
    bool sendPrivateMessageToUser(const std::string& recipientUsername, const std::string& message, const std::string& senderUsername);

    // Структура для кэширования информации о пользователе
    struct CachedUser {
        std::string username;
        long long userId = -1; // ID пользователя
        bool isOnline = false;
        std::shared_ptr<INetworkClient> client = nullptr; 
        std::set<std::string> friendUsernames;
        std::set<std::string> groupChatIds;
    };
    std::unordered_map<std::string, CachedUser> m_cachedUsers; 
    // Структура для кэширования информации о групповом чате
    struct CachedGroupChat {
        std::string id;
        std::string name;
        std::string creatorUsername;
        std::set<std::string> memberUsernames;
    };
    std::unordered_map<std::string, CachedGroupChat> m_cachedGroupChats; 
    void loadCachesFromDb(); // Загрузка начальных данных из БД
    void updateUserCacheOnLogin(const std::string& username, std::shared_ptr<INetworkClient> client);
    void updateUserCacheOnLogout(const std::string& username);
    void updateFriendCache(const std::string& username1, const std::string& username2, bool added);
    void updateGroupChatCache(const std::string& chatId, bool createdOrUpdated = true);
    void updateUserGroupMembershipCache(const std::string& username, const std::string& chatId, bool joined);

    // Обновление кэша друзей
    void addFriendToCache(const std::string& username1, const std::string& username2);
    void removeFriendFromCache(const std::string& username1, const std::string& username2);

    // Обновление кэша групповых чатов
    void addOrUpdateGroupChatInCache(const std::string& chatId, const std::string& chatName, const std::string& creatorUsername);
    void removeGroupChatFromCache(const std::string& chatId);
    void addUserToGroupChatInCache(const std::string& username, const std::string& chatId);
    void removeUserFromGroupChatInCache(const std::string& username, const std::string& chatId);

    // Вспомогательные функции для работы с кэшем (заменят m_authenticatedUsers)
    std::shared_ptr<INetworkClient> getClientFromCache(const std::string& username);
    std::string getUsernameFromCache(std::shared_ptr<INetworkClient> client);
    bool isUserOnlineInCache(const std::string& username);

    // Создание таблицы пользователей друзей сообщений и так далее.
    bool initUserTable();
    bool initMessageTable();
    bool initHistoryTable();
    bool initGroupChatTables();
    bool initReadMessageTable();
    bool initFriendshipsTable();

    // Методы для работы с сообщениями и историей
    bool logMessage(const std::string &sender, const std::string &recipient, const std::string &message);
    bool saveToHistory(const std::string &sender, const std::string &message);
    void sendMessageHistoryToClient(std::shared_ptr<INetworkClient> client);
    void sendPrivateMessageHistoryToClient(std::shared_ptr<INetworkClient> client, const std::string& user1, const std::string& user2);

    // Методы для работы с оффлайн сообщениями
    bool storeOfflineMessage(const std::string &sender, const std::string &recipient, const std::string &message);
    void sendStoredOfflineMessages(const std::string &username, std::shared_ptr<INetworkClient> client);

    // Методы для работы с групповыми чатами
    bool createGroupChat(const std::string &chatId, const std::string &chatName, const std::string &creator);
    bool addUserToGroupChat(const std::string &chatId, const std::string &username, bool sendJoinMessage = true);
    bool removeUserFromGroupChat(const std::string &chatId, const std::string &username);
    void sendGroupChatInfo(const std::string &chatId, std::shared_ptr<INetworkClient> client);
    bool saveGroupChatMessage(const std::string &chatId, const std::string &sender, const std::string &message);
    void sendGroupChatMessageToClients(const std::string &chatId, const std::string &sender, const std::string &message); 
    void sendGroupChatHistory(const std::string &chatId, std::shared_ptr<INetworkClient> client);
    void sendUserGroupChats(const std::string &username, std::shared_ptr<INetworkClient> client);
    void broadcastGroupChatInfo(const std::string &chatId);

    // Методы для отслеживания прочитанных сообщений
    bool updateLastReadMessage(const std::string &username, const std::string &chatPartner, int64_t messageId); 
    int64_t getLastReadMessageId(const std::string &username, const std::string &chatPartner); 
    int getUnreadMessageCount(const std::string &username, const std::string &chatPartner);
    bool markAllMessagesAsRead(const std::string &username, const std::string &chatPartner);
    void sendUnreadMessagesCount(std::shared_ptr<INetworkClient> client, const std::string &username, const std::string &chatPartner);

    // Методы для работы с друзьями и поиском пользователей
    void searchUsers(const std::string &query, std::shared_ptr<INetworkClient> client);
    bool addFriend(const std::string &username, const std::string &friendName);
    bool removeFriend(const std::string &username, const std::string &friendName);
    std::vector<std::string> getUserFriends(const std::string &username); // возможно не сработает и всё это зря.
    bool isFriend(const std::string &username, const std::string &friendName);

    std::unique_ptr<IDatabase> m_db;
    std::shared_ptr<INetworkServer> m_networkServer;
};

#endif // CHAT_LOGIC_SERVER_H
