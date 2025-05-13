#ifndef MOCK_NETWORK_SERVER_H
#define MOCK_NETWORK_SERVER_H

#include "gmock/gmock.h"
#include "../../server/network_interface.h" // Путь к вашему network_interface.h
#include <memory> // для std::shared_ptr

// Прямое объявление, если INetworkClient используется в аргументах методов INetworkServer
// class INetworkClient;

class MockNetworkServer : public INetworkServer {
public:
    MOCK_METHOD(bool, startListening, (int port), (override));
    MOCK_METHOD(void, stopListening, (), (override));
    MOCK_METHOD(void, sendMessageToClient, (std::shared_ptr<INetworkClient> client, const std::string& message), (override));
    // Добавьте другие виртуальные методы из INetworkServer, если они есть
};

#endif // MOCK_NETWORK_SERVER_H