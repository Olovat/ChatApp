#include <gmock/gmock.h>
#include <QTcpSocket>
#include <qsignalspy.h>
#include "chat_controller.h"

class MockTcpSocket : public QTcpSocket {
public:
    MOCK_METHOD0(abort, void());
    MOCK_METHOD0(close, void());
    MOCK_METHOD1(setParent, void(QObject*));
    MOCK_METHOD2(connectToHost, void(const QString&, quint16)); // ❗ ВАЖНО: правильно указываем количество аргументов
    MOCK_METHOD1(waitForConnected, bool(int));
    MOCK_METHOD1(write, qint64(const QByteArray&));
    MOCK_METHOD0(state, QAbstractSocket::SocketState());
    // Метод для эмуляции данных от сервера
    void simulateReadyRead(const QByteArray& data) {
        buffer.append(data);
        emit readyRead();
    }

    // Переопределяем readAll(), чтобы возвращать данные из буфера
    QByteArray readAll() {
        QByteArray result = buffer;
        buffer.clear();
        return result;
    }

private:
    QByteArray buffer;
};

class ChatControllerNoNetworkTest : public ::testing::Test {
protected:
    ChatController* controller;
    MockTcpSocket* mockSocket;

    void SetUp() override {
        mockSocket = new MockTcpSocket();
        controller = new ChatController(nullptr, mockSocket);

    }

    void TearDown() override {
        delete controller;
    }
};

TEST_F(ChatControllerNoNetworkTest, ProcessAuthOk_SendsSuccessSignal) {
    // Подписываемся на сигнал
    QSignalSpy authSuccess(controller, &ChatController::authenticationSuccessful);

    // Убеждаемся, что изначально не авторизованы
    EXPECT_FALSE(controller->isLoginSuccessful());

    // Эмулируем успешный ответ сервера
    controller->processServerResponse("AUTH_OK");

    // Проверяем, что сигнал был вызван
    EXPECT_EQ(authSuccess.count(), 1);

    // Проверяем, что статус авторизации изменился
    EXPECT_TRUE(controller->isLoginSuccessful());
}

TEST_F(ChatControllerNoNetworkTest, ProcessPrivateMessage_EmitsSignal) {
    QString sender = "alice";
    QString message = "Привет!";
    QString expectedMessage = QString("PRIVATE:%1:%2").arg(sender, message);

    QSignalSpy privateMessageReceived(controller, &ChatController::privateMessageReceived);

    // Обрабатываем сообщение
    controller->processServerResponse(expectedMessage);

    // Проверяем, что сигнал был отправлен
    EXPECT_EQ(privateMessageReceived.count(), 1);

    auto args = privateMessageReceived.takeFirst();
    EXPECT_EQ(args.at(0).toString(), sender);
    EXPECT_EQ(args.at(1).toString(), message);
}

TEST_F(ChatControllerNoNetworkTest, ProcessGroupMessage_EmitsSignal) {
    QString chatId = "group123";
    QString sender = "alice";
    QString message = "Привет";
    QString command = QString("GROUP_MESSAGE:%1:%2:%3").arg(chatId, sender, message);

    QSignalSpy groupMessageReceived(controller, &ChatController::groupMessageReceived);

    controller->processServerResponse(command);

    EXPECT_EQ(groupMessageReceived.count(), 1);

    auto args = groupMessageReceived.takeFirst();
    EXPECT_EQ(args.at(0).toString(), chatId);
    EXPECT_EQ(args.at(1).toString(), sender);
    EXPECT_EQ(args.at(2).toString(), message);
}

TEST_F(ChatControllerNoNetworkTest, ProcessGroupChatCreated_EmitsSignal) {
    QString chatId = "group123";
    QString chatName = "MyGroup";
    QString command = QString("GROUP_CHAT_CREATED:%1:%2").arg(chatId, chatName);

    QSignalSpy groupChatCreated(controller, &ChatController::groupChatCreated);

    controller->processServerResponse(command);

    EXPECT_EQ(groupChatCreated.count(), 1);

    auto args = groupChatCreated.takeFirst();
    EXPECT_EQ(args.at(0).toString(), chatId);
    EXPECT_EQ(args.at(1).toString(), chatName);
}

TEST_F(ChatControllerNoNetworkTest, ProcessFriendAdded_AddsToList) {
    QString friendName = "test_user";

    // Подписываемся на сигнал
    QSignalSpy friendAdded(controller, &ChatController::friendAddedSuccessfully);

    // Эмулируем ответ сервера о добавлении друга
    controller->processServerResponse(QString("FRIEND_ADDED:%1").arg(friendName));

    // Проверяем, что сигнал был вызван
    EXPECT_EQ(friendAdded.count(), 1);
    if (friendAdded.size() > 0) {
        EXPECT_EQ(friendAdded[0][0].toString(), friendName);
    }

    // Проверяем, что пользователь добавлен в список друзей
    EXPECT_TRUE(controller->isFriend(friendName));
    EXPECT_TRUE(controller->getFriendList().contains(friendName));
}
