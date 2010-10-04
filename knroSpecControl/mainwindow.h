#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>


#include <libindi/baseclient.h>

class QProcess;
class QTemporaryFile;

namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow, INDI::BaseClient
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

protected:
    virtual void newDevice();
    virtual void newBLOB(IBLOB *bp);
    virtual void newSwitch(ISwitchVectorProperty *svp);
private:
    Ui::MainWindow *ui;
    QProcess *kstProcess;
    QTemporaryFile *specTempFile;
    QString currentSMode;
    bool BLOBDirty;

public slots:
    void connectServer();
    void disconnectServer();
    void updateModeButtons(int specMode);

signals:
    void modeUpdated(int specMode);
};

#endif // MAINWINDOW_H
