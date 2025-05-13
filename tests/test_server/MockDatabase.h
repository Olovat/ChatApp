\
#ifndef MOCK_DATABASE_H
#define MOCK_DATABASE_H

#include "gmock/gmock.h"
#include "../../server/database_interface.h"
#include <string>
#include <vector>
#include <tuple>
#include <set>

class MockDatabase : public IDatabase {
public:
    MOCK_METHOD(bool, connect, (), (override));
    MOCK_METHOD(void, disconnect, (), (override));
    MOCK_METHOD(bool, userExists, (const std::string& username), (override));
    MOCK_METHOD(bool, addUser, (const std::string& username, const std::string& password), (override));
    MOCK_METHOD(bool, verifyPassword, (const std::string& username, const std::string& password), (override));
    MOCK_METHOD(long long, getUserId, (const std::string& username), (override));
    
    MOCK_METHOD(bool, logMessage, (long long senderId, long long receiverId, const std::string& message, const std::string& timestamp), (override));
    MOCK_METHOD(std::vector<std::tuple<std::string, std::string, std::string>>, getMessageHistory, (long long userId1, long long userId2), (override));
    
    MOCK_METHOD(bool, addFriend, (long long userId1, long long userId2), (override)); 
    MOCK_METHOD(bool, removeFriend, (long long userId1, long long userId2), (override));
    MOCK_METHOD(std::vector<std::string>, getUserFriends, (const std::string& username), (override));
    MOCK_METHOD(bool, isFriend, (long long userId1, long long userId2), (override)); 

    MOCK_METHOD(std::vector<std::string>, searchUsers, (const std::string& query), (override));
    
    MOCK_METHOD(bool, createGroupChat, (const std::string& chatId, const std::string& chatName, const std::string& creatorUsername), (override));
    MOCK_METHOD(bool, addUserToGroupChatDB, (const std::string& chatId, const std::string& username), (override)); 
    MOCK_METHOD(bool, removeUserFromGroupChatDB, (const std::string& chatId, const std::string& username), (override));
    MOCK_METHOD(std::vector<std::string>, getGroupChatMembers, (const std::string& chatId), (override));
    MOCK_METHOD(std::vector<std::pair<std::string, std::string>>, getUserGroupChats, (const std::string& username), (override));
    MOCK_METHOD(bool, saveGroupChatMessage, (const std::string& chatId, const std::string& senderUsername, const std::string& message, const std::string& timestamp), (override));
    MOCK_METHOD(std::vector<std::tuple<std::string, std::string, std::string, std::string>>, getGroupChatHistory, (const std::string& chatId), (override)); 

    MOCK_METHOD(bool, storeOfflineMessage, (const std::string& senderUsername, const std::string& recipientUsername, const std::string& message, const std::string& timestamp), (override));
    MOCK_METHOD(std::vector<std::tuple<std::string, std::string, std::string, std::string>>, getOfflineMessages, (const std::string& recipientUsername), (override));
    MOCK_METHOD(bool, deleteOfflineMessages, (const std::string& recipientUsername), (override));

    MOCK_METHOD(bool, initTables, (), (override)); 

    MOCK_METHOD(std::string, getGroupChatName, (const std::string& chatId), (override));
    MOCK_METHOD(std::string, getGroupChatCreator, (const std::string& chatId), (override));
    MOCK_METHOD(std::vector<std::string>, getAllUsernames, (), (override));
    MOCK_METHOD(std::vector<std::string>, getAllGroupChatIds, (), (override));

    MOCK_METHOD(bool, updateLastReadMessageIdDB, (long long userId, long long partnerId, long long messageId), (override));
    MOCK_METHOD(long long, getLastReadMessageIdDB, (long long userId, long long partnerId), (override));
    MOCK_METHOD(int, getUnreadMessageCountDB, (long long userId, long long partnerId, long long lastReadMessageId), (override));
    MOCK_METHOD(bool, markAllMessagesAsReadDB, (long long userId, long long partnerId), (override));
};

#endif // MOCK_DATABASE_H
