#ifndef REG_WINDOW_H
#define REG_WINDOW_H

#include <QWidget>
#include <QDialog>


namespace Ui {
class reg_window;
}

class reg_window : public QDialog
{
    Q_OBJECT

public:
    explicit reg_window(QWidget *parent = nullptr);
    ~reg_window();
    QString getName();
    QString getPass();
    bool checkPass();

signals:
    void register_button_clicked2();

private slots:
    // очень опасно, желательно никогда не трогать,сверял названия по буквам.
    void on_Password_line_textEdited(const QString &arg1);
    void on_Confirm_line_textEdited(const QString &arg1);
    void on_Register_button_clicked();
    void on_Login_line_textEdited(const QString &arg1);

private:
    Ui::reg_window *ui;
    QString m_userName;
    QString m_userPass;
    QString m_confirmation;
};

#endif // REG_WINDOW_H
