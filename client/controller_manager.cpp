#include "controller_manager.h"
#include "mainwindow_controller.h"
#include <QDebug>

ControllerManager::ControllerManager() 
    : chatController(nullptr), 
      mainWindowController(nullptr) 
{
    chatController = new ChatController(nullptr);
    
}

ControllerManager::~ControllerManager() 
{
    if (chatController && !chatController->parent()) {
        delete chatController;
    }
    
   
}

ChatController* ControllerManager::getChatController() 
{
    if (!chatController) {
        qDebug() << "Creating new ChatController instance";
        chatController = new ChatController(nullptr);
    } else if (!chatController->isValid()) {
        qDebug() << "Reconnecting existing ChatController instance";
        // Вместо пересоздания, просто реконект, иначе дубликат.
        chatController->reconnectToServer();
    }
    return chatController;
}

void ControllerManager::registerMainWindowController(MainWindowController* controller) 
{
    mainWindowController = controller;
}

MainWindowController* ControllerManager::getMainWindowController() 
{
    return mainWindowController;
}
