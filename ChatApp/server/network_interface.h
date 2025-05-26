#ifndef NETWORK_INTERFACE_H
#define NETWORK_INTERFACE_H

#include <string>
#include <vector>
#include <functional>
#include <memory>

class INetworkClient;

class INetworkServer {
public:
    virtual ~INetworkServer() = default;
    virtual bool start(int port) = 0;
    virtual void stop() = 0;
    virtual void broadcastMessage(const std::string& message) = 0;

    using ClientConnectedCallback = std::function<void(std::shared_ptr<INetworkClient>)>;
    using ClientDisconnectedCallback = std::function<void(std::shared_ptr<INetworkClient>)>;
    using MessageReceivedCallback = std::function<void(std::shared_ptr<INetworkClient>, const std::string&)>;

    virtual void setClientConnectedCallback(ClientConnectedCallback cb) = 0;
    virtual void setClientDisconnectedCallback(ClientDisconnectedCallback cb) = 0;
    virtual void setMessageReceivedCallback(MessageReceivedCallback cb) = 0;
};

class INetworkClient {
public:
    virtual ~INetworkClient() = default;
    virtual void sendMessage(const std::string& message) = 0;
    virtual std::string getClientId() const = 0;
    virtual bool isConnected() const = 0;
    virtual void disconnectClient() = 0;
};

#endif // NETWORK_INTERFACE_H