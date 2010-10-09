#include <cstdlib>
#include <unistd.h>

#include <QDebug>
#include <QProcess>
#include <QTime>
#include <QTemporaryFile>
#include <QDataStream>

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <libindi/basedevice.h>

#define MAX_FILENAME_LEN    1024

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    BLOBDirty = false;

    kstProcess = new QProcess();

    specTempFile = new QTemporaryFile();

    out = new QDataStream();

    QObject::connect(ui->connectB, SIGNAL(clicked()), this, SLOT(connectServer()));
    QObject::connect(ui->disconnectB, SIGNAL(clicked()), this, SLOT(disconnectServer()));
    QObject::connect(this, SIGNAL(modeUpdated(int)), this, SLOT(updateModeButtons(int)));

}

MainWindow::~MainWindow()
{
    kstProcess->kill();
    delete kstProcess;
    delete specTempFile;
}

void MainWindow::connectServer()
{
    qDebug() << "Connecting ...." << endl;

    QFont cf = ui->connectB->font();
    cf.setBold(true);
    ui->connectB->setFont(cf);

    QFont df = ui->disconnectB->font();
    df.setBold(false);
    ui->disconnectB->setFont(df);


    if (INDI::BaseClient::connect() == false)
        ui->msgQueue->append(QString("KNRO: connection to server on port 7624 is refused..."));

}

void MainWindow::disconnectServer()
{
    qDebug() << "Disconnecting ..." << endl;

    QFont cf = ui->connectB->font();
    cf.setBold(false);
    ui->connectB->setFont(cf);

    QFont df = ui->disconnectB->font();
    df.setBold(true);
    ui->disconnectB->setFont(df);

    INDI::BaseClient::disconnect();

}

void MainWindow::newDevice()
{
    std::vector<INDI::BaseDevice *>::const_iterator devicei;

    for (devicei = getDevices().begin(); devicei != getDevices().end(); devicei++)
            ui->msgQueue->append(QString("We received device %1.").arg((*devicei)->deviceName()));

    setBLOBMode(B_ALSO);

}

void MainWindow::newBLOB(IBLOB *bp)
{
   // IDLog("We recieved BLOB for device %s and size %ld and format %s\n", deviceName, size, format);
   int nr=0,n=0;

  //static QTime bTime;

  //qDebug() << "elapsed time is " << bTime.elapsed() << " ms with format " << bp->format << endl;


    if (currentSMode != QString(bp->format) || BLOBDirty)
    {
        BLOBDirty = false;
        currentSMode = QString(bp->format);
        //specTempFile->close();
        //if (!specTempFile->fileName().isEmpty())
          //  QFile::remove(specTempFile->fileName());

        delete (specTempFile);

        specTempFile = new QTemporaryFile();
        kstProcess->kill();
        kstProcess->waitForFinished();

       qDebug() << "Current format mode is: " << currentSMode << endl;

        specTempFile->setFileTemplate(QString("/tmp/XXXXXX%1").arg(currentSMode));

        //specTempFile->setFileName(file_template);
        if (specTempFile->open() == false)
        {
            qDebug() << "Failed to open temp file!" << endl;
            return;
        }

        out->setDevice(specTempFile);

        qDebug() << "We create a new temp file " << specTempFile->fileName() << endl;

        if (kstProcess->state() == QProcess::NotRunning)
            qDebug() << "kst process is now NOT running" << endl;
        else if (kstProcess->state() == QProcess::Starting)
            qDebug() << "kst process is now starting" << endl;
        else if (kstProcess->state() == QProcess::Running)
            qDebug() << "kst process is now running..." << endl;

    }

    //processEvents();

    for (nr=0; nr < bp->size; nr += n)
        n = out->writeRawData( static_cast<const char *>(bp->blob) + nr, bp->size - nr);

    out->writeRawData( (const char *) "\n" , 1);
    specTempFile->flush();


    if (kstProcess->state() == QProcess::NotRunning)
    {
       // qDebug() << "in process not running yet..." << endl;
        QStringList argList;

        if (currentSMode == ".ascii_cont")
        {
            kstProcess->start(QString("kst -x 1 -y 2 %1").arg(specTempFile->fileName()));
            ui->msgQueue->append(QString("KNRO: Starting continuum channel monitor..."));
            emit modeUpdated(0);
        }
        else
        {
            kstProcess->start(QString("kst -x 3 -y 2 %1").arg(specTempFile->fileName()));
            ui->msgQueue->append(QString("KNRO: Starting spectral channel monitor..."));
            emit modeUpdated(1);

        }

        kstProcess->waitForStarted();

    }

    //bTime.restart();
}

void MainWindow::updateModeButtons(int specMode)
{

    if (specMode == 0)
    {
        QFont cf = ui->contB->font();
        cf.setBold(true);
        ui->contB->setFont(cf);

        QFont df = ui->specB->font();
        df.setBold(false);
        ui->specB->setFont(df);
    }
    else
    {
        QFont cf = ui->contB->font();
        cf.setBold(false);
        ui->contB->setFont(cf);

        QFont df = ui->specB->font();
        df.setBold(true);
        ui->specB->setFont(df);

    }

}

void MainWindow::newSwitch(ISwitchVectorProperty *svp)
{
    if (QString("Scan") == QString(svp->name))
        BLOBDirty = true;
}
