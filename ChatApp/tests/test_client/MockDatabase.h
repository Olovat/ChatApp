#ifndef MOCKDATABASE_H
#define MOCKDATABASE_H

#include <QMap>
#include <QString>

class MockDatabase {
public:
    QMap<QString, QString> users; // login -> password

    bool registerUser(const QString& login, const QString& pass) {
        if (users.contains(login)) return false;
        users[login] = pass;
        return true;
    }

    bool authenticate(const QString& login, const QString& pass) const {
        return users.contains(login) && users[login] == pass;
    }

};

#endif // MOCKDATABASE_H
