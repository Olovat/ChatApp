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
    void setButtonsEnabled(bool enabled);
    void setName(const QString& name);
    void setPass(const QString& pass);

signals:
    void register_button_clicked2();
    void returnToAuth();

private slots:
    // очень опасно, желательно никогда не трогать,сверял названия по буквам.
    void on_Password_line_2_textEdited(const QString &arg1);
    void on_Register_button_2_clicked();
    void on_Login_line_2_textEdited(const QString &arg1);
    void on_return_auth_clicked();

private:
    Ui::reg_window *ui;
    QString m_userName;
    QString m_userPass;

protected:
void closeEvent(QCloseEvent *event) override;
};

#endif // REG_WINDOW_H
