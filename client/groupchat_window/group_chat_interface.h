#ifndef GROUP_CHAT_INTERFACE_H
#define GROUP_CHAT_INTERFACE_H
#include <string>
#include <vector>
#include <functional>

class GroupChatInterface {
public:
    virtual ~GroupChatInterface() = default;

    // Основные методы
    virtual void receiveMessage(const std::string& sender, const std::string& message) = 0;
    virtual void receiveMessage(const std::string& sender, const std::string& message, const std::string& timestamp) = 0;

    virtual void beginHistoryDisplay() = 0;
    virtual void addHistoryMessage(const std::string& formattedMessage) = 0;
    virtual void endHistoryDisplay() = 0;

    virtual void updateMembersList(const std::vector<std::string>& members) = 0;
    virtual void setCreator(const std::string& creator) = 0;

    // Сигналы (заменены на std::function)
    std::function<void(const std::string&)> onSendMessage;
    std::function<void()> onAddUserRequest;
    std::function<void()> onRemoveUserRequest;
    std::function<void()> onDeleteChatRequest;

protected:
    std::string chatId;
    std::string chatName;
    std::string creator;
    bool isCreator = false;
    bool historyDisplayed = false;
};
#endif // GROUP_CHAT_INTERFACE_H
