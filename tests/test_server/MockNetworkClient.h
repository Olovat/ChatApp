#ifndef MOCK_NETWORK_CLIENT_H
#define MOCK_NETWORK_CLIENT_H

#include "gmock/gmock.h"
#include "../../server/network_interface.h"
#include <string>

class MockNetworkClient : public INetworkClient {
public:
    MOCK_METHOD(void, sendMessage, (const std::string& message), (override));
    MOCK_METHOD(std::string, getClientId, (), (const, override));
    MOCK_METHOD(bool, isConnected, (), (const, override));
    MOCK_METHOD(void, disconnectClient, (), (override));
};

#endif // MOCK_NETWORK_CLIENT_H