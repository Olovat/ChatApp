#include <QCoreApplication>
#include "chat_logic_server.h"
#include "qt_network_adapter.h"
#include "qt_database_adapter.h"
#include <QDir>
#include <memory>
#include <iostream>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    // Создаем адаптеры
    auto dbAdapter = std::make_unique<QtDatabaseAdapter>("QSQLITE");
    auto networkAdapter = std::make_shared<QtNetworkServerAdapter>(); 
    QDir appDir(QCoreApplication::applicationDirPath());
    appDir.cdUp(); // Я уже не помню где бд изначально лежит, потом поправлю.
    appDir.cdUp(); 
    appDir.cdUp();
    QString dataPath = appDir.absolutePath() + "/data";
    QDir dataDir(dataPath);
    if (!dataDir.exists()) {
        if (!dataDir.mkpath(".")) {
            qCritical() << "Failed to create data directory:" << dataPath;
            return 1;
        }
    }
    std::string dbFilePath = (dataPath + "/authorisation.db").toStdString();

    if (!dbAdapter->connect(dbFilePath)) {
        qCritical() << "Failed to connect to database via adapter:" << QString::fromStdString(dbAdapter->lastError());
        return 1;
    }
    ChatLogicServer logicServer(std::move(dbAdapter));
    logicServer.setNetworkServer(networkAdapter);

    // Обработка ошибки
    if (!logicServer.initializeDatabase()) {
        qCritical() << "Failed to initialize database.";
        return 1;
    }
    //Запускаем сервер
    logicServer.startServer(5402);

    int result = a.exec();
    std::cout << "Server application event loop finished. Exiting..." << std::endl;
    return result;
}
