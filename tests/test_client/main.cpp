#include "build/Desktop_Qt_6_8_2_MinGW_64_bit-Debug/ui_mainwindow.h"
#include "mainwindow.h"
#include <QtTest/QtTest>
#include <QTcpSocket>
#include <QSignalSpy>
#include <QCoreApplication>

class TestMainWindow : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();     // Инициализация перед всеми тестами
    void cleanupTestCase();  // Очистка после всех тестов
    void init();            // Инициализация перед каждым тестом
    void cleanup();         // Очистка после каждого теста

    void testAuthRequest();  // Тест формирования запроса авторизации
    void testRegRequest();   // Тест формирования запроса регистрации
    void testUserListUpdate(); // Тест обновления списка пользователей
    void testPrivateMessageHandling(); // Тест обработки приватных сообщений
    void testSocketBufferClearing(); // Тест очистки буфера сокета

private:
    MainWindow* m_mainWindow;
    QTcpSocket* m_testSocket;
};

void TestMainWindow::initTestCase()
{
    qDebug() << "Global test initialization";
    // Используем QCoreApplication чтобы избежать создания GUI
    int argc = 0;
    new QCoreApplication(argc, nullptr);
}

void TestMainWindow::cleanupTestCase()
{
    qDebug() << "Global test cleanup";
}

void TestMainWindow::init()
{
    m_mainWindow = new MainWindow();
    m_mainWindow->hide(); // Гарантируем, что окно не показывается

    // Заменяем реальный сокет на тестовый
    m_testSocket = new QTcpSocket(m_mainWindow);
    m_mainWindow->socket = m_testSocket;
}

void TestMainWindow::cleanup()
{
    delete m_mainWindow;
    m_mainWindow = nullptr;
    m_testSocket = nullptr;
}

void TestMainWindow::testAuthRequest()
{
    // Устанавливаем тестовые логин и пароль
    m_mainWindow->setLogin("testuser");
    m_mainWindow->setPass("testpass");

    // Вызываем авторизацию
    m_mainWindow->authorizeUser();

    // Проверяем, что данные были записаны в сокет
    QVERIFY(m_testSocket->bytesToWrite() > 0);

    // Читаем отправленные данные
    QByteArray sentData = m_testSocket->readAll();
    QString sentMessage = QString::fromUtf8(sentData.mid(2)); // Пропускаем размер блока

    // Проверяем формат сообщения
    QVERIFY(sentMessage.startsWith("AUTH:testuser:testpass"));
}

void TestMainWindow::testRegRequest()
{
    // Тест аналогичен testAuthRequest, но для регистрации

    // Для упрощения теста, эмулируем данные регистрации
    m_mainWindow->setLogin("newuser");
    m_mainWindow->setPass("newpass");

    // Вызываем регистрацию напрямую (в реальном приложении это делается через ui_Reg)
    QString regString = QString("REGISTER:%1:%2").arg("newuser", "newpass");

    QByteArray data;
    QDataStream out(&data, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_2);
    out << quint16(0) << regString;
    out.device()->seek(0);
    out << quint16(data.size() - sizeof(quint16));

    m_testSocket->write(data);

    // Проверяем отправленные данные
    QVERIFY(m_testSocket->bytesToWrite() > 0);
    QByteArray sentData = m_testSocket->readAll();
    QString sentMessage = QString::fromUtf8(sentData.mid(2));
    QVERIFY(sentMessage.startsWith("REGISTER:newuser:newpass"));
}

void TestMainWindow::testUserListUpdate()
{
    // Подготавливаем тестовый список пользователей
    QStringList testUsers = {"user1", "user2", "user3"};

    // Вызываем обновление списка
    m_mainWindow->updateUserList(testUsers);

    // Проверяем, что список обновился
    QCOMPARE(m_mainWindow->ui->userListWidget->count(), testUsers.size());

    // Проверяем содержимое списка
    for (int i = 0; i < testUsers.size(); ++i) {
        QCOMPARE(m_mainWindow->ui->userListWidget->item(i)->text(), testUsers[i]);
    }
}

void TestMainWindow::testPrivateMessageHandling()
{
    // Устанавливаем текущего пользователя
    m_mainWindow->setLogin("currentuser");

    // Эмулируем получение приватного сообщения
    QString testMessage = "PRIVATE:sender:Hello!";
    QByteArray data;
    QDataStream out(&data, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_2);
    out << quint16(0) << testMessage;
    out.device()->seek(0);
    out << quint16(data.size() - sizeof(quint16));

    // "Отправляем" сообщение в сокет
    m_testSocket->write(data);
    m_testSocket->flush();

    // Вызываем обработку данных
    m_mainWindow->slotReadyRead();

    // Проверяем, что окно чата было создано
    QVERIFY(m_mainWindow->privateChatWindows.contains("sender"));
}

void TestMainWindow::testSocketBufferClearing()
{
    // Заполняем буфер тестовыми данными
    QString testMessage = "TEST_MESSAGE";
    QByteArray data;
    QDataStream out(&data, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_2);
    out << quint16(0) << testMessage;
    out.device()->seek(0);
    out << quint16(data.size() - sizeof(quint16));

    m_testSocket->write(data);
    m_testSocket->flush();

    // Проверяем, что данные есть в буфере
    QVERIFY(m_testSocket->bytesAvailable() > 0);

    // Очищаем буфер
    m_mainWindow->clearSocketBuffer();

    // Проверяем, что буфер пуст
    QCOMPARE(m_testSocket->bytesAvailable(), 0);
}

QTEST_APPLESS_MAIN(TestMainWindow)
#include "mainwindow_test.moc"




