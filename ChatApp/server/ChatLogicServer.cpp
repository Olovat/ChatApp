#include "chat_logic_server.h"
#include <iostream> 
#include <optional>
#include <sstream>  
#include <algorithm> 
#include <vector>
#include <random>
#include <iomanip> 

// Вспомогательная функция для разделения строки
std::vector<std::string> splitString(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

// Вспомогательная функция для генерации случайного ID (для group chat ID)
std::string generateRandomId(size_t length = 16) {
    const std::string CHARACTERS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::random_device random_device;
    std::mt19937 generator(random_device());
    std::uniform_int_distribution<> distribution(0, CHARACTERS.length() - 1);
    std::string random_string;
    for (std::size_t i = 0; i < length; ++i) {
        random_string += CHARACTERS[distribution(generator)];
    }
    return random_string;
}


ChatLogicServer::ChatLogicServer(std::unique_ptr<IDatabase> db)
    : m_db(std::move(db)) {
    std::cout << "ChatLogicServer created." << std::endl;
}

ChatLogicServer::~ChatLogicServer() {
    std::cout << "ChatLogicServer destroyed." << std::endl;
}

void ChatLogicServer::setNetworkServer(std::shared_ptr<INetworkServer> network) {
    m_networkServer = network;
    if (m_networkServer) {
        m_networkServer->setClientConnectedCallback(
            [this](std::shared_ptr<INetworkClient> client) {
                this->handleClientConnected(client);
        });
        m_networkServer->setClientDisconnectedCallback(
            [this](std::shared_ptr<INetworkClient> client) {
                this->handleClientDisconnected(client);
        });
        m_networkServer->setMessageReceivedCallback(
            [this](std::shared_ptr<INetworkClient> client, const std::string& message) {
                this->handleMessageReceived(client, message);
        });
        std::cout << "Network server callbacks set." << std::endl;
    }
}

bool ChatLogicServer::initializeDatabase() {
    if (!m_db) {
        std::cerr << "Database is not initialized!" << std::endl;
        return false;
    }
    bool success = true;
    success &= initUserTable();
    success &= initMessageTable();
    success &= initHistoryTable(); // Общая история
    success &= initFriendshipsTable();
    success &= initGroupChatTables();
    success &= initReadMessageTable();

    if (success) {
        std::cout << "Database initialized successfully." << std::endl;
        loadCachesFromDb();
    } else {
        std::cerr << "Database initialization failed." << std::endl;
    }
    return success;
}

void ChatLogicServer::startServer(int port) {
    if (m_networkServer) {
        if (m_networkServer->start(port)) {
            std::cout << "ChatLogicServer started on port " << port << std::endl;
        } else {
            std::cerr << "ChatLogicServer failed to start on port " << port << std::endl;
        }
    } else {
        std::cerr << "Network server is not set. Cannot start server." << std::endl;
    }
}

void ChatLogicServer::stopServer() {
    if (m_networkServer) {
        m_networkServer->stop();
        std::cout << "ChatLogicServer stopped." << std::endl;
    }
}

void ChatLogicServer::handleClientConnected(std::shared_ptr<INetworkClient> client) {
    std::cout << "Client connected: " << client->getClientId() << std::endl;
}

void ChatLogicServer::handleClientDisconnected(std::shared_ptr<INetworkClient> client) {
    std::cout << "Client disconnected: " << client->getClientId() << std::endl;
    std::string username_dc = getUsernameFromCache(client);

    if (!username_dc.empty()) {
        updateUserCacheOnLogout(username_dc);
        std::cout << "User " << username_dc << " marked as offline in cache." << std::endl;
    } else {
        std::cout << "Disconnected client was not authenticated or not found in cache." << std::endl;
    }
    broadcastUserList(); 
}

void ChatLogicServer::handleMessageReceived(std::shared_ptr<INetworkClient> client, const std::string& message) {    
    std::vector<std::string> parts = splitString(message, ':');
    if (parts.empty()) {
        std::cerr << "Received empty or malformed message." << std::endl;
        return;
    }

    std::string command = parts[0];
    std::string senderUsername = getUsernameFromCache(client);

    if (command == "AUTH" && parts.size() >= 3) {
        authenticateUser(parts[1], parts[2], client);
    } else if (command == "REGISTER" && parts.size() >= 3) {
        registerUser(parts[1], parts[2], client);
    } else if (command == "GET_USERLIST" || command == "GET_USERS") {
        if (!senderUsername.empty()) {
            sendUserList(client);
        } else {
            client->sendMessage("ERROR:Authentication required to get user list.");
        }
    } else if (command == "PRIVATE" && parts.size() >= 3) {
        if (!senderUsername.empty()) {
            std::string recipientUsername = parts[1];
            std::string msgText;
            for (size_t i = 2; i < parts.size(); ++i) {
                msgText += parts[i] + (i == parts.size() - 1 ? "" : ":");
            }
            // Логируем и отправляем.
            logMessage(senderUsername, recipientUsername, msgText);
            sendPrivateMessageToUser(recipientUsername, msgText, senderUsername);
        } else {
            client->sendMessage("ERROR:Authentication required to send private messages.");
        }
    } else if (command == "MSG" && parts.size() >= 3) {
         if (!senderUsername.empty()) {
            std::string recipientUsername = parts[1];
            std::string msgText;
            for (size_t i = 2; i < parts.size(); ++i) {
                msgText += parts[i] + (i == parts.size() - 1 ? "" : ":");
            }
            logMessage(senderUsername, recipientUsername, msgText);
            sendPrivateMessageToUser(recipientUsername, msgText, senderUsername);
        } else {
            client->sendMessage("ERROR:Authentication required to send messages.");
        }
    }
    else if (command == "GET_HISTORY") {
        if (!senderUsername.empty()) {
            sendMessageHistoryToClient(client);
        }    } else if (command == "GET_PRIVATE_HISTORY" && parts.size() >= 3) {
        if (!senderUsername.empty()) {
            sendPrivateMessageHistoryToClient(client, parts[1], parts[2]);
        }
    } else if (command == "CREATE_GROUP_CHAT" && parts.size() >= 2) {
        if (!senderUsername.empty()) {
            std::string chatName = parts[1];
            std::string chatId = generateRandomId(); // Генерируем ID для чата
            if (createGroupChat(chatId, chatName, senderUsername)) {
                client->sendMessage("GROUP_CHAT_CREATED:" + chatId + ":" + chatName);
                broadcastUserList(); // Обновить списки у всех, т.к. появился новый чат
            } else {
                client->sendMessage("ERROR:Failed to create group chat");
            }
        }
    } else if (command == "JOIN_GROUP_CHAT" && parts.size() >= 2) {
        if (!senderUsername.empty()) {
            std::string chatId = parts[1];
            if (addUserToGroupChat(chatId, senderUsername)) {
                sendGroupChatInfo(chatId, client); // Отправляем инфу о чате этому клиенту
                sendGroupChatHistory(chatId, client); // И историю
            } else {
                client->sendMessage("ERROR:Failed to join group chat " + chatId);
            }
        }
    } else if (command == "GROUP_MESSAGE" && parts.size() >= 3) {
        if (!senderUsername.empty()) {
            std::string chatId = parts[1];
            std::string chatMessage;
            for (size_t i = 2; i < parts.size(); ++i) {
                chatMessage += parts[i] + (i == parts.size() - 1 ? "" : ":");
            }
            // Сохраняем в БД только один раз
            saveGroupChatMessage(chatId, senderUsername, chatMessage);
    
            // Отправляем всем участникам только один раз
            for (auto& client : m_cachedGroupChats[chatId].memberUsernames) {
                if (auto adapter = getClientFromCache(client)) {
                    adapter->sendMessage("GROUP_MESSAGE:" + chatId + ":" + senderUsername + ":" + chatMessage);
                }
            }
        }    } else if (command == "GROUP_ADD_USER" && parts.size() >= 3) {
         if (!senderUsername.empty()) { // Только аутентифицированный пользователь может добавлять
            std::string chatId = parts[1];
            std::string userToAdd = parts[2];
            if (addUserToGroupChat(chatId, userToAdd, false)) { // Отключаем автоматическое сообщение о присоединении
                 sendGroupChatMessageToClients(chatId, "SYSTEM", userToAdd + " добавлен в чат пользователем " + senderUsername);
            } else {
                client->sendMessage("ERROR:Failed to add user " + userToAdd + " to group " + chatId);
            }
        }
    } else if (command == "GROUP_REMOVE_USER" && parts.size() >= 3) {
        if (!senderUsername.empty()) {
            std::string chatId = parts[1];
            std::string userToRemove = parts[2];
            if (removeUserFromGroupChat(chatId, userToRemove)) {
                sendGroupChatMessageToClients(chatId, "SYSTEM", userToRemove + " удален из чата пользователем " + senderUsername);
            } else {
                 client->sendMessage("ERROR:Failed to remove user " + userToRemove + " from group " + chatId);
            }
        }
    } else if (command == "DELETE_GROUP_CHAT" && parts.size() >= 2) {
        if(!senderUsername.empty()){
            std::string chatId = parts[1];
            bool canDelete = false;
            if (m_cachedGroupChats.count(chatId) && m_cachedGroupChats.at(chatId).creatorUsername == senderUsername) {
                canDelete = true;
            } else {
                auto chatInfo = m_db->fetchOne("SELECT creator_username FROM group_chats WHERE id = ?;", {chatId});
                if(chatInfo && chatInfo.value().count("creator_username") && std::any_cast<std::string>(chatInfo.value().at("creator_username")) == senderUsername){
                    canDelete = true;
                }
            }

            if(canDelete){
                std::vector<std::string> membersToNotify;
                if (m_cachedGroupChats.count(chatId)) {
                    for(const auto& memberName : m_cachedGroupChats.at(chatId).memberUsernames) {
                        membersToNotify.push_back(memberName);
                    }
                } else {
                    auto membersResult = m_db->fetchAll("SELECT username FROM group_chat_members WHERE chat_id = ?;", {chatId});
                    for(const auto& row : membersResult){
                        membersToNotify.push_back(std::any_cast<std::string>(row.at("username")));
                    }
                }

                m_db->execute("DELETE FROM group_chat_messages WHERE chat_id = ?;", {chatId});
                m_db->execute("DELETE FROM group_chat_members WHERE chat_id = ?;", {chatId});
                if(m_db->execute("DELETE FROM group_chats WHERE id = ?;", {chatId})){
                    removeGroupChatFromCache(chatId); 
                    std::string notification = "GROUP_CHAT_DELETED:" + chatId;
                    for(const auto& memberName : membersToNotify){
                        auto memberClient = getClientFromCache(memberName);
                        if(memberClient && memberClient->isConnected()){
                            memberClient->sendMessage(notification);
                        }
                    }
                    broadcastUserList(); 
                } else {
                    client->sendMessage("ERROR:Failed to delete group chat from DB.");
                }
            } else {
                client->sendMessage("ERROR:Only the creator can delete the group chat or chat not found.");
            }
        }
    } else if (command == "GROUP_GET_CREATOR" && parts.size() >= 2) {
        std::string chatId = parts[1];
        auto result = m_db->fetchOne("SELECT creator_username FROM group_chats WHERE id = ?;", {chatId});
        if (result && result.value().count("creator_username")) {
            std::string creator = std::any_cast<std::string>(result.value().at("creator_username"));
            client->sendMessage("GROUP_CHAT_CREATOR:" + chatId + ":" + creator);
        } else {
            client->sendMessage("ERROR:Could not get creator for chat " + chatId);
        }
    } else if (command == "GROUP_GET_INFO" && parts.size() >= 2) {
        std::cout << "DEBUG: Processing GROUP_GET_INFO command, senderUsername: '" << senderUsername << "'" << std::endl;
        if (!senderUsername.empty()) {
            std::string chatId = parts[1];
            std::cout << "DEBUG: Sending group chat info for chatId: " << chatId << std::endl;
            sendGroupChatInfo(chatId, client);
        } else {
            std::cout << "DEBUG: Authentication required for GROUP_GET_INFO" << std::endl;
            client->sendMessage("ERROR:Authentication required to get group chat info.");
        }
    } else if (command == "GET_GROUP_HISTORY" && parts.size() >= 2) {
        std::cout << "DEBUG: Processing GET_GROUP_HISTORY command, senderUsername: '" << senderUsername << "'" << std::endl;
        if (!senderUsername.empty()) {
            std::string chatId = parts[1];
            std::cout << "DEBUG: Sending group chat history for chatId: " << chatId << std::endl;
            sendGroupChatHistory(chatId, client);
        } else {
            std::cout << "DEBUG: Authentication required for GET_GROUP_HISTORY" << std::endl;
            client->sendMessage("ERROR:Authentication required to get group chat history.");
        }
    }
    else if (command == "GET_GROUP_CHATS") {
        if (!senderUsername.empty()) {
            sendUserGroupChats(senderUsername, client);
        }
    } else if (command == "MARK_READ" && parts.size() >= 2) {
        if (!senderUsername.empty()) {
            markAllMessagesAsRead(senderUsername, parts[1]);
            sendUnreadMessagesCount(client, senderUsername, parts[1]);
        }
    } else if (command == "MARK_GROUP_READ" && parts.size() >= 2) {
        if (!senderUsername.empty()) {
            // Get the chat ID from the command parts
            std::string chatId = parts[1];
            // Mark all messages as read for this user in this group chat
            markAllMessagesAsRead(senderUsername, chatId);
            // Send updated unread count back to the client
            sendUnreadMessagesCount(client, senderUsername, chatId);
        }
    } else if (command == "GET_UNREAD_COUNT" && parts.size() >= 2) {
        if (!senderUsername.empty()) {
            sendUnreadMessagesCount(client, senderUsername, parts[1]);
        }
    }
    else if (command == "SEARCH_USERS" && parts.size() >= 2) {
        searchUsers(parts[1], client);
    } else if (command == "ADD_FRIEND" && parts.size() >= 2) {
        if (!senderUsername.empty()) {
            if (addFriend(senderUsername, parts[1])) {
                client->sendMessage("FRIEND_ADDED:" + parts[1]);
                broadcastUserList();
            } else {
                client->sendMessage("FRIEND_ADD_FAIL:" + parts[1]);
            }
        }
    } else if (command == "REMOVE_FRIEND" && parts.size() >= 2) {
        if (!senderUsername.empty()) {
            if (removeFriend(senderUsername, parts[1])) {
                client->sendMessage("FRIEND_REMOVED:" + parts[1]); 
                broadcastUserList(); 
            } else {
                client->sendMessage("FRIEND_REMOVE_FAIL:" + parts[1]);
            }
        }
    } else if (command == "GET_FRIENDS") {
        if (!senderUsername.empty()) {
            std::vector<std::string> friends = getUserFriends(senderUsername);
            std::string response = "FRIENDS_LIST";
            for(const auto& friendName : friends) {
                response += ":" + friendName;
            }
            client->sendMessage(response);
        }
    }
    else if (!senderUsername.empty()) { 
        logMessage(senderUsername, "", message);
        saveToHistory(senderUsername, message);
        if (m_networkServer) {
            m_networkServer->broadcastMessage(senderUsername + ": " + message);
        }
    }
    else {
        if (command != "AUTH" && command != "REGISTER") {
             client->sendMessage("ERROR:Authentication required for this command.");
        }
        std::cerr << "Unknown command or unauthenticated user for command: " << command << std::endl;
    }
}


bool ChatLogicServer::authenticateUser(const std::string& username, const std::string& password, std::shared_ptr<INetworkClient> client) {
    if (!m_db) return false;
    
    // Проверка пароля по БД, надоело уже.
    std::string query_str = "SELECT password FROM users WHERE username = ?;";
    auto result = m_db->fetchOne(query_str, {username});

    if (result.has_value() && !result.value().empty()) {
        std::string db_password;
        try {
            db_password = std::any_cast<std::string>(result.value().at("password"));
        } catch (const std::bad_any_cast& e) {
            std::cerr << "AuthenticateUser: Bad any_cast for password - " << e.what() << std::endl;
            client->sendMessage("AUTH_FAILED:Internal server error.");
            return false;
        }

        if (db_password == password) {
            if (m_cachedUsers.count(username)) {
                CachedUser& cachedUser = m_cachedUsers.at(username);
                if (cachedUser.isOnline && cachedUser.client) {
                     client->sendMessage("AUTH_FAIL:User already logged in.");
                     return false;
                }
                updateUserCacheOnLogin(username, client);
            } else {
                std::cerr << "User " << username << " not found in cache during authentication, but exists in DB!" << std::endl;
                auto userRow = m_db->fetchOne("SELECT id FROM users WHERE username = ?;", {username});
                 if (userRow && userRow.value().count("id")) {
                    long long userId = std::any_cast<long long>(userRow.value().at("id"));
                    m_cachedUsers[username] = {username, userId, false, nullptr, {}, {}}; // Создаем запись. Загружаем друзей и группы для этого пользователя, 
                    updateUserCacheOnLogin(username, client);
                    std::cout << "Cache emergency update: User " << username << " loaded and set online." << std::endl;
                } else {
                    client->sendMessage("AUTH_FAILED:User data inconsistency.");
                    return false;
                }
            }
            
            client->sendMessage("AUTH_SUCCESS");
            std::cout << "User " << username << " authenticated." << std::endl;
            sendStoredOfflineMessages(username, client);
            sendMessageHistoryToClient(client); 
            sendUserGroupChats(username, client); 
            broadcastUserList(); 
            return true;
        }
    }
    client->sendMessage("AUTH_FAILED");
    return false;
}

bool ChatLogicServer::registerUser(const std::string& username, const std::string& password, std::shared_ptr<INetworkClient> client) {
    if (!m_db) return false;
    std::string check_query = "SELECT id FROM users WHERE username = ?;";
    auto existing_user = m_db->fetchOne(check_query, {username});

    if (existing_user.has_value()) {
        client->sendMessage("REGISTER_FAILED:Username already exists.");
        return false;
    }

    std::string insert_query = "INSERT INTO users (username, password) VALUES (?, ?);";
    if (m_db->execute(insert_query, {username, password})) {
        client->sendMessage("REGISTER_SUCCESS");
        std::cout << "User " << username << " registered." << std::endl;
        auto userRow = m_db->fetchOne("SELECT id FROM users WHERE username = ?;", {username});
        if (userRow && userRow.value().count("id")) {
            try {
                long long userId = std::any_cast<long long>(userRow.value().at("id"));
                m_cachedUsers[username] = {username, userId, false, nullptr, {}, {}};
                std::cout << "Cache updated: New user " << username << " added to cache." << std::endl;
            } catch (const std::bad_any_cast& e) {
                std::cerr << "Error casting new user ID for cache: " << e.what() << std::endl;
            }
        } else {
             std::cerr << "Could not retrieve ID for newly registered user " << username << " for cache update." << std::endl;
        }
        return true;
    } else {
        client->sendMessage("REGISTER_FAILED:Database error.");
        std::cerr << "Failed to register user " << username << ": " << m_db->lastError() << std::endl;
        return false;
    }
}

void ChatLogicServer::sendUserList(std::shared_ptr<INetworkClient> client) {
    if (!client) return;
    std::string currentUsername = getUsernameFromCache(client);
    if (currentUsername.empty() || !m_cachedUsers.count(currentUsername)) {
        std::cerr << "sendUserList: Client not authenticated or not in cache." << std::endl;
        return; 
    }

    const CachedUser& currentUserData = m_cachedUsers.at(currentUsername);
    std::vector<std::string> userEntries;

    for (const auto& pair : m_cachedUsers) {
        const CachedUser& otherUser = pair.second;
        if (otherUser.username == currentUsername) continue; // Пропускаем самого себя

        bool is_friend = currentUserData.friendUsernames.count(otherUser.username);
        std::string status = otherUser.isOnline ? "1" : "0";
        userEntries.push_back(otherUser.username + ":" + status + ":U" + (is_friend ? ":F" : ""));
    }
    
    for (const std::string& chatId : currentUserData.groupChatIds) {
        if (m_cachedGroupChats.count(chatId)) {
            const CachedGroupChat& groupChat = m_cachedGroupChats.at(chatId);
            userEntries.push_back(groupChat.id + ":1:G:" + groupChat.name);
        }
    }

    std::string userListMsg = "USERLIST:";
    if (!userEntries.empty()) {
        for (size_t i = 0; i < userEntries.size(); ++i) {
            userListMsg += userEntries[i];
            if (i < userEntries.size() - 1) {
                userListMsg += ",";
            }
        }
    }
    client->sendMessage(userListMsg);
}

void ChatLogicServer::broadcastUserList() {
    if (!m_networkServer) return;
    
    // Логирование текущего состояния кэша
    std::cout << "Broadcasting user list. Current cache state:" << std::endl;
    for (const auto& pair : m_cachedUsers) {
        std::cout << "User: " << pair.first << " - " << (pair.second.isOnline ? "ONLINE" : "offline") << std::endl;
    }
    
    // Отправляем обновленный список всем подключенным пользователям
    for (const auto& pair : m_cachedUsers) {
        const CachedUser& user = pair.second;
        if (user.isOnline && user.client) {
            sendUserList(user.client);
        }
    }
}


std::string ChatLogicServer::getUsernameFromCache(std::shared_ptr<INetworkClient> client) {
    if (!client) return "";
    for (const auto& pair : m_cachedUsers) {
        const CachedUser& cachedUser = pair.second;
        if (cachedUser.isOnline && cachedUser.client && cachedUser.client->getClientId() == client->getClientId()) {
            return cachedUser.username;
        }
    }
    return "";
}

std::shared_ptr<INetworkClient> ChatLogicServer::getClientFromCache(const std::string& username) {
    if (m_cachedUsers.count(username)) {
        const CachedUser& cachedUser = m_cachedUsers.at(username);
        if (cachedUser.isOnline && cachedUser.client) {
            return cachedUser.client;
        }
    }
    return nullptr;
}

bool ChatLogicServer::isUserOnlineInCache(const std::string& username) {
    if (m_cachedUsers.count(username)) {
        return m_cachedUsers.at(username).isOnline;
    }
    return false;
}

bool ChatLogicServer::initUserTable() {
    if (!m_db) return false;
    std::string query = "CREATE TABLE IF NOT EXISTS users ("
                        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                        "username TEXT UNIQUE NOT NULL, "
                        "password TEXT NOT NULL);";
    bool success = m_db->execute(query);
    if (!success) std::cerr << "Failed to create users table: " << m_db->lastError() << std::endl;
    return success;
}

bool ChatLogicServer::initMessageTable() {
    if (!m_db) return false;
    std::string query = "CREATE TABLE IF NOT EXISTS messages ("
                        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                        "sender TEXT NOT NULL, "
                        "recipient TEXT NOT NULL, "
                        "message TEXT NOT NULL, "
                        "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP);";
    bool success = m_db->execute(query);
    if (!success) std::cerr << "Failed to create messages table: " << m_db->lastError() << std::endl;
    return success;
}

bool ChatLogicServer::initHistoryTable() {
    if (!m_db) return false;
    std::string query = "CREATE TABLE IF NOT EXISTS history ("
                        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                        "sender TEXT NOT NULL, "
                        "message TEXT NOT NULL, "
                        "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP);";
    bool success = m_db->execute(query);
    if (!success) std::cerr << "Failed to create history table: " << m_db->lastError() << std::endl;
    return success;
}

bool ChatLogicServer::initFriendshipsTable() {
    if (!m_db) return false;
    std::string query = "CREATE TABLE IF NOT EXISTS friendships ("
                        "user_id INTEGER NOT NULL, "
                        "friend_id INTEGER NOT NULL, "
                        "FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE, "
                        "FOREIGN KEY(friend_id) REFERENCES users(id) ON DELETE CASCADE, "
                        "PRIMARY KEY (user_id, friend_id));";
    bool success = m_db->execute(query);
    if (!success) std::cerr << "Failed to create friendships table: " << m_db->lastError() << std::endl;
    return success;
}

bool ChatLogicServer::initGroupChatTables() {
    if (!m_db) return false;
    bool success = true;
    std::string query_groups = "CREATE TABLE IF NOT EXISTS group_chats ("
                               "id TEXT PRIMARY KEY, " 
                               "name TEXT NOT NULL, "
                               "creator_username TEXT NOT NULL, "
                               "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP);";
    success &= m_db->execute(query_groups);
    if (!success) std::cerr << "Failed to create group_chats table: " << m_db->lastError() << std::endl;

    std::string query_members = "CREATE TABLE IF NOT EXISTS group_chat_members ("
                                "chat_id TEXT NOT NULL, "
                                "username TEXT NOT NULL, "
                                "FOREIGN KEY(chat_id) REFERENCES group_chats(id) ON DELETE CASCADE, "
                                "FOREIGN KEY(username) REFERENCES users(username) ON DELETE CASCADE, "
                                "PRIMARY KEY (chat_id, username));";
    success &= m_db->execute(query_members);
    if (!success) std::cerr << "Failed to create group_chat_members table: " << m_db->lastError() << std::endl;

    std::string query_messages = "CREATE TABLE IF NOT EXISTS group_chat_messages ("
                                 "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                 "chat_id TEXT NOT NULL, "
                                 "sender_username TEXT NOT NULL, "
                                 "message TEXT NOT NULL, "
                                 "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP, "
                                 "FOREIGN KEY(chat_id) REFERENCES group_chats(id) ON DELETE CASCADE, "
                                 "FOREIGN KEY(sender_username) REFERENCES users(username) ON DELETE CASCADE);";
    success &= m_db->execute(query_messages);
    if (!success) std::cerr << "Failed to create group_chat_messages table: " << m_db->lastError() << std::endl;
    return success;
}

bool ChatLogicServer::initReadMessageTable() {
     if (!m_db) return false;
    std::string query = "CREATE TABLE IF NOT EXISTS read_messages ("
                        "username TEXT NOT NULL, "
                        "chat_partner TEXT NOT NULL, " 
                        "last_read_message_id INTEGER NOT NULL, "
                        "PRIMARY KEY (username, chat_partner));";
    bool success = m_db->execute(query);
    if (!success) std::cerr << "Failed to create read_messages table: " << m_db->lastError() << std::endl;
    return success;
}

bool ChatLogicServer::logMessage(const std::string &sender, const std::string &recipient, const std::string &message) {
    if (!m_db) return false;
    std::string query = "INSERT INTO messages (sender, recipient, message) VALUES (?, ?, ?);";
    bool success = m_db->execute(query, {sender, recipient, message});
    if (!success) std::cerr << "Failed to log message: " << m_db->lastError() << std::endl;
    return success;
}

bool ChatLogicServer::saveToHistory(const std::string &sender, const std::string &message) {
    if (!m_db) return false;
    std::string query = "INSERT INTO history (sender, message) VALUES (?, ?);";
    bool success = m_db->execute(query, {sender, message});
    if (!success) std::cerr << "Failed to save to history: " << m_db->lastError() << std::endl;
    return success;
}

void ChatLogicServer::sendMessageHistoryToClient(std::shared_ptr<INetworkClient> client) {
    if (!m_db || !client) return;
    
    client->sendMessage("HISTORY_CMD:BEGIN");

    std::string query_str = "SELECT sender, message, timestamp FROM history WHERE timestamp >= datetime('now','-7 days') ORDER BY timestamp ASC;";
    auto results = m_db->fetchAll(query_str);
    
    if (results.empty()) {
        client->sendMessage("HISTORY_MSG:0000-00-00 00:00:00|Система|История сообщений пуста");
    } else {
        for (const auto& row : results) {
            try {
                std::string sender = std::any_cast<std::string>(row.at("sender"));
                std::string message = std::any_cast<std::string>(row.at("message"));
                std::string timestamp = std::any_cast<std::string>(row.at("timestamp"));
                client->sendMessage("HISTORY_MSG:" + timestamp + "|" + sender + "|" + message);
            } catch (const std::bad_any_cast& e) {
                std::cerr << "Bad any_cast in sendMessageHistoryToClient: " << e.what() << std::endl;
            }
        }
    }
    client->sendMessage("HISTORY_CMD:END");
}

void ChatLogicServer::sendPrivateMessageHistoryToClient(std::shared_ptr<INetworkClient> client, const std::string& user1, const std::string& user2) {
    if (!m_db || !client) return;

    std::string requesterUsername = getUsernameFromCache(client);
    if (requesterUsername.empty()) {
        client->sendMessage("ERROR:Authentication required to get private history.");
        return;
    }

    std::string otherUser = (requesterUsername == user1) ? user2 : user1;
    client->sendMessage("PRIVATE_HISTORY_CMD:BEGIN:" + otherUser);

    std::string query_str = "SELECT sender, recipient, message, timestamp FROM messages "
                            "WHERE ((sender = ? AND recipient = ?) OR (sender = ? AND recipient = ?)) "
                            "AND timestamp >= datetime('now','-7 days') "
                            "ORDER BY timestamp ASC;";
    auto results = m_db->fetchAll(query_str, {user1, user2, user2, user1});

    if (results.empty()) {
        client->sendMessage("PRIVATE_HISTORY_MSG:0000-00-00 00:00:00|Система|История личных сообщений пуста");
    } else {
        for (const auto& row : results) {
            try {
                std::string sender = std::any_cast<std::string>(row.at("sender"));
                std::string recipient = std::any_cast<std::string>(row.at("recipient"));
                std::string message_text = std::any_cast<std::string>(row.at("message"));
                std::string timestamp = std::any_cast<std::string>(row.at("timestamp"));
                client->sendMessage("PRIVATE_HISTORY_MSG:" + timestamp + "|" + sender + "|" + recipient + "|" + message_text);
            } catch (const std::bad_any_cast& e) {
                std::cerr << "Bad any_cast in sendPrivateMessageHistoryToClient: " << e.what() << std::endl;
            }
        }
    }
    client->sendMessage("PRIVATE_HISTORY_CMD:END");
}

bool ChatLogicServer::sendPrivateMessageToUser(const std::string& recipientUsername, const std::string& message, const std::string& senderUsername) {
    if (senderUsername == recipientUsername) {
        auto senderClient = getClientFromCache(senderUsername);
        if(senderClient) senderClient->sendMessage("ERROR:You cannot send messages to yourself this way.");
        return false;
    }

    auto recipientClient = getClientFromCache(recipientUsername); 
    if (recipientClient && recipientClient->isConnected()) {
        recipientClient->sendMessage("PRIVATE:" + senderUsername + ":" + message);
        return true;
    } else {
        storeOfflineMessage(senderUsername, recipientUsername, message);
        std::cout << "User " << recipientUsername << " is offline. Storing message." << std::endl;
        return true; 
    }
}

bool ChatLogicServer::storeOfflineMessage(const std::string &sender, const std::string &recipient, const std::string &message) {
    if (!m_db) return false;
    // В server.cpp была проверка на дубликаты в течение минуты, вроде перенёс верно
     std::string check_query_str = "SELECT COUNT(*) as count FROM messages WHERE sender = ? "
                                 "AND recipient = ? AND message = ? "
                                 "AND timestamp >= datetime('now', '-1 minute')";
    auto check_result = m_db->fetchOne(check_query_str, {sender, recipient, message});
    if (check_result && check_result.value().count("count")) {
        if (std::any_cast<long long>(check_result.value().at("count")) > 0) {
            std::cout << "Duplicate offline message detected, not storing again." << std::endl;
            return false; // Не сохраняем дубликат
        }
    }
    return true; 
}

void ChatLogicServer::sendStoredOfflineMessages(const std::string &username, std::shared_ptr<INetworkClient> client) {
    if (!m_db || !client) return;
    std::string query_str = "SELECT sender, message, timestamp FROM messages " // id не используется для отправки
                            "WHERE recipient = ? "
                            "ORDER BY timestamp ASC;";
    auto results = m_db->fetchAll(query_str, {username});
    for (const auto& row : results) {
        try {
            std::string sender = std::any_cast<std::string>(row.at("sender"));
            std::string message_text = std::any_cast<std::string>(row.at("message"));
            client->sendMessage("PRIVATE:" + sender + ":" + message_text);
        } catch (const std::bad_any_cast& e) {
            std::cerr << "Bad any_cast in sendStoredOfflineMessages: " << e.what() << std::endl;
        }
    }
}

void ChatLogicServer::searchUsers(const std::string &query, std::shared_ptr<INetworkClient> client) {
    if (!m_db || !client) return;
    std::string currentUsername = getUsernameFromCache(client);
    std::string sql = "SELECT username FROM users WHERE username LIKE ? AND username != ?;";
    auto results = m_db->fetchAll(sql, {"%" + query + "%", currentUsername});
    std::string response = "SEARCH_RESULTS";
    for (const auto& row : results) {
        try {
            response += ":" + std::any_cast<std::string>(row.at("username"));
        } catch (const std::bad_any_cast& e) {
            std::cerr << "Bad any_cast in searchUsers: " << e.what() << std::endl;
        }
    }
    client->sendMessage(response);
}

bool ChatLogicServer::addFriend(const std::string &username, const std::string &friendName) {
    if (!m_db || username == friendName) return false;

    // Проверяем, существуют ли пользователи в кэше (и получаем их ID)
    long long userId = -1, friendId = -1;
    if (m_cachedUsers.count(username)) userId = m_cachedUsers.at(username).userId;
    if (m_cachedUsers.count(friendName)) friendId = m_cachedUsers.at(friendName).userId;

    if (userId == -1 || friendId == -1) {
        // Если кого-то нет в кэше, пробуем загрузить из БД (на случай, если кэш неполный)
        auto getUserIdFromDb = [&](const std::string& name) -> std::optional<long long> {
            auto res = m_db->fetchOne("SELECT id FROM users WHERE username = ?;", {name});
            if (res && res.value().count("id")) {
                try { return std::any_cast<long long>(res.value().at("id")); }
                catch (const std::bad_any_cast&) { return std::nullopt; }
            }
            return std::nullopt;
        };
        if (userId == -1) {
            auto idOpt = getUserIdFromDb(username);
            if (!idOpt) { std::cerr << "Cannot add friend: user " << username << " not found." << std::endl; return false; }
            userId = idOpt.value();
        }
        if (friendId == -1) {
            auto idOpt = getUserIdFromDb(friendName);
            if (!idOpt) { std::cerr << "Cannot add friend: user " << friendName << " not found." << std::endl; return false; }
            friendId = idOpt.value();
        }
    }
    
    // Проверка, не являются ли уже друзьями (через кэш, если возможно, или БД)
    if (m_cachedUsers.count(username) && m_cachedUsers.at(username).friendUsernames.count(friendName)) {
        return true; // Уже друзья в кэше
    }
    // Если нет в кэше, можно проверить БД (isFriend делает это)
    if (isFriend(username, friendName)) { // isFriend обращается к БД
        // Если они друзья в БД, но не в кэше, обновляем кэш
        addFriendToCache(username, friendName);
        return true;
    }

    bool success1 = m_db->execute("INSERT OR IGNORE INTO friendships (user_id, friend_id) VALUES (?, ?);", {userId, friendId});
    bool success2 = m_db->execute("INSERT OR IGNORE INTO friendships (user_id, friend_id) VALUES (?, ?);", {friendId, userId});
      if (success1 || success2) {
        addFriendToCache(username, friendName); // Обновляем кэш
        
        // Broadcast the updated user list to all clients to ensure statuses are current
        std::cout << "Friend relation between " << username << " and " << friendName 
                  << " established. Broadcasting user list." << std::endl;
        broadcastUserList();
        
        return true;
    }
    std::cerr << "Failed to add friend relationship between " << username << " and " << friendName << " in DB: " << m_db->lastError() << std::endl;
    return false;
}

bool ChatLogicServer::removeFriend(const std::string &username, const std::string &friendName) {
    if (!m_db) return false;
    long long userId = -1, friendId = -1;
    if (m_cachedUsers.count(username)) userId = m_cachedUsers.at(username).userId;
    if (m_cachedUsers.count(friendName)) friendId = m_cachedUsers.at(friendName).userId;

     if (userId == -1 || friendId == -1) {
        auto getUserIdFromDb = [&](const std::string& name) -> std::optional<long long> { 
            auto res = m_db->fetchOne("SELECT id FROM users WHERE username = ?;", {name});
            if (res && res.value().count("id")) {
                try { return std::any_cast<long long>(res.value().at("id")); }
                catch (const std::bad_any_cast&) { return std::nullopt; }
            }
            return std::nullopt;
        };
        if (userId == -1) {
            auto idOpt = getUserIdFromDb(username);
            if (!idOpt) { std::cerr << "Cannot remove friend: user " << username << " not found." << std::endl; return false; }
            userId = idOpt.value();
        }
        if (friendId == -1) {
            auto idOpt = getUserIdFromDb(friendName);
            if (!idOpt) { std::cerr << "Cannot remove friend: user " << friendName << " not found." << std::endl; return false; }
            friendId = idOpt.value();
        }
    }

    bool success1 = m_db->execute("DELETE FROM friendships WHERE user_id = ? AND friend_id = ?;", {userId, friendId});
    bool success2 = m_db->execute("DELETE FROM friendships WHERE user_id = ? AND friend_id = ?;", {friendId, userId});
    
    if (!m_db->lastError().empty() && !success1 && !success2) {
         std::cerr << "DB error while trying to remove friend relationship between " << username << " and " << friendName << ": " << m_db->lastError() << std::endl;
         return false;
    }
    removeFriendFromCache(username, friendName); // Обновляем кэш в любом случае
    return true;
}

std::vector<std::string> ChatLogicServer::getUserFriends(const std::string &username) {
    std::vector<std::string> friends;
    if (!m_db) return friends;

    auto userIdOpt = m_db->fetchOne("SELECT id FROM users WHERE username = ?;", {username});
    if (!userIdOpt || !userIdOpt.value().count("id")) return friends;
    long long userId = std::any_cast<long long>(userIdOpt.value().at("id"));

    // Выбираем имена друзей
    std::string query = "SELECT u.username FROM users u JOIN friendships f ON u.id = f.friend_id WHERE f.user_id = ?;";
    auto results = m_db->fetchAll(query, {userId});
    for (const auto& row : results) {
        try {
            friends.push_back(std::any_cast<std::string>(row.at("username")));
        } catch (const std::bad_any_cast& e) {
            std::cerr << "Bad any_cast in getUserFriends: " << e.what() << std::endl;
        }
    }
    return friends;
}

bool ChatLogicServer::isFriend(const std::string &username, const std::string &friendName) {
    if (!m_db) return false;
    auto userIdOpt = m_db->fetchOne("SELECT id FROM users WHERE username = ?;", {username});
    auto friendIdOpt = m_db->fetchOne("SELECT id FROM users WHERE username = ?;", {friendName});

    if (!userIdOpt || !userIdOpt.value().count("id") || !friendIdOpt || !friendIdOpt.value().count("id")) return false;
    long long userId = std::any_cast<long long>(userIdOpt.value().at("id"));
    long long friendId = std::any_cast<long long>(friendIdOpt.value().at("id"));

    auto result = m_db->fetchOne("SELECT 1 FROM friendships WHERE user_id = ? AND friend_id = ?;", {userId, friendId});
    return result.has_value();
}

bool ChatLogicServer::createGroupChat(const std::string &chatId, const std::string &chatName, const std::string &creator) {
    if (!m_db) return false;
    std::string query = "INSERT INTO group_chats (id, name, creator_username) VALUES (?, ?, ?);";
    if (m_db->execute(query, {chatId, chatName, creator})) {
        addOrUpdateGroupChatInCache(chatId, chatName, creator);
        if (addUserToGroupChat(chatId, creator)) { 
            return true;
        } else {
            removeGroupChatFromCache(chatId);
            std::cerr << "Failed to add creator " << creator << " to newly created group chat " << chatId << std::endl;
            return false;
        }
    }
    std::cerr << "Failed to create group chat " << chatId << " in DB: " << m_db->lastError() << std::endl;
    return false;
}

bool ChatLogicServer::addUserToGroupChat(const std::string &chatId, const std::string &username, bool sendJoinMessage) {
    if (!m_db) return false;
    // Проверки существования чата и пользователя (как и раньше)
    if (!(m_db->fetchOne("SELECT 1 FROM group_chats WHERE id = ?;", {chatId})).has_value()){
        std::cerr << "Attempt to add user to non-existent group chat: " << chatId << std::endl;
        return false;
    }
    if (!m_cachedUsers.count(username) && !(m_db->fetchOne("SELECT 1 FROM users WHERE username = ?;", {username})).has_value()){
        std::cerr << "Attempt to add non-existent user to group chat: " << username << std::endl;
        return false;
    }
     // Проверка, не является ли уже участником (через кэш, если чат там есть)
    if (m_cachedGroupChats.count(chatId) && m_cachedGroupChats.at(chatId).memberUsernames.count(username)) {
        return true; // Уже участник в кэше
    }
    // Если нет в кэше, проверяем БД
    if ((m_db->fetchOne("SELECT 1 FROM group_chat_members WHERE chat_id = ? AND username = ?;", {chatId, username})).has_value()){
        addUserToGroupChatInCache(username, chatId); // Синхронизируем кэш
        return true; // Уже участник в БД
    }

    std::string query = "INSERT INTO group_chat_members (chat_id, username) VALUES (?, ?);";
    bool success = m_db->execute(query, {chatId, username});
    if (success) {
        addUserToGroupChatInCache(username, chatId); // Обновляем кэш
        broadcastGroupChatInfo(chatId); 
        if (sendJoinMessage) {
            sendGroupChatMessageToClients(chatId, "SYSTEM", username + " присоединился к чату.");
        }
    } else {
        std::cerr << "Failed to add user " << username << " to group " << chatId << " in DB: " << m_db->lastError() << std::endl;
    }
    return success;
}

bool ChatLogicServer::removeUserFromGroupChat(const std::string &chatId, const std::string &username) {
    if (!m_db) return false;
    std::string query = "DELETE FROM group_chat_members WHERE chat_id = ? AND username = ?;";
    bool success = m_db->execute(query, {chatId, username});
    if (success) { // Успех, даже если пользователь не был в чате (DELETE не вернет ошибку)
        removeUserFromGroupChatInCache(username, chatId); // Обновляем кэш

        auto members_left_result = m_db->fetchAll("SELECT username FROM group_chat_members WHERE chat_id = ?;", {chatId});
        if (members_left_result.empty()) {
            std::cout << "Last user left group " << chatId << ". Deleting group." << std::endl;
            m_db->execute("DELETE FROM group_chat_messages WHERE chat_id = ?;", {chatId});
            m_db->execute("DELETE FROM group_chats WHERE id = ?;", {chatId});
            removeGroupChatFromCache(chatId); // Удаляем чат из кэша
            broadcastUserList(); 
        } else {
            broadcastGroupChatInfo(chatId);
            sendGroupChatMessageToClients(chatId, "SYSTEM", username + " покинул чат.");
        }
    } else {
        std::cerr << "Failed to remove user " << username << " from group " << chatId << " in DB: " << m_db->lastError() << std::endl;
    }
    return success;
}

void ChatLogicServer::sendGroupChatInfo(const std::string &chatId, std::shared_ptr<INetworkClient> client) {
    if (!m_db || !client) return;
    auto chatInfoOpt = m_db->fetchOne("SELECT name, creator_username FROM group_chats WHERE id = ?;", {chatId});
    if (!chatInfoOpt || !chatInfoOpt.value().count("name")) { // Проверяем наличие поля name
        client->sendMessage("ERROR:Group chat " + chatId + " not found or info incomplete.");
        return;
    }
    std::string chatName = std::any_cast<std::string>(chatInfoOpt.value().at("name"));
    std::string creator = std::any_cast<std::string>(chatInfoOpt.value().at("creator_username"));

    std::string membersQuery = "SELECT username FROM group_chat_members WHERE chat_id = ?;";
    auto membersResult = m_db->fetchAll(membersQuery, {chatId});
    std::string membersStr;
    for (size_t i = 0; i < membersResult.size(); ++i) {
        membersStr += std::any_cast<std::string>(membersResult[i].at("username"));
        if (i < membersResult.size() - 1) {
            membersStr += ",";
        }
    }
    client->sendMessage("GROUP_CHAT_INFO:" + chatId + ":" + chatName + ":" + membersStr);
    client->sendMessage("GROUP_CHAT_CREATOR:" + chatId + ":" + creator);
}

void ChatLogicServer::broadcastGroupChatInfo(const std::string &chatId) {
    if (!m_db || !m_networkServer) return;
    auto chatInfoOpt = m_db->fetchOne("SELECT name, creator_username FROM group_chats WHERE id = ?;", {chatId});
    if (!chatInfoOpt || !chatInfoOpt.value().count("name")) return;
    
    std::string chatName = std::any_cast<std::string>(chatInfoOpt.value().at("name"));
    std::string creator = std::any_cast<std::string>(chatInfoOpt.value().at("creator_username"));

    std::string membersQuery = "SELECT username FROM group_chat_members WHERE chat_id = ?;";
    auto membersResult = m_db->fetchAll(membersQuery, {chatId});
    std::string membersStr;
    std::vector<std::string> memberUsernames;

    for (size_t i = 0; i < membersResult.size(); ++i) {
        std::string memberUsername = std::any_cast<std::string>(membersResult[i].at("username"));
        memberUsernames.push_back(memberUsername);
        membersStr += memberUsername;
        if (i < membersResult.size() - 1) {
            membersStr += ",";
        }
    }

    std::string infoMessage = "GROUP_CHAT_INFO:" + chatId + ":" + chatName + ":" + membersStr;
    std::string creatorMessage = "GROUP_CHAT_CREATOR:" + chatId + ":" + creator;

    for (const std::string& memberUsername : memberUsernames) {
        auto memberClient = getClientFromCache(memberUsername);
        if (memberClient && memberClient->isConnected()) {
            memberClient->sendMessage(infoMessage);
            memberClient->sendMessage(creatorMessage);
        }
    }
    broadcastUserList(); // Обновляем списки у всех, т.к. состав чата мог измениться
}

bool ChatLogicServer::saveGroupChatMessage(const std::string &chatId, const std::string &sender, const std::string &message) {
    if (!m_db) return false;
    std::string query = "INSERT INTO group_chat_messages (chat_id, sender_username, message) VALUES (?, ?, ?);";
    bool success = m_db->execute(query, {chatId, sender, message});
    if (!success) std::cerr << "Failed to save group message to " << chatId << ": " << m_db->lastError() << std::endl;
    return success;
}

void ChatLogicServer::sendGroupChatMessageToClients(const std::string &chatId, const std::string &sender, const std::string &message) {
    if (!m_db || !m_networkServer) return;
    std::string membersQuery = "SELECT username FROM group_chat_members WHERE chat_id = ?;";
    auto membersResult = m_db->fetchAll(membersQuery, {chatId});
    std::string formattedMessage = "GROUP_MESSAGE:" + chatId + ":" + sender + ":" + message;
    
    // Сохраняем сообщение в базе данных для истории и отслеживания непрочитанных
    if(!saveGroupChatMessage(chatId, sender, message)) {
        std::cerr << "Failed to save group chat message to database" << std::endl;
    }

    for (const auto& row : membersResult) { 
        std::string memberUsername = std::any_cast<std::string>(row.at("username"));
        auto memberClient = getClientFromCache(memberUsername);
        if (memberClient && memberClient->isConnected()) {
            memberClient->sendMessage(formattedMessage);
        }
    }
}

void ChatLogicServer::sendGroupChatHistory(const std::string &chatId, std::shared_ptr<INetworkClient> client) {
    if (!m_db || !client) return;
    
    client->sendMessage("GROUP_HISTORY_BEGIN:" + chatId);

    std::string query_str = "SELECT sender_username, message, timestamp FROM group_chat_messages WHERE chat_id = ? ORDER BY timestamp ASC;";
    auto results = m_db->fetchAll(query_str, {chatId});
    
    if (results.empty()) {
        client->sendMessage("GROUP_HISTORY_MSG:" + chatId + "|0000-00-00 00:00:00|SYSTEM|История группового чата пуста");
    } else {
        for (const auto& row : results) {
            try {
                std::string sender = std::any_cast<std::string>(row.at("sender_username"));
                std::string message_text = std::any_cast<std::string>(row.at("message"));
                std::string timestamp = std::any_cast<std::string>(row.at("timestamp"));
                client->sendMessage("GROUP_HISTORY_MSG:" + chatId + "|" + timestamp + "|" + sender + "|" + message_text);
            } catch (const std::bad_any_cast& e) {
                std::cerr << "Bad any_cast in sendGroupChatHistory: " << e.what() << std::endl;
            }
        }
    }
    client->sendMessage("GROUP_HISTORY_END:" + chatId);
}

void ChatLogicServer::sendUserGroupChats(const std::string &username, std::shared_ptr<INetworkClient> client) {
    if (!m_db || !client) return;
    std::string query = "SELECT gc.id, gc.name FROM group_chats gc "
                        "JOIN group_chat_members gcm ON gc.id = gcm.chat_id "
                        "WHERE gcm.username = ?;";
    auto results = m_db->fetchAll(query, {username});
    
    std::string response = "GROUP_CHATS_LIST";
    if (!results.empty()) {
         for (size_t i = 0; i < results.size(); ++i) {
            try {
                std::string chatId = std::any_cast<std::string>(results[i].at("id"));
                std::string chatName = std::any_cast<std::string>(results[i].at("name"));
                response += (i == 0 ? ":" : ",") + chatId + ":" + chatName; 
            } catch (const std::bad_any_cast& e) {
                std::cerr << "Bad any_cast in sendUserGroupChats: " << e.what() << std::endl;
            }
        }
    } else {
        response += ":";
    }
    std::vector<std::string> chatEntries;
     for (const auto& row : results) {
        try {
            std::string chatId = std::any_cast<std::string>(row.at("id"));
            std::string chatName = std::any_cast<std::string>(row.at("name"));
            chatEntries.push_back(chatId + ":" + chatName);
        } catch (const std::bad_any_cast& e) {
            std::cerr << "Bad any_cast in sendUserGroupChats: " << e.what() << std::endl;
        }
    }
    response = "GROUP_CHATS_LIST:";
    for(size_t i=0; i < chatEntries.size(); ++i){
        response += chatEntries[i];
        if(i < chatEntries.size() -1){
            response += ",";
        }
    }
    client->sendMessage(response);
}

bool ChatLogicServer::updateLastReadMessage(const std::string &username, const std::string &chatPartner, int64_t messageId) {
    if (!m_db) return false;
    std::string query = "INSERT INTO read_messages (username, chat_partner, last_read_message_id) VALUES (?, ?, ?) "
                        "ON CONFLICT(username, chat_partner) DO UPDATE SET last_read_message_id = excluded.last_read_message_id;";
    bool success = m_db->execute(query, {username, chatPartner, messageId});
    if (!success) std::cerr << "Failed to update last read message for " << username << " with " << chatPartner << ": " << m_db->lastError() << std::endl;
    return success;
}

int64_t ChatLogicServer::getLastReadMessageId(const std::string &username, const std::string &chatPartner) {
    if (!m_db) return -1;
    auto result = m_db->fetchOne("SELECT last_read_message_id FROM read_messages WHERE username = ? AND chat_partner = ?;", {username, chatPartner});
    if (result && result.value().count("last_read_message_id")) {
        try {
            return std::any_cast<long long>(result.value().at("last_read_message_id"));
        } catch (const std::bad_any_cast& e) {
             std::cerr << "Bad any_cast for last_read_message_id: " << e.what() << std::endl;
             return -1;
        }
    }
    return -1; 
}

int ChatLogicServer::getUnreadMessageCount(const std::string &username, const std::string &chatPartner) {
    if (!m_db) return 0;
    int64_t lastReadId = getLastReadMessageId(username, chatPartner);
    std::string query_str;
    DbResult results;

    bool isGroupChat = (m_db->fetchOne("SELECT 1 FROM group_chats WHERE id = ?", {chatPartner})).has_value();

    if (isGroupChat) {
        query_str = "SELECT COUNT(DISTINCT message_text || timestamp) as unread_count FROM group_chat_messages WHERE chat_id = ? AND id > ? AND sender_username != ?;";
        results = m_db->fetchAll(query_str, {chatPartner, lastReadId, username});
    } else {
        query_str = "SELECT COUNT(*) as unread_count FROM messages WHERE sender = ? AND recipient = ? AND id > ?;";
        results = m_db->fetchAll(query_str, {chatPartner, username, lastReadId});
    }
    
    if (!results.empty() && results[0].count("unread_count")) {
        try {
            return static_cast<int>(std::any_cast<long long>(results[0].at("unread_count")));
        } catch (const std::bad_any_cast& e) {
            std::cerr << "Bad any_cast for unread_count: " << e.what() << std::endl;
            return 0;
        }
    }
    return 0;
}

bool ChatLogicServer::markAllMessagesAsRead(const std::string &username, const std::string &chatPartner) {
    if (!m_db) return false;
    int64_t latestMessageId = 0;
    std::string query_str;
    std::optional<DbRow> result_row;

    bool isGroupChat = (m_db->fetchOne("SELECT 1 FROM group_chats WHERE id = ?", {chatPartner})).has_value();

    if (isGroupChat) {
        query_str = "SELECT MAX(id) as max_id FROM group_chat_messages WHERE chat_id = ?;";
        result_row = m_db->fetchOne(query_str, {chatPartner});
    } else {
        query_str = "SELECT MAX(id) as max_id FROM messages WHERE (sender = ? AND recipient = ?) OR (sender = ? AND recipient = ?);";
        result_row = m_db->fetchOne(query_str, {username, chatPartner, chatPartner, username});
    }

    if (result_row && result_row.value().count("max_id")) {
        try {
            const auto& val = result_row.value().at("max_id");
            if (val.type() == typeid(long long)) { 
                 latestMessageId = std::any_cast<long long>(val);
            } else if (val.type() == typeid(nullptr) || (val.type() == typeid(std::string) && std::any_cast<std::string>(val).empty())) { // SQLite MAX может вернуть NULL строкой
                latestMessageId = 0; 
            } else {
                latestMessageId = std::stoll(std::any_cast<std::string>(val));
            }
        } catch (const std::bad_any_cast& e) {
            std::cerr << "Bad any_cast for max_id in markAllMessagesAsRead: " << e.what() << std::endl;
            latestMessageId = 0; 
        } catch (const std::invalid_argument& ia) {
            std::cerr << "Invalid argument for stoll (max_id): " << ia.what() << std::endl;
            latestMessageId = 0;
        } catch (const std::out_of_range& oor) {
            std::cerr << "Out of range for stoll (max_id): " << oor.what() << std::endl;
            latestMessageId = 0;
        }
    }
    return updateLastReadMessage(username, chatPartner, latestMessageId);
}

void ChatLogicServer::sendUnreadMessagesCount(std::shared_ptr<INetworkClient> client, const std::string &username, const std::string &chatPartner) {
    if (!client) return;
    int count = getUnreadMessageCount(username, chatPartner);
    client->sendMessage("UNREAD_COUNT:" + chatPartner + ":" + std::to_string(count));
}

void ChatLogicServer::loadCachesFromDb() {
    if (!m_db) {
        std::cerr << "Cannot load caches: Database not available." << std::endl;
        return;
    }
    std::cout << "Loading caches from database..." << std::endl;
    m_cachedUsers.clear();
    m_cachedGroupChats.clear();

    auto usersResult = m_db->fetchAll("SELECT id, username FROM users;");
    for (const auto& row : usersResult) {
        try {
            std::string username = std::any_cast<std::string>(row.at("username"));
            long long userId = std::any_cast<long long>(row.at("id"));
            m_cachedUsers[username] = {username, userId, false, nullptr, {}, {}};
        } catch (const std::bad_any_cast& e) {
            std::cerr << "Error casting user data during cache load: " << e.what() << std::endl;
        }
    }
    std::cout << "Loaded " << m_cachedUsers.size() << " users into cache." << std::endl;

    for (auto& pair : m_cachedUsers) {
        CachedUser& cachedUser = pair.second;
        std::string friendsQuery = "SELECT u_friend.username FROM users u_friend "
                                   "JOIN friendships f ON u_friend.id = f.friend_id "
                                   "WHERE f.user_id = ?;";
        auto friendsResult = m_db->fetchAll(friendsQuery, {cachedUser.userId});
        for (const auto& row : friendsResult) {
            try {
                cachedUser.friendUsernames.insert(std::any_cast<std::string>(row.at("username")));
            } catch (const std::bad_any_cast& e) {
                std::cerr << "Error casting friend username for " << cachedUser.username << ": " << e.what() << std::endl;
            }
        }
    }
    std::cout << "Loaded friend lists into cache." << std::endl;

    auto groupChatsResult = m_db->fetchAll("SELECT id, name, creator_username FROM group_chats;");
    for (const auto& row : groupChatsResult) {
        try {
            std::string chatId = std::any_cast<std::string>(row.at("id"));
            std::string chatName = std::any_cast<std::string>(row.at("name"));
            std::string creatorUsername = std::any_cast<std::string>(row.at("creator_username"));
            m_cachedGroupChats[chatId] = {chatId, chatName, creatorUsername, {}};
        } catch (const std::bad_any_cast& e) {
            std::cerr << "Error casting group chat data during cache load: " << e.what() << std::endl;
        }
    }
    std::cout << "Loaded " << m_cachedGroupChats.size() << " group chats into cache." << std::endl;

    for (auto& pair : m_cachedGroupChats) {
        CachedGroupChat& cachedChat = pair.second;
        std::string membersQuery = "SELECT username FROM group_chat_members WHERE chat_id = ?;";
        auto membersResult = m_db->fetchAll(membersQuery, {cachedChat.id});
        for (const auto& row : membersResult) {
            try {
                std::string memberUsername = std::any_cast<std::string>(row.at("username"));
                cachedChat.memberUsernames.insert(memberUsername);
                // Также обновляем информацию у пользователя, что он состоит в этом чате
                if (m_cachedUsers.count(memberUsername)) {
                    m_cachedUsers[memberUsername].groupChatIds.insert(cachedChat.id);
                }
            } catch (const std::bad_any_cast& e) {
                std::cerr << "Error casting group chat member for chat " << cachedChat.id << ": " << e.what() << std::endl;
            }
        }
    }
    std::cout << "Loaded group chat memberships into cache." << std::endl;
    std::cout << "Cache loading complete." << std::endl;
}


void ChatLogicServer::addFriendToCache(const std::string& username1, const std::string& username2) {
    if (m_cachedUsers.count(username1)) {
        m_cachedUsers.at(username1).friendUsernames.insert(username2);
    }
    if (m_cachedUsers.count(username2)) {
        m_cachedUsers.at(username2).friendUsernames.insert(username1);
    }
}

void ChatLogicServer::removeFriendFromCache(const std::string& username1, const std::string& username2) {
    if (m_cachedUsers.count(username1)) {
        m_cachedUsers.at(username1).friendUsernames.erase(username2);
    }
    if (m_cachedUsers.count(username2)) {
        m_cachedUsers.at(username2).friendUsernames.erase(username1);
    }
    std::cout << "Cache updated: Friend relationship removed between " << username1 << " and " << username2 << std::endl;
}

void ChatLogicServer::addOrUpdateGroupChatInCache(const std::string& chatId, const std::string& chatName, const std::string& creatorUsername) {
    if (m_cachedGroupChats.count(chatId)) {
        // Обновляем существующий
        CachedGroupChat& chat = m_cachedGroupChats.at(chatId);
        chat.name = chatName;
        // chat.creatorUsername обычно не меняется, но можно добавить, если нужно
        std::cout << "Cache updated: Group chat " << chatId << " updated." << std::endl;
    } else {
        // Добавляем новый
        m_cachedGroupChats[chatId] = {chatId, chatName, creatorUsername, {}};
        std::cout << "Cache updated: Group chat " << chatId << " добавлен." << std::endl;
    }
}

void ChatLogicServer::removeGroupChatFromCache(const std::string& chatId) {
    if (m_cachedGroupChats.count(chatId)) {
        const auto& members = m_cachedGroupChats.at(chatId).memberUsernames;
        // Удаляем этот чат из списка groupChatIds у всех его бывших участников
        for (const std::string& memberUsername : members) {
            if (m_cachedUsers.count(memberUsername)) {
                m_cachedUsers.at(memberUsername).groupChatIds.erase(chatId);
            }
        }
        m_cachedGroupChats.erase(chatId);
        std::cout << "Cache updated: Group chat " << chatId << " removed." << std::endl;
    }
}

void ChatLogicServer::addUserToGroupChatInCache(const std::string& username, const std::string& chatId) {
    if (m_cachedGroupChats.count(chatId)) {
        m_cachedGroupChats.at(chatId).memberUsernames.insert(username);
    }
    if (m_cachedUsers.count(username)) {
        m_cachedUsers.at(username).groupChatIds.insert(chatId);
    }
    std::cout << "Cache updated: User " << username << " added to group chat " << chatId << std::endl;
}

void ChatLogicServer::removeUserFromGroupChatInCache(const std::string& username, const std::string& chatId) {
    if (m_cachedGroupChats.count(chatId)) {
        m_cachedGroupChats.at(chatId).memberUsernames.erase(username);
    }
    if (m_cachedUsers.count(username)) {
        m_cachedUsers.at(username).groupChatIds.erase(chatId);
    }
    std::cout << "Cache updated: User " << username << " removed from group chat " << chatId << std::endl;
}

void ChatLogicServer::updateUserCacheOnLogin(const std::string& username, std::shared_ptr<INetworkClient> client) {
    if (m_cachedUsers.count(username)) {
        CachedUser& user = m_cachedUsers.at(username);
        user.isOnline = true;
        user.client = client;
        std::cout << "Cache updated: User " << username << " is online." << std::endl;
    } else {
        std::cerr << "User " << username << " not found in cache during login update. Attempting to load from DB..." << std::endl;
        auto userRow = m_db->fetchOne("SELECT id FROM users WHERE username = ?;", {username});
        if (userRow && userRow.value().count("id")) {
            try {
                long long userId = std::any_cast<long long>(userRow.value().at("id"));
                m_cachedUsers[username] = {username, userId, true, client, {}, {}}; // Сразу ставим онлайн
                
                std::cout << "Cache updated: User " << username << " (re)loaded and set online." << std::endl;
            } catch (const std::bad_any_cast& e) {
                std::cerr << "Error casting user ID for cache during login update: " << e.what() << std::endl;
            }
        } else {
            std::cerr << "Failed to load user " << username << " into cache during login update." << std::endl;
        }
    }
}

void ChatLogicServer::updateUserCacheOnLogout(const std::string& username) {
    if (m_cachedUsers.count(username)) {
        CachedUser& user = m_cachedUsers.at(username);
        user.isOnline = false;
        user.client = nullptr;
        std::cout << "Cache updated: User " << username << " is offline." << std::endl;
    }
}

