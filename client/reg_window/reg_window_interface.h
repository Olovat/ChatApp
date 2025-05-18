#ifndef REG_WINDOW_INTERFACE_H
#define REG_WINDOW_INTERFACE_H

#include <string>

class RegWindowInterface {
public:
    virtual ~RegWindowInterface() = default;

    // Базовые методы
    virtual std::string getName() const = 0;
    virtual std::string getPass() const = 0;
    virtual void setName(const std::string& name) = 0;
    virtual void setPass(const std::string& pass) = 0;
    virtual void setButtonsEnabled(bool enabled) = 0;

protected:
    std::string m_userName;
    std::string m_userPass;
};

#endif // REG_WINDOW_INTERFACE_H
