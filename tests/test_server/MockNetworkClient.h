#ifndef MOCK_NETWORK_CLIENT_H
#define MOCK_NETWORK_CLIENT_H

#include "gmock/gmock.h"
#include "../../server/network_interface.h"
#include <string>

class MockNetworkClient : public INetworkClient {
public:
    MOCK_METHOD(void, send, (const std::string& message), (override));
    MOCK_METHOD(std::string, getIdentifier, (), (const, override));
};

#endif // MOCK_NETWORK_CLIENT_H