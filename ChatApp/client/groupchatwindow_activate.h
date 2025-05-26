#ifndef GROUPCHATWINDOW_ACTIVATE_H
#define GROUPCHATWINDOW_ACTIVATE_H

#include "groupchatwindow.h"
#include <QEvent>

// Расширение класса GroupChatWindow для перехвата события активации окна
class GroupChatWindowWithActivate : public GroupChatWindow
{
    Q_OBJECT
    
public:
    explicit GroupChatWindowWithActivate(const QString &chatId, const QString &chatName, QWidget *parent = nullptr)
        : GroupChatWindow(chatId, chatName, parent) {}
    
protected:
    bool event(QEvent *e) override 
    {
        // Перехватываем событие активации окна
        if (e->type() == QEvent::WindowActivate) {
            // Отмечаем сообщения как прочитанные при активации окна
            emit markAsReadRequested(chatId);
        }
        return GroupChatWindow::event(e);
    }
};

#endif // GROUPCHATWINDOW_ACTIVATE_H
