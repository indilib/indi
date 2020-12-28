
#include "practicedevice.h"

#include <memory>
#include <cstring>
#include <cmath>

std::unique_ptr<PracticeDevice> practiceDevice(new PracticeDevice());

/**************************************************************************************
** Return properties of device.
***************************************************************************************/
void ISGetProperties(const char *dev)
{
    practiceDevice->ISGetProperties(dev);
}

/**************************************************************************************
** Process new switch from client
***************************************************************************************/
void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    practiceDevice->ISNewSwitch(dev, name, states, names, n);
}

/**************************************************************************************
** Process new text from client
***************************************************************************************/
void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    practiceDevice->ISNewText(dev, name, texts, names, n);
}

/**************************************************************************************
** Process new number from client
***************************************************************************************/
void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    practiceDevice->ISNewNumber(dev, name, values, names, n);
}

/**************************************************************************************
** Process new blob from client
***************************************************************************************/
void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
{
    practiceDevice->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

/**************************************************************************************
** Process snooped property from another driver
***************************************************************************************/
void ISSnoopDevice(XMLEle *root)
{
    INDI_UNUSED(root);
}

//////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////
bool PracticeDevice::initProperties()
{
    DefaultDevice::initProperties();

    IUFillText(&FirmwareT[FIRMWARE_VERSION], "FIRMWARE_VERSION", "Version", "NA");
    IUFillTextVector(&FirmwareTP, FirmwareT, 1, getDeviceName(), "FIRMWARE_INFO", "Firmware", MAIN_CONTROL_TAB, IP_RO, 60,
                     IPS_IDLE);

    IUFillNumber(&ScaleN[0], "SCALE_NUMBER", "Scale", "%.f", 0, 100, 10, 0);
    IUFillNumberVector(&ScaleNP, ScaleN, 1, getDeviceName(), "SCALE_INFO", "Scale", MAIN_CONTROL_TAB, IP_RO, 60,
                       IPS_IDLE);

    IUFillNumber(&SyncScaleN[0], "SYNC_SCALE", "Sync", "%.f", 0, 100, 1, 0);
    IUFillNumberVector(&SyncScaleNP, SyncScaleN, 1, getDeviceName(), "SYNC_NUMBER", "Sync", MAIN_CONTROL_TAB, IP_RW, 0,
                       IPS_OK);

    IUFillLight(&StatusL[STATUS_FULLY_RELEASED], "FULLY_RELEASED", "Fully Released", IPS_OK);
    IUFillLight(&StatusL[STATUS_RELEASING], "RELEASING", "Releasing", IPS_IDLE);
    IUFillLight(&StatusL[STATUS_HOLDING], "HOLDING", "Holding", IPS_IDLE);
    IUFillLight(&StatusL[STATUS_PRESSING], "PUSHING", "Pushing", IPS_IDLE);
    IUFillLight(&StatusL[STATUS_FULLY_PRESSED], "FULLY_PRESSED", "Fully Pressed", IPS_IDLE);
    IUFillLightVector(&StatusLP, StatusL, 5, getDeviceName(), "STATUS_LIGHT", "Status", MAIN_CONTROL_TAB, IPS_IDLE);

    IUFillSwitch(&PedalActionsS[ACTION_REMOVE_FOOT], "REMOVE_FOOT", "Remove Foot", ISS_ON);
    IUFillSwitch(&PedalActionsS[ACTION_HOLD_FOOT], "HOLD_FOOT", "Hold Foot", ISS_OFF);
    IUFillSwitch(&PedalActionsS[ACTION_PRESS_FOOT], "PUSH_FOOT", "Push Foot", ISS_OFF);
    IUFillSwitchVector(&PedalActionsSP, PedalActionsS, 3, getDeviceName(), "PEDAL_BUTTONS", "Pedal Actions", MAIN_CONTROL_TAB,
                       IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    IUFillSwitch(&TensionLevelsS[TENSION_LOW], "TENSION_LOW", "Low", ISS_ON);
    IUFillSwitch(&TensionLevelsS[TENSION_HIGH], "TENSION_HIGH", "High", ISS_OFF);
    IUFillSwitchVector(&TensionLevelsSP, TensionLevelsS, 2, getDeviceName(), "TENSION_BUTTONS", "Tension Level",
                       MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    return true;
}

//////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////
bool PracticeDevice::updateProperties()
{
    DefaultDevice::updateProperties();

    if (isConnected())
    {
        readDeviceValues();
        defineText(&FirmwareTP);
        defineNumber(&ScaleNP);
        defineNumber(&SyncScaleNP);
        defineLight(&StatusLP);
        defineSwitch(&PedalActionsSP);
        defineSwitch(&TensionLevelsSP);
    }
    else
    {
        deleteProperty(FirmwareTP.name);
        deleteProperty(ScaleNP.name);
        deleteProperty(SyncScaleNP.name);
        deleteProperty(StatusLP.name);
        deleteProperty(PedalActionsSP.name);
        deleteProperty(TensionLevelsSP.name);
    }

    return true;
}
//////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////
bool PracticeDevice::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Pedal Action
        if (!strcmp(name, PedalActionsSP.name))
        {
            IUUpdateSwitch(&PedalActionsSP, states, names, n);
            m_PedalState = static_cast<PedalState>(IUFindOnSwitchIndex(&PedalActionsSP));
            PedalActionsSP.s = IPS_OK;
            IDSetSwitch(&PedalActionsSP, nullptr);
            return true;

        }

        //////////////////////////////////////////////////////////////////
        /// Tenson Stuff
        //////////////////////////////////////////////////////////////////
        if (!strcmp(name, TensionLevelsSP.name))
        {
            TensionState prevTension = m_TensionState;

            IUUpdateSwitch(&TensionLevelsSP, states, names, n);
            m_TensionState = static_cast<TensionState>(IUFindOnSwitchIndex(&TensionLevelsSP));
            TensionLevelsSP.s = IPS_OK;
            IDSetSwitch(&TensionLevelsSP, nullptr);

            if (m_TensionState == TENSION_LOW && prevTension != m_TensionState)
            {
                ScaleN->step = 10;
                IDSetNumber(&ScaleNP, nullptr);
            }
            if (m_TensionState == TENSION_HIGH && prevTension != m_TensionState)
            {
                ScaleN->step = 5;
                IDSetNumber(&ScaleNP, nullptr);
            }

            return true;
        }
    }
    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

//////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////
bool PracticeDevice::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        if (!strcmp(name, SyncScaleNP.name))
        {
            if (SyncScaleN[0].value != values[0])
            {
                SyncScaleN[0].value = values[0];
                ScaleN[0].value = values[0];

                SyncScaleNP.s = IPS_OK;
                IDSetNumber(&SyncScaleNP, nullptr);
                IDSetNumber(&ScaleNP, nullptr);
            }

            return true;
        }

    }
    return DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

//////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////
void PracticeDevice::TimerHit()
{
    if (isConnected() == false)
        return;

    double prevScale = ScaleN[0].value;

    switch(m_PedalState)
    {
        case ACTION_PRESS_FOOT:
            ScaleN[0].value += ScaleN[0].step;
            ScaleN[0].value = std::min(ScaleN[0].value, ScaleN[0].max);

            if  (ScaleN[0].value == ScaleN[0].max)
            {
                if (StatusL[STATUS_FULLY_PRESSED].s == IPS_IDLE)
                    updateStatus(StatusL[STATUS_FULLY_PRESSED]);
            }
            else
            {
                if (StatusL[STATUS_PRESSING].s == IPS_IDLE)
                    updateStatus(StatusL[STATUS_PRESSING]);
            }
            break;

        case ACTION_HOLD_FOOT:

            if (StatusL[STATUS_HOLDING].s == IPS_IDLE)
                updateStatus(StatusL[STATUS_HOLDING]);
            break;

        case ACTION_REMOVE_FOOT:
            ScaleN[0].value -= ScaleN[0].step;
            ScaleN[0].value = std::max(ScaleN[0].value, ScaleN[0].min);


            if (ScaleN[0].value == ScaleN[0].min)
            {
                if (StatusL[STATUS_FULLY_RELEASED].s == IPS_IDLE)
                    updateStatus(StatusL[STATUS_FULLY_RELEASED]);
            }

            else
            {
                if (StatusL[STATUS_RELEASING].s == IPS_IDLE)
                    updateStatus(StatusL[STATUS_RELEASING]);
            }
            break;
    }

    if (std::fabs(prevScale - ScaleN[0].value) > 0)
    {
        IDSetNumber(&ScaleNP, nullptr);
    }

    SetTimer(POLLMS);
}
//////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////
bool PracticeDevice::readDeviceValues()
{
    IUSaveText(&FirmwareT[FIRMWARE_VERSION], "1.1");

    return true;
}
/**************************************************************************************
** Client is asking us to establish connection to the device
***************************************************************************************/
bool PracticeDevice::Connect()
{
    LOG_INFO("Simple device connected successfully!");

    SetTimer(POLLMS);

    return true;
}

/**************************************************************************************
** Client is asking us to terminate connection to the device
***************************************************************************************/
bool PracticeDevice::Disconnect()
{
    LOG_INFO("Simple device disconnected successfully!");
    return true;
}

/**************************************************************************************
** INDI is asking us for our default device name
***************************************************************************************/
const char *PracticeDevice::getDefaultName()
{
    return "Practice Device";
}

//////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////
void PracticeDevice::resetStatus()
{
    StatusL[STATUS_FULLY_RELEASED].s = IPS_IDLE;
    StatusL[STATUS_RELEASING].s = IPS_IDLE;
    StatusL[STATUS_HOLDING].s = IPS_IDLE;
    StatusL[STATUS_PRESSING].s = IPS_IDLE;
    StatusL[STATUS_FULLY_PRESSED].s = IPS_IDLE;
}

//////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////
void PracticeDevice::updateStatus(ILight &Pstate)
{
    resetStatus();
    Pstate.s = IPS_OK;
    IDSetLight(&StatusLP, nullptr);
}
