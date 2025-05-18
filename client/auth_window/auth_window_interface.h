#ifndef AUTH_WINDOW_INTERFACE_H
#define AUTH_WINDOW_INTERFACE_H

#include <string>

class AuthWindowInterface {
public:
    virtual ~AuthWindowInterface() = default;

    virtual std::string getLogin() const = 0;
    virtual std::string getPass() const = 0;

    virtual void show() = 0;
    virtual void hide() = 0;
    virtual void LineClear() = 0;
    virtual void setButtonsEnabled(bool enabled) = 0;


protected:
    std::string m_username;
    std::string m_userpass;
};

#endif // AUTH_WINDOW_INTERFACE_H
