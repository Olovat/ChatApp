#ifndef MOCK_NETWORK_SERVER_H
#define MOCK_NETWORK_SERVER_H

#include "gmock/gmock.h"
#include "../../server/network_interface.h"
#include <memory>

class MockNetworkServer : public INetworkServer {
public:
    MOCK_METHOD(bool, start, (int port), (override));
    MOCK_METHOD(void, stop, (), (override));
    MOCK_METHOD(void, broadcastMessage, (const std::string& message), (override));
    MOCK_METHOD(void, setClientConnectedCallback, (ClientConnectedCallback cb), (override));
    MOCK_METHOD(void, setClientDisconnectedCallback, (ClientDisconnectedCallback cb), (override));
    MOCK_METHOD(void, setMessageReceivedCallback, (MessageReceivedCallback cb), (override));
};

#endif // MOCK_NETWORK_SERVER_H