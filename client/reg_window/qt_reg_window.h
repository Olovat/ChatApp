#ifndef QT_REG_WINDOW_H
#define QT_REG_WINDOW_H

#include "reg_window_interface.h"
#include <QDialog>

namespace Ui {
class reg_window;
}

class QtRegWindow : public QDialog, public RegWindowInterface
{
    Q_OBJECT

public:
    explicit QtRegWindow(QWidget *parent = nullptr);
    ~QtRegWindow();

    // Реализация интерфейса
    std::string getName() const override;
    std::string getPass() const override;
    void setName(const std::string& name) override;
    void setPass(const std::string& pass) override;
    void setButtonsEnabled(bool enabled) override;

signals:
    void register_button_clicked2();
    void returnToAuth();

private slots:
    // Слоты с оригинальными именами
    void on_Password_line_2_textEdited(const QString &arg1);
    void on_Register_button_2_clicked();
    void on_Login_line_2_textEdited(const QString &arg1);
    void on_return_auth_clicked();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    Ui::reg_window *ui;
};

#endif // QT_REG_WINDOW_H
