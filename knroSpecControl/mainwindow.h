#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>


#include <libindi/baseclient.h>

class QProcess;
class QTemporaryFile;
class QDataStream;

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
    virtual void newDevice(INDI::BaseDevice *dv);
    virtual void newProperty(INDI::Property *prop);
    virtual void newBLOB(IBLOB *bp);
    virtual void newSwitch(ISwitchVectorProperty *svp);
    virtual void newNumber(INumberVectorProperty *) {}
    virtual void newText(ITextVectorProperty *) {}
    virtual void newLight(ILightVectorProperty *) {}
    virtual void newMessage(INDI::BaseDevice *dp) {}
    virtual void serverConnected() {}
    virtual void serverDisconnected(int);

private:
    Ui::MainWindow *ui;
    QProcess *kstProcess;
    QTemporaryFile *specTempFile;
    QDataStream *out;
    QString currentSMode;
    bool BLOBDirty;
    bool deviceReceived;

public slots:
    void connectServer();
    void disconnectServer();
    void updateModeButtons(int specMode);
    void updateConnectionButtons(bool status);
    void validateDeviceReception();

signals:
    void modeUpdated(int specMode);
    void serverConnectionUpdated(bool status);
};

#endif // MAINWINDOW_H
