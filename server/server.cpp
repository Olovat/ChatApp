#include "server.h"

Server::Server(){
    /*проверяем запустился ли сервер; первый аргумент говорит, что сервер принимает сигналы
    пришедшие с любого адреса, а второй, что сервер прослушивает порт 2323*/
    if(this->listen(QHostAddress::Any, 5402))
    {
        qDebug() << "start";
        
        // Проверка соединения с базой данных
        if (!connectDB()) {
            qDebug() << "Failed to connect to database";
        } else {
            qDebug() << "Database connection established";
        }
    }
    else
    {
        qDebug() << "error";
    }
    nextBlockSize = 0;
}

Server::~Server()
{
    // снос соединения с клиентами
    for (QTcpSocket* socket : Sockets) {
        if (socket && socket->isOpen()) {
            socket->close();
            socket->deleteLater();
        }
    }
    Sockets.clear();
    
    // снос соединения с базой данных
    if (srv_db.isOpen()) {
        srv_db.close();
    }
    
    qDebug() << "Server destroyed";
}

//обработчик новых подключений
void Server::incomingConnection(qintptr socketDescriptor){
    socket = new QTcpSocket;
    //устанавливаем в сокет дескриптор из аргумента функции. Дескриптор - неотрицательное число, которое индефицирует поток ввода-вывода
    socket->setSocketDescriptor(socketDescriptor);
    //объединяем сигналы readyRead с соответствующем слотом
    connect(socket, &QTcpSocket::readyRead, this, &Server::slotReadyRead);
    // ВАЖНОЕ ИЗМЕНЕНИЕ: соединяем сигнал disconnected с нашим слотом clientDisconnected
    // вместо deleteLater, чтобы корректно удалить сокет из всех структур
    connect(socket, &QTcpSocket::disconnected, this, &Server::clientDisconnected);

    //помещаем сокет в контейнер, функция вызывается при каждом новом подключении клиента, получается для каждого клиента мы создаем новый сокет
    Sockets.push_back(socket);
    qDebug() << "client connected" << socketDescriptor;
}

//слот для чтения сообщений
void Server::slotReadyRead()
{
    socket = (QTcpSocket*)sender();
    QDataStream in(socket);
    in.setVersion(QDataStream::Qt_6_2);
    
    if(in.status() == QDataStream::Ok){
        for(;;){
            if(nextBlockSize == 0){
                if(socket->bytesAvailable() < 2){
                    break;
                }
                in >> nextBlockSize;
            }
            if(socket->bytesAvailable() < nextBlockSize){
                break;
            }
            
            // Парсинг сообщения
            QString message;
            in >> message;
            nextBlockSize = 0;
            
            // Обработка сообщения
            if (message.startsWith("AUTH:")) {
                QStringList parts = message.split(":", Qt::SkipEmptyParts);
                if (parts.size() >= 3) {
                    QString username = parts[1];
                    QString password = parts[2];
                    
                    bool success = authenticateUser(username, password);
                    QString response = success ? "AUTH_SUCCESS" : "AUTH_FAILED";
                    
                    // Добавление пользователя в список авторизированных
                    if (success) {
                        AuthenticatedUser user;
                        user.username = username;
                        user.socket = socket;
                        authenticatedUsers.append(user);
                    }
                    
                    // Отправка ответа клиенту об успешной или неуспешной авторизации
                    Data.clear();
                    QDataStream out(&Data, QIODevice::WriteOnly);
                    out.setVersion(QDataStream::Qt_6_2);
                    out << quint16(0) << response;
                    out.device()->seek(0);
                    out << quint16(Data.size() - sizeof(quint16));
                    socket->write(Data);
                }
            }
            else if (message.startsWith("REGISTER:")) {
                QStringList parts = message.split(":", Qt::SkipEmptyParts);
                if (parts.size() >= 3) {
                    QString username = parts[1];
                    QString password = parts[2];
                    
                    bool success = registerUser(username, password);
                    QString response = success ? "REGISTER_SUCCESS" : "REGISTER_FAILED";
                    
                    // Отправка ответа клиенту об успешной или неуспешной регистрации
                    Data.clear();
                    QDataStream out(&Data, QIODevice::WriteOnly);
                    out.setVersion(QDataStream::Qt_6_2);
                    out << quint16(0) << response;
                    out.device()->seek(0);
                    out << quint16(Data.size() - sizeof(quint16));
                    socket->write(Data);
                }
            }
            else {
                QString senderUsername = "Unknown";
                for (const AuthenticatedUser& user : authenticatedUsers) {
                    if (user.socket == socket) {
                        senderUsername = user.username;
                        break;
                    }
                }
                
                QString formattedMessage = senderUsername + ": " + message;
                SendToCllient(formattedMessage);
            }
            
            break;
        }
    }
}

