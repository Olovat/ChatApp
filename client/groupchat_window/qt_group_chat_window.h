#ifndef QT_GROUP_CHAT_WINDOW_H
#define QT_GROUP_CHAT_WINDOW_H
#include "group_chat_interface.h"
#include <QWidget>

class QtMainWindow;
namespace Ui {
class Form;
}

class QtGroupChatWindow : public QWidget, public GroupChatInterface
{
    Q_OBJECT

public:
    explicit QtGroupChatWindow(const QString& chatId, const QString& chatName,
                               QtMainWindow* mainWindow, QWidget* parent = nullptr);
    ~QtGroupChatWindow();

    // Реализация GroupChatInterface
    void receiveMessage(const std::string& sender, const std::string& message) override;
    void receiveMessage(const std::string& sender, const std::string& message,
                        const std::string& timestamp) override;

    void beginHistoryDisplay() override;
    void addHistoryMessage(const std::string& formattedMessage) override;
    void endHistoryDisplay() override;

    void updateMembersList(const std::vector<std::string>& members) override;
    void setCreator(const std::string& creator) override;

private slots:
    void on_pushButton_2_clicked();
    void on_lineEdit_returnPressed();
    void on_pushButton_clicked();
    void on_pushButton_3_clicked();
    void on_pushButton_4_clicked();

private:
    Ui::Form* ui;
    QtMainWindow* mainWindow;

    void sendMessage(const QString& message);
    void updateWindowTitle();
    QString convertUtcToLocalTime(const QString& utcTimestamp);
};
#endif // QT_GROUP_CHAT_WINDOW_H
