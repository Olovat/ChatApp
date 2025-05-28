#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QApplication>
#include <QMessageBox>
#include <QMainWindow>
#include <QString>
#include <QtSql/QtSql>
#include "auth_window.h"
#include "reg_window.h"
#include <QUuid>
#include <QListWidgetItem>
#include <QMap>
#include <QDialog>
#include <QListWidget>
#include <QSet>
#include "../tests/test_client/MockDatabase.h"
#include "ui_mainwindow.h"
#include "controller_manager.h"

// Предварительные объявления
class ChatController;
class MainWindowController;
class PrivateChatWindow;
class GroupChatWindow;
class TransitWindow;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    enum class Mode { Production, Testing };

    explicit MainWindow(QWidget *parent = nullptr);
    explicit MainWindow(Mode mode, QWidget *parent = nullptr); // Конструктор для тестового режима
    ~MainWindow();
    
    // Для тестового режима
    void setTestCredentials(const QString& login, const QString& pass) {
        m_username = login;
        m_userpass = pass;
    }
    
    // Метод отображения главного окна (после успешной авторизации)
    void display();
    
    // Методы доступа к данным пользователя
    void setLogin(const QString &login);
    void setPass(const QString &pass);
    QString getUsername() const;
    QString getUserpass() const;
    
    // Методы для работы с приватными чатами
      // Методы для управления состоянием авторизации
    bool isLoginSuccessful() const;
    void setLoginSuccessful(bool success); // Метод для установки статуса авторизации
    
    // Методы для тестирования
    QStringList getDisplayedUsers() const;
    QString getCurrentUsername() const;
    
    // Для поддержки тестов
    void setTestDatabase(std::unique_ptr<MockDatabase> db) { testDb = std::move(db); }
    
    // Получение контроллера для тестов
    MainWindowController* getController() const { return controller; }
    
    // Метод для получения счетчиков непрочитанных сообщений групповых чатов
    QMap<QString, int> getUnreadGroupMessageCounts() const { return unreadGroupMessageCounts; }
    
    void initializeCommon(); // Инициализация общих компонентов

signals:
    void authSuccess();
    void registerSuccess();
    void userListUpdated();
    void requestAuthorizeUser(const QString &username, const QString &password);
    void requestRegisterUser(const QString &username, const QString &password);
    void userSelected(const QString &username);
    void requestSearchUsers(const QString &query);
    void requestAddUserToFriends(const QString &username);
    void requestCreateGroupChat(const QString &chatName);
    void requestJoinGroupChat(const QString &chatId);
    void requestGroupMessageSend(const QString &chatId, const QString &message);
    void requestAddUserToGroupChat(const QString &chatId, const QString &username);
    void requestRemoveUserFromGroupChat(const QString &chatId, const QString &username);
    void requestDeleteGroupChat(const QString &chatId);
    void requestStartAddUserToGroupMode(const QString &chatId);
    void groupChatSelected(const QString &chatId);
    void userDoubleClicked(const QString &username);
    void requestPrivateMessageSend(const QString &recipient, const QString &message);
    void requestPrivateMessageHistory(const QString &username);

public slots:
    // Слоты для обновления пользовательского интерфейса
    void updateUserList(const QStringList &users);
    void showSearchResults(const QStringList &users);
    // Handle successful authentication
    void onAuthSuccess();
    // Handle successful registration
    void onRegisterSuccess();
    void updateUnreadCounts(const QMap<QString, int> &counts);
    void updateGroupUnreadCounts(const QMap<QString, int> &counts);

private slots:
    // Слоты для обработки действий в интерфейсе
    void authorizeUser(); // Обработка нажатия кнопки авторизации
    void registerUser();  // Обработка нажатия кнопки регистрации
    void prepareForRegistration(); // Для переключения на окно регистрации
    void on_pushButton_clicked();  // Создать групповой чат
    void on_pushButton_2_clicked(); // Присоединиться к групповому чату
    void on_lineEdit_returnPressed(); // Поиск пользователей
    void onUserSelected(QListWidgetItem *item); // Выбор пользователя из списка
    // Функции для поиска пользователей
    void searchUsers();
    void addUserToFriends(const QString &username);
    void startAddUserToGroupMode(const QString &chatId); // Установка режима добавления пользователя в группу
    void onUserDoubleClicked(QListWidgetItem *item);

public:
    Mode mode;
    std::unique_ptr<MockDatabase> testDb; // Mock база данных
    
    // Переменные для работы с окнами авторизации и регистрации
    auth_window ui_Auth;
    reg_window ui_Reg;

private:
    Ui::MainWindow *ui;
    // Контроллер для управления логикой приложения
    MainWindowController *controller;
    QString m_username;
    QString m_userpass;
    bool m_loginSuccesfull;
      // Функции для создания или получения существующих окон чатов
    
public:
    GroupChatWindow* findOrCreateGroupChatWindow(const QString &chatId, const QString &chatName);
    
private:
    QMap<QString, GroupChatWindow*> groupChatWindows; // Map для хранения открытых групповых чатов

    // Таблица друзей пользователя
    QMap<QString, bool> userFriends; // имя пользователя -> флаг онлайн
    
    // Хранение счетчиков непрочитанных сообщений
    QMap<QString, int> unreadMessageCounts; // приватные чаты: пользователь -> количество
    QMap<QString, int> unreadGroupMessageCounts; // групповые чаты: id чата -> количество
    
    // Результаты поиска
    QStringList searchResults;
    QDialog* searchDialog;
    QListWidget* searchListWidget;
      // UI функции
    void updateWindowTitle(); // Обновление заголовка окна
    // Обновление статусов чатов
    void updatePrivateChatStatuses(const QMap<QString, bool> &userStatusMap);
    
    // Хранит пользователей, с которыми было общение
    QSet<QString> recentChatPartners;

    QStringList userList; // Хранение текущего списка пользователей
};

#endif // MAINWINDOW_H
