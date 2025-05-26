#ifndef TRANSITWINDOW_H
#define TRANSITWINDOW_H

#include <QDialog>

namespace Ui {
class TransitWindow;
}

class MainWindow;

class TransitWindow : public QDialog
{
    Q_OBJECT

public:
    explicit TransitWindow(MainWindow *mainWindow, QWidget *parent = nullptr);
    ~TransitWindow();

private slots:
    void on_createButton_clicked();
    void on_cancelButton_clicked();

private:
    Ui::TransitWindow *ui;
    MainWindow *mainWindow;
};

#endif // TRANSITWINDOW_H