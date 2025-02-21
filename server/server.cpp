#include "server.h"

Server::Server(){
    /*проверяем запустился ли сервер; первый аргумент говорит, что сервер принимает сигналы
    пришедшие с любого адреса, а второй, что сервер прослушивает порт 2323*/
    if(this->listen(QHostAddress::Any, 1010))
    {
        qDebug() << "start";
    }
    else
    {
        qDebug() << "error";
    }
    nextBlockSize = 0;
}

//обработчик новых подключений
void Server::incomingConnection(qintptr socketDescriptor){
    socket = new QTcpSocket;
    //устанавливаем в сокет дескриптор из аргумента функции. Дескриптор - неотрицательное число, которое индефицирует поток ввода-вывода
    socket->setSocketDescriptor(socketDescriptor);
    //объединяем сигналы readyRead с соответствующем слотом
    connect(socket, &QTcpSocket::readyRead, this, &Server::slotReadyRead);
    //объединяем сигналы readyRead со слотом deleteLater, теперь при отключении клиента приложение удалит сокет
    connect(socket, &QTcpSocket::disconnected, this, &Server::deleteLater);

    //помещаем сокет в контейнер, функция вызывается при каждом новом подключении клиента, получается для каждого клиента мы создаем новый сокет
    Sockets.push_back(socket);
    qDebug() << "client connected" << socketDescriptor;
}

//слот для чтения сообщений
void Server::slotReadyRead(){
     //в переменную сокет нужно записать именно тот сокет, с которого пришел запрос
    socket = (QTcpSocket*)sender();
     QDataStream in(socket);
    //с помощью объекта работаем с данными нахлдящимися в сокете
     in.setVersion(QDataStream::Qt_6_2);
     if(in.status() == QDataStream::Ok){
         qDebug() << "read...";
         /*QString str;
         in >> str;
         qDebug() << str;
         SendToCllient(str);*/
         for(;;){
             if(nextBlockSize == 0){
                 qDebug() << "nextBlockSize == 0";
                 if(socket->bytesAvailable() < 2){
                     qDebug() << "Data < 2, break";
                     break;
                 }
                 in >> nextBlockSize;
                 qDebug() << "nextBlockSize ==" << nextBlockSize;
             }
             if(socket->bytesAvailable() < nextBlockSize){
                 qDebug() << "Data not full, break";
                 break;
             }
             QString str;
             in >> str;
             nextBlockSize = 0;
             qDebug() << str;
             SendToCllient(str);
             break;
         }
     }
     else{
         qDebug() << "DataStream error";
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









