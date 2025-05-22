#ifndef CONTROLLER_MANAGER_H
#define CONTROLLER_MANAGER_H

#include "singleton_helper.h"
#include "chat_controller.h"

class MainWindowController;

class ControllerManager : public Singleton<ControllerManager> {
    friend class Singleton<ControllerManager>;

public:

    ChatController* getChatController();

    void registerMainWindowController(MainWindowController* controller);

    MainWindowController* getMainWindowController();

private:
    ControllerManager();
    ~ControllerManager();

    ChatController* chatController;
    MainWindowController* mainWindowController;
};

#endif // CONTROLLER_MANAGER_H