void Server::SendToCllient(QString str){
    Data.clear();
    //создаем данные на вывод, передаем массив байтов(&Data) и режим работы(QIODevice::WriteOnly) только для записи
    QDataStream out(&Data, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_2);
    out << quint16(0) << str;
    out.device()->seek(0);
    out << quint16(Data.size() - sizeof(quint16));
    //нужно чтобы сообщения отправлялись всем клиентам, для этого мы и создавали сокет вектор
    for(int i = 0; i < Sockets.size(); i++){
        Sockets[i]->write(Data);
    }
}

bool Server::connectDB()
{
    srv_db = QSqlDatabase::addDatabase("QSQLITE");
    
    QDir appDir(QCoreApplication::applicationDirPath());
    
    appDir.cdUp();
    appDir.cdUp();
    appDir.cdUp(); 
    
    QString dataPath = appDir.absolutePath() + "/data";
    QDir dataDir(dataPath);
    if (!dataDir.exists()) {
        dataDir.mkpath(".");
    }
    
    srv_db.setDatabaseName(dataPath + "/authorisation.db");
    
    if(!srv_db.open())
    {
        qDebug() << "Cannot open database: " << srv_db.lastError();
        return false;
    }
    
    qDebug() << "Connected to database at: " << dataPath + "/authorisation.db";
    
    if (!initUserTable()) {
        qDebug() << "Failed to initialize user table";
        return false;
    }
    
    return true;
}

bool Server::initUserTable()
{
    QSqlQuery query(srv_db);
    return query.exec("CREATE TABLE IF NOT EXISTS userlist ("
                    "number INTEGER PRIMARY KEY AUTOINCREMENT,"
                    "name VARCHAR(20) UNIQUE, "
                    "pass VARCHAR(12), "
                    "xpos INTEGER DEFAULT 100, "
                    "ypos INTEGER DEFAULT 100, "
                    "width INTEGER DEFAULT 400, "
                    "length INTEGER DEFAULT 300)");
}

bool Server::authenticateUser(const QString &username, const QString &password)
{
    QSqlQuery query(srv_db);
    query.prepare("SELECT * FROM userlist WHERE name = :name");
    query.bindValue(":name", username);
    
    if (query.exec() && query.next()) {
        QString storedPassword = query.value("pass").toString();
        return (storedPassword == password);
    }
    return false;
}

bool Server::registerUser(const QString &username, const QString &password)
{
    QSqlQuery query(srv_db);
    query.prepare("INSERT INTO userlist (name, pass) VALUES (:name, :pass)");
    query.bindValue(":name", username);
    query.bindValue(":pass", password);
    
    return query.exec();
}

QString Server::getUsernameBySocket(QTcpSocket *socket)
{
    for (const AuthenticatedUser &user : authenticatedUsers) {
        if (user.socket == socket) {
            return user.username;
        }
    }
    return "Anonymous";
}

void Server::clientDisconnected()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        // Удаление пользователя из авторизированных, при разрыве соединения
        for (int i = 0; i < authenticatedUsers.size(); ++i) {
            if (authenticatedUsers[i].socket == socket) {
                qDebug() << "User" << authenticatedUsers[i].username << "disconnected";
                authenticatedUsers.removeAt(i);
                break;
            }
        }
        
        // Удаление сокета из вектора
        Sockets.removeOne(socket);
        qDebug() << "Client disconnected";
    }
}
