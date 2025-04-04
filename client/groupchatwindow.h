#ifndef GROUPCHATWINDOW_H
#define GROUPCHATWINDOW_H

#include <QWidget>
#include <QDateTime>
#include <QList>

class MainWindow;

namespace Ui {
class Form;
}

class GroupChatWindow : public QWidget
{
    Q_OBJECT

public:
    explicit GroupChatWindow(const QString &chatId, const QString &chatName, MainWindow *mainWindow, QWidget *parent = nullptr);
    ~GroupChatWindow();

    void receiveMessage(const QString &sender, const QString &message);
    void receiveMessage(const QString &sender, const QString &message, const QString &timestamp);
    
    void beginHistoryDisplay();
    void addHistoryMessage(const QString &formattedMessage);
    void endHistoryDisplay();
    
    void updateMembersList(const QStringList &members);
    void setCreator(const QString &creator); // Добавляем метод для установки создателя чата

private slots:
    void on_pushButton_2_clicked(); // Кнопка отправки сообщения
    void on_lineEdit_returnPressed(); // Обработка нажатия Enter в поле ввода
    void on_pushButton_clicked();  // Кнопка добавления пользователя в чат
    void on_pushButton_3_clicked(); // Кнопка удаления пользователя из чата

private:
    Ui::Form *ui;
    QString chatId;
    QString chatName;
    MainWindow *mainWindow;
    QString creator; // Добавляем переменную для хранения имени создателя чата
    bool isCreator = false; // Флаг, указывающий, является ли текущий пользователь создателем
    
    void sendMessage(const QString &message);
    void updateWindowTitle();
    
    // Флаги для отображения истории сообщений
    bool historyDisplayed = false;
    
    // Метод для конвертации времени UTC в локальное
    QString convertUtcToLocalTime(const QString &utcTimestamp);
};

#endif // GROUPCHATWINDOW_H
