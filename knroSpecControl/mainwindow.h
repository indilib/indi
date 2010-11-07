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
    virtual void newDevice(const char *device_name);
    virtual void newProperty(const char *property_name) {}
    virtual void newBLOB(IBLOB *bp);
    virtual void newSwitch(ISwitchVectorProperty *svp);
    virtual void newNumber(INumberVectorProperty *nvp) {}
    virtual void newText(ITextVectorProperty *tvp) {}
    virtual void newLight(ILightVectorProperty *lvp) {}
    virtual void serverConnected() {}
    virtual void serverDisconnected();

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
