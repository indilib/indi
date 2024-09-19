
#include "Excalibur.h"

#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <cerrno>
#include <cstring>
#include <memory>
#include <termios.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/ioctl.h>
#include <math.h>
#define CLOSE_CMD "S1#"
#define OPEN_CMD "S0#"

static std::unique_ptr<Excalibur> flatmaster(new Excalibur());


Excalibur::Excalibur() : LightBoxInterface(this), DustCapInterface(this)
{
    setVersion(1, 0);
}

bool Excalibur::initProperties()
{
    INDI::DefaultDevice::initProperties();

    IUFillText(&StatusT[0], "Cover", "Cover", nullptr);
    IUFillText(&StatusT[1], "Light", "Light", nullptr);
    IUFillTextVector(&StatusTP, StatusT, 2, getDeviceName(), "Status", "Status", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    DI::initProperties(MAIN_CONTROL_TAB);
    LI::initProperties(MAIN_CONTROL_TAB, CAN_DIM);

    setDriverInterface(AUX_INTERFACE | LIGHTBOX_INTERFACE | DUSTCAP_INTERFACE);

    LightIntensityNP[0].setMin(0);
    LightIntensityNP[0].setMax(3000);
    LightIntensityNP[0].setStep(100);

    addAuxControls();

    serialConnection = new Connection::Serial(this);
    serialConnection->setDefaultBaudRate(Connection::Serial::B_9600);
    serialConnection->registerHandshake([&]()
    {
        return Ack();
    });


    registerConnection(serialConnection);
    return true;
}


bool Excalibur::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    DI::updateProperties();
    LI::updateProperties();

    if (isConnected())
    {
        defineProperty(&StatusTP);

    }
    else
    {
        deleteProperty(StatusTP.name);

    }

    return true;
}

const char *Excalibur::getDefaultName()
{
    return "RBF Excalibur";
}



bool Excalibur::Ack()
{
    PortFD = serialConnection->getPortFD();

    char response[16] = {0};
    if(sendCommand("#", response))
    {
        if(strstr("FLAT.FLAP!#", response) != nullptr)
        {
            LightSP[1].setState(ISS_ON);
            LightSP[0].setState(ISS_OFF);
            LightSP.apply();
            deviceStatus();
            return true;

        }
    }
    else
    {
        LOG_ERROR("Ack failed.");
        return false;
    }

    return false;
}

bool Excalibur::EnableLightBox(bool enable)
{
    char response[20] = {0};
    char cmd[16] = {0};
    if (!enable)
    {
        snprintf(cmd, 16, "L%d##", 0);

        sendCommand(cmd, response);
        IUSaveText(&StatusT[1], "Off");
        IDSetText(&StatusTP, nullptr);
        return true;



    }
    else
    {
        snprintf(cmd, 16, "L%d##", (int)LightIntensityNP[0].getValue());

        sendCommand(cmd, response);
        IUSaveText(&StatusT[1], "On");
        IDSetText(&StatusTP, nullptr);
        return true;


    }

    return false;
}

bool Excalibur::SetLightBoxBrightness(uint16_t value)
{
    if(LightSP[FLAT_LIGHT_ON].getState() != ISS_ON)
    {
        LOG_ERROR("You must set On the Flat Light first.");
        return false;
    }
    if( ParkCapSP[0].getState() != ISS_ON)
    {
        LOG_ERROR("You must Park eXcalibur first.");
        return false;
    }

    //char response[20] = {0};
    char cmd[DRIVER_RES] = {0};

    snprintf(cmd, 30, "L%d##", value);
    sendCommand(cmd);
    return  true;



}
IPState Excalibur::ParkCap()
{
    sendCommand("S1#");

    ParkCapSP.reset();
    ParkCapSP[0].setState(ISS_ON);
    ParkCapSP.setState(IPS_OK);
    LOG_INFO("Cover closed.");
    ParkCapSP.apply();
    return IPS_OK;
}

IPState Excalibur::UnParkCap()
{
    sendCommand("S0#");
    // Set cover status to random value outside of range to force it to refresh
    IUSaveText(&StatusT[1], "Off");
    ParkCapSP.reset();
    ParkCapSP[1].setState(ISS_ON);
    ParkCapSP.setState(IPS_OK);
    LOG_INFO("Cover open.");
    ParkCapSP.apply();
    IDSetText(&StatusTP, nullptr);
    return IPS_OK;
}

void Excalibur::TimerHit()
{
    if (!isConnected())
        return;

    deviceStatus();

    // parking or unparking timed out, try again
    if (ParkCapSP.getState() == IPS_BUSY && !strcmp(StatusT[0].text, "Timed out"))
    {
        if (ParkCapSP[0].getState() == ISS_ON)
            ParkCap();
        else
            UnParkCap();
    }

    SetTimer(getCurrentPollingPeriod());
}

bool Excalibur::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (LI::processNumber(dev, name, values, names, n))
        return true;

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool Excalibur::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (LI::processText(dev, name, texts, names, n))
            return true;
    }

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool Excalibur::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (DI::processSwitch(dev, name, states, names, n))
            return true;
        if (LI::processSwitch(dev, name, states, names, n))
            return true;
    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool Excalibur::ISSnoopDevice(XMLEle *root)
{
    LI::snoop(root);

    return INDI::DefaultDevice::ISSnoopDevice(root);
}

bool Excalibur::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);

    return LI::saveConfigItems(fp);
}

bool Excalibur::sendCommand(const char *command, char *res)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    char cmd[20] = {0};
    snprintf(cmd, 20, "%s", command);

    LOGF_DEBUG("CMD <%#02X>", cmd[0]);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial write error: %s.", errstr);
        return false;
    }
    if (res == nullptr)
        return true;

    if ((rc = tty_read_section(PortFD, res, DRIVER_DEL, 5, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("command: %s error: %s.", cmd, errstr);
        return  false;
    }

    // Remove the #
    res[nbytes_read - 1] = 0;

    LOGF_DEBUG("RES <%s>", res);

    tcflush(PortFD, TCIOFLUSH);

    return true;
}
void Excalibur::deviceStatus()
{
    char res[DRIVER_RES] = {0};

    sendCommand("O#", res);

    int32_t pos;
    int rc = sscanf(res, "%d#", &pos);

    if (rc > 0)
    {
        LightIntensityNP[0].setValue(pos);
        LightIntensityNP.apply();
    }

    if(LightIntensityNP[0].getValue() > 0)
    {
        IUSaveText(&StatusT[1], "On");
        if (LightSP[1].getState() == ISS_ON)
        {
            LightSP[0].setState(ISS_ON);
            LightSP[1].setState(ISS_OFF);
            LightSP.apply();
        }

    }
    else
    {
        IUSaveText(&StatusT[1], "Off");
    }

    sendCommand("P#", res);

    int32_t pos2;
    sscanf(res, "%d#", &pos2);


    if(pos2 <= 0)
    {
        IUSaveText(&StatusT[0], "Closed");
        if (ParkCapSP[0].getState() == ISS_OFF)
        {
            ParkCapSP[0].setState(ISS_ON);
            LightSP.apply();
        }
    }
    else
    {
        IUSaveText(&StatusT[0], "Open");
        if (ParkCapSP[1].getState() == ISS_OFF)
        {
            ParkCapSP[1].setState(ISS_ON);
            IUSaveText(&StatusT[1], "Off");
            LightSP.apply();
        }

    }
    IDSetText(&StatusTP, nullptr);

}
