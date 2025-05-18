#ifndef MAIN_WINDOW_INTERFACE_H
#define MAIN_WINDOW_INTERFACE_H
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <functional>

// Forward declaration

class MainWindowInterface {
public:
    enum class Mode { Production, Testing };

    virtual ~MainWindowInterface() = default;

    // Основные методы
    virtual void setTestCredentials(const std::string& login, const std::string& pass) = 0;
    virtual std::string getUsername() const = 0;
    virtual std::string getUserpass() const = 0;
    virtual bool connectToServer(const std::string& host, unsigned short port) = 0;
    virtual void sendMessageToServer(const std::string& message) = 0;
    virtual void handlePrivateMessage(const std::string& sender, const std::string& message) = 0;
    virtual void sendPrivateMessage(const std::string& recipient, const std::string& message) = 0;
    virtual void requestPrivateMessageHistory(const std::string& otherUser) = 0;
    virtual void createGroupChat(const std::string& chatName, const std::string& chatId) = 0;
    virtual void joinGroupChat(const std::string& chatId) = 0;
    virtual void startAddUserToGroupMode(const std::string& chatId) = 0;
    virtual bool isLoginSuccessful() const = 0;
    virtual bool isConnected() const = 0;
    virtual std::vector<std::string> getOnlineUsers() const = 0;
    virtual bool hasPrivateChatWith(const std::string& username) const = 0;
    virtual int privateChatsCount() const = 0;
    virtual std::vector<std::string> privateChatParticipants() const = 0;
    virtual void requestUnreadCounts() = 0;

    // Сигналы (заменены на std::function)
    std::function<void()> onAuthSuccess;
    std::function<void()> onRegisterSuccess;
    std::function<void()> onUserListUpdated;


protected:
    Mode mode;
    std::string m_username;
    std::string m_userpass;
    bool m_loginSuccesfull;
    std::vector<std::string> recentSentMessages;
    std::map<std::string, bool> userFriends;
    std::string pendingGroupChatId;
    std::map<std::string, int> unreadPrivateMessageCounts;
    std::map<std::string, int> unreadGroupMessageCounts;
};
#endif // MAIN_WINDOW_INTERFACE_H
