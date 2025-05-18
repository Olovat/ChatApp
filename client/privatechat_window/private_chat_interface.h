#ifndef PRIVATE_CHAT_INTERFACE_H
#define PRIVATE_CHAT_INTERFACE_H
#include <string>


class PrivateChatInterface {
public:
    virtual ~PrivateChatInterface() = default;

    // Основные методы
    virtual void receiveMessage(const std::string& sender, const std::string& message) = 0;
    virtual void receiveMessage(const std::string& sender, const std::string& message,
                                const std::string& timestamp) = 0;
    virtual void setOfflineStatus(bool offline) = 0;
    virtual void beginHistoryDisplay() = 0;
    virtual void addHistoryMessage(const std::string& formattedMessage) = 0;
    virtual void endHistoryDisplay() = 0;
    virtual void markMessagesAsRead() = 0;


protected:
    std::string username;
    bool isOffline;
    bool historyDisplayed = false;
    bool statusMessagePending = false;
    bool previousOfflineStatus = false;
};

#endif // PRIVATE_CHAT_INTERFACE_H
