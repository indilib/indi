/*******************************************************************************
  Driver type: TitanTCS for HOBYM CRUX Mount INDI Driver

  Copyright(c) 2020 Park Suyoung <hparksy@gmail.com>. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include <cstring>

#include "crux_mount.h"

#include "indicom.h"

#include <cmath>
#include <memory>

#define PRODUCT_NAME    "TitanTCS CRUX"
#define HANDSHAKE_NAME  "TiTaN TCS"
#define MIN_FW_VERSION  "3.1.0"
#define MAX_CMD_LEN     256

static std::unique_ptr<TitanTCS> titanTCS(new TitanTCS());

TitanTCS::TitanTCS()
{
    setVersion(1, 0);

    SetTelescopeCapability(
        TELESCOPE_CAN_GOTO |
        TELESCOPE_CAN_SYNC |
        TELESCOPE_CAN_PARK |
        TELESCOPE_CAN_ABORT |
        TELESCOPE_HAS_TIME |
        TELESCOPE_HAS_LOCATION |
        //TELESCOPE_HAS_PIER_SIDE |
#if USE_PEC
        TELESCOPE_HAS_PEC |
#endif
        TELESCOPE_HAS_TRACK_MODE |
        TELESCOPE_CAN_CONTROL_TRACK
        //TELESCOPE_HAS_TRACK_RATE
        , 4);

    SetParkDataType(PARK_HA_DEC);

    LOG_INFO("Initializing from " PRODUCT_NAME " device...");

    m_Connect = 0;
}

bool TitanTCS::Connect()
{
    m_Connect = 1;

    bool bResult = Telescope::Connect();
    LOGF_DEBUG("Connect() => %s", (bResult ? "true" : "false"));

    if(bResult)
        m_Connect = 2;
    else
        m_Connect = -1;

    return bResult;
}

bool TitanTCS::Disconnect()
{
    m_Connect = 0;

    bool bResult = Telescope::Disconnect();
    LOGF_DEBUG("Disconnect() => %s", (bResult ? "true" : "false"));

    return bResult;
}

/**************************************************************************************
** We init our properties here. The only thing we want to init are the Debug controls
***************************************************************************************/
bool TitanTCS::initProperties()
{
#if USE_PEC
    _PECStatus = -1;
#endif
    //
    INDI::Telescope::initProperties();

    initGuiderProperties(getDeviceName(), GUIDE_TAB);
    setDriverInterface(getDriverInterface() | GUIDER_INTERFACE);

    AddTrackMode("TRACK_SIDEREAL", "Sidereal");
    AddTrackMode("TRACK_SOLAR", "Solar");
    AddTrackMode("TRACK_LUNAR", "Lunar");

    addDebugControl();

#if USE_PEC
    // PEC Training
    IUFillSwitch(&PECTrainingS[0], "PEC_Start", "Start", ISS_OFF);
    IUFillSwitch(&PECTrainingS[1], "PEC_Stop", "Stop", ISS_OFF);
    IUFillSwitchVector(&PECTrainingSP, PECTrainingS, 2, getDeviceName(), "PEC_TRAINING", "PEC Training", MOTION_TAB, IP_RW,
                       ISR_1OFMANY, 0,
                       IPS_IDLE);

    // PEC Details
    IUFillText(&PECInfoT[0], "PEC_INFO", "PEC", "");
    IUFillText(&PECInfoT[1], "PEC_TR_INFO", "Training", "");
    IUFillTextVector(&PECInfoTP, PECInfoT, 2, getDeviceName(), "PEC_INFOS", "PEC Info", MOTION_TAB,
                     IP_RO, 60, IPS_IDLE);
#endif
    // Mount Details
    IUFillText(&MountInfoT[0], "MOUNT_PARK", "Park", "");
    IUFillText(&MountInfoT[1], "MOUNT_TRACKING", "Tracking", "");
    IUFillTextVector(&MountInfoTP, MountInfoT, 2, getDeviceName(), "MOUNT_INFOS", "Mount Info", MAIN_CONTROL_TAB,
                     IP_RO, 60, IPS_IDLE);

    TrackState = SCOPE_IDLE;

    return true;
}


bool TitanTCS::updateProperties()
{
    INDI::Telescope::updateProperties();

    if (isConnected())
    {
#if USE_PEC
        defineProperty(&PECTrainingSP);
        defineProperty(&PECInfoTP);
#endif
        defineProperty(&MountInfoTP);
        //
        defineProperty(&GuideNSNP);
        defineProperty(&GuideWENP);
        //
        IUResetSwitch(&TrackModeSP);
        TrackModeS[TRACK_SIDEREAL].s = ISS_ON;
        TrackState = SCOPE_TRACKING;
        //
        GetMountParams();
    }
    else
    {
#if USE_PEC
        deleteProperty(PECTrainingSP.name);
        deleteProperty(PECInfoTP.name);
#endif
        deleteProperty(MountInfoTP.name);
        //
        deleteProperty(GuideNSNP.name);
        deleteProperty(GuideWENP.name);
    }

    return true;
}

bool TitanTCS::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        /*
        if (strcmp(name, "JOG_RATE") == 0) {
            IUUpdateNumber(&JogRateNP, values, names, n);
            JogRateNP.s = IPS_OK;
            IDSetNumber(&JogRateNP, nullptr);
            return true;
        }
        if (strcmp(name, GuideRateNP.name) == 0) {
            IUUpdateNumber(&GuideRateNP, values, names, n);
            GuideRateNP.s = IPS_OK;
            IDSetNumber(&GuideRateNP, nullptr);
            return true;
        }
        */
        if (strcmp(name, GuideNSNP.name) == 0 || strcmp(name, GuideWENP.name) == 0)
        {
            LOGF_DEBUG("%s = %g", name, values[0]);
            processGuiderProperties(name, values, names, n);
            return true;
        }
    }

    return INDI::Telescope::ISNewNumber(dev, name, values, names, n);
}

bool TitanTCS::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        int iVal = 0;
        // ---------------------------------------------------------------------
        // Park $$$
        if (!strcmp(name, ParkSP.name))
        {
            IUUpdateSwitch(&ParkSP, states, names, n);
            int nowIndex = IUFindOnSwitchIndex(&ParkSP);
            if(nowIndex == 0)
                Park();
            else if(nowIndex == 1)
                UnPark();

            if(CommandResponse("#:hP?#", "$hP", '#', NULL, &iVal))
            {
                if(TrackState == SCOPE_PARKING)
                {
                    if(iVal == 2)
                        TrackState = SCOPE_PARKED;
                    else if(iVal == 0)
                        TrackState = SCOPE_IDLE;
                }
                else if(TrackState == SCOPE_PARKED)
                {
                    if(iVal == 0)
                        TrackState = SCOPE_IDLE;
                }
            }

            return true;
        }
        //
#if USE_PEC
        if (!strcmp(name, PECStateSP.name))
        {
            //int preIndex = IUFindOnSwitchIndex(&PECStateSP);
            IUUpdateSwitch(&PECStateSP, states, names, n);
            int nowIndex = IUFindOnSwitchIndex(&PECStateSP);

            IDSetSwitch(&PECStateSP, nullptr);

            if(nowIndex == 0)
            {
                SendCommand("#:\\e10#:\\e11#");
            }
            else if(nowIndex == 1)
            {
                SendCommand("#:\\e12#");
            }
            return true;
        }
        //
        if (!strcmp(name, PECTrainingSP.name))
        {
            //int preIndex = IUFindOnSwitchIndex(&PECTrainingSP);
            IUUpdateSwitch(&PECTrainingSP, states, names, n);
            int nowIndex = IUFindOnSwitchIndex(&PECTrainingSP);

            IDSetSwitch(&PECTrainingSP, nullptr);

            if(nowIndex == 0)
            {
                SendCommand("#:\\e20#:\\e21#");
            }
            else if(nowIndex == 1)
            {
                SendCommand("#:\\e23#");
            }
            return true;
        }
#endif
        //
        if (!strcmp(name, TrackStateSP.name))
        {
            IUUpdateSwitch(&TrackStateSP, states, names, n);
            int nowIndex = IUFindOnSwitchIndex(&TrackStateSP);

            IDSetSwitch(&TrackStateSP, nullptr);

            if(nowIndex == 0)
            {
                SetTrackEnabled(true);
            }
            else if(nowIndex == 1)
            {
                SetTrackEnabled(false);
            }
            return true;
        }
        //
        if (!strcmp(name, TrackModeSP.name))
        {
            IUUpdateSwitch(&TrackModeSP, states, names, n);
            int nowIndex = IUFindOnSwitchIndex(&TrackModeSP);

            IDSetSwitch(&TrackModeSP, nullptr);

            SetTrackMode(nowIndex);

            return true;
        }
    }

    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}

bool TitanTCS::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    return INDI::Telescope::ISNewText(dev, name, texts, names, n);
}

/**************************************************************************************
** INDI is asking us to check communication with the device via a handshake
***************************************************************************************/
bool TitanTCS::Handshake()
{
    if(!isConnected())
        return true;

    char szResponse[MAX_CMD_LEN] = { 0 };

    if(CommandResponseStr("#:GVP#", "", '#', szResponse, sizeof(szResponse)))
    {
        LOGF_INFO("Product Name = '%s'", szResponse);
        if(strstr(szResponse, HANDSHAKE_NAME))
        {
            if(CommandResponseStr("#:GVN#", "", '#', szResponse, sizeof(szResponse)))
            {
                LOGF_INFO("Firmware Version = '%s'", szResponse);
                int comp = strcmp(szResponse, MIN_FW_VERSION);
                if(comp >= 0)
                    return true;
                else
                {
                    LOGF_ERROR("Firmware version '%s' is too old. Required > %s", szResponse, MIN_FW_VERSION);
                }
            }
            else
            {
                LOG_ERROR("The firmware version cannot be read.");
            }
        }
        else
        {
            LOGF_ERROR("TitanTCS could not be found. return code = '%s'", szResponse);
        }
    }

    LOG_ERROR("Handshake() failed!");

    //Disconnect();
    return false;
}

static int Char2Num(char chr)
{
    if((chr >= '0') && (chr <= '9'))
        return chr - '0';

    if((chr >= 'A') && (chr <= 'F'))
        return chr - 'A' + 10;

    if((chr >= 'a') && (chr <= 'f'))
        return chr - 'a' + 10;

    return 0;
}

static int GetDigitParam(const char *pStr, int* pDigit, char* pDelimeter = nullptr)
{
    /*
    HH:MM.T#
    HH:MM:SS#
    */

    int cntParam = 0;
    short	sign = 1;
    int i, chrCnt;

    while (*pStr == ' ' || *pStr == '+')
    {
        ++pStr;
        if(*pStr == 0)
        {
            return 0;
        }
    }

    if (*pStr == '-')
    {
        sign = -1;
        ++pStr;
    }

    pDigit[cntParam] = sign;
    if(pDelimeter)
        pDelimeter[cntParam] = 0;
    cntParam++;

    for(i = 0; i < 3; i++)
    {
        pDigit[cntParam] = 0;
        chrCnt = 0;
        for(;;)
        {
            if(*pStr == 0)
            {
                if(chrCnt > 0)
                    cntParam++;
                return cntParam;
            }

            if(!isdigit(*pStr))
            {
                if(pDelimeter)
                    pDelimeter[cntParam] = *pStr;
                break;
            }

            pDigit[cntParam] *= 10;
            pDigit[cntParam] += Char2Num(*pStr);

            chrCnt++;
            pStr++;
        }
        cntParam++;

        if(*pStr == 0)
            return cntParam;

        if(!isdigit(*pStr))
        {
            pStr++;
        }
    }

    return cntParam;
}

// 00:02:43
static bool HMS2Hour(const char* pStr, double* hr)
{
    int digit[5] = { 0, 0, 0, 0, 0 };
    int rtnCode = GetDigitParam(pStr, digit);

    int sec = 0;
    for(int i = 1; i < 4; i++)
    {
        sec *= 60;
        sec += digit[i];
    }

    if(digit[0] < 0)
        sec = -sec;

    *hr = sec / 3600.0;
    return rtnCode >= 3;
}

static void formatRA(long secRa, char* pStr)
{
    int sign = 1;
    if(secRa < 0)
    {
        sign = -1;
        secRa = -secRa;
    }

    int h = secRa / 3600;
    secRa -= (h * 3600);

    int m = secRa / 60;
    secRa -= (m * 60);

    int s = secRa;

    if(sign < 0)
        sprintf(pStr, "-%02d:%02d:%02d", h, m, s);
    else
        sprintf(pStr, "%02d:%02d:%02d", h, m, s);
}

static void formatDEC(long secDec, char* pStr)
{
    if (secDec > (270 * 3600))
        secDec = secDec - (360 * 3600);
    else if (secDec > (90 * 3600))
        secDec = (180 * 3600) - secDec;

    char sign;
    if (secDec >= 0)
    {
        sign = '+';
    }
    else
    {
        sign = '-';
        secDec = -secDec;
    }

    int h = secDec / 3600;
    secDec -= (h * 3600);

    int m = secDec / 60;
    secDec -= (m * 60);

    int s = secDec;

    sprintf(pStr, "%c%02d*%02d:%02d", sign, h, m, s);
}

/**************************************************************************************
** Guiding
***************************************************************************************/
IPState TitanTCS::GuideNorth(uint32_t ms)
{
    SendCommand(":Mgn%d#", (int)ms);
    //
    if(MovementNSSP.s == IPS_BUSY)
        return IPS_ALERT;

    if (GuideNSTID)
    {
        IERmTimer(GuideNSTID);
        GuideNSTID = 0;
    }

    GuideNSTID = IEAddTimer(static_cast<int>(ms), guideTimeoutHelperNS, this);
    return IPS_BUSY;
}

IPState TitanTCS::GuideSouth(uint32_t ms)
{
    SendCommand(":Mgs%d#", (int)ms);
    //
    if(MovementNSSP.s == IPS_BUSY)
        return IPS_ALERT;

    if (GuideNSTID)
    {
        IERmTimer(GuideNSTID);
        GuideNSTID = 0;
    }

    GuideNSTID = IEAddTimer(static_cast<int>(ms), guideTimeoutHelperNS, this);
    return IPS_BUSY;
}

IPState TitanTCS::GuideEast(uint32_t ms)
{
    SendCommand(":Mge%d#", (int)ms);
    //
    if(MovementWESP.s == IPS_BUSY)
        return IPS_ALERT;

    if (GuideWETID)
    {
        IERmTimer(GuideWETID);
        GuideWETID = 0;
    }

    GuideWETID = IEAddTimer(static_cast<int>(ms), guideTimeoutHelperWE, this);
    return IPS_BUSY;
}

IPState TitanTCS::GuideWest(uint32_t ms)
{
    SendCommand(":Mgw%d#", (int)ms);
    //
    if(MovementWESP.s == IPS_BUSY)
        return IPS_ALERT;

    if (GuideWETID)
    {
        IERmTimer(GuideWETID);
        GuideWETID = 0;
    }

    GuideWETID = IEAddTimer(static_cast<int>(ms), guideTimeoutHelperWE, this);
    return IPS_BUSY;
}

/**************************************************************************************
** INDI is asking us for our default device name
***************************************************************************************/
const char *TitanTCS::getDefaultName()
{
    return PRODUCT_NAME;
}

/**************************************************************************************
** Client is asking us to slew to a new position
***************************************************************************************/
bool TitanTCS::Goto(double ra, double dec)
{
    if(!SetTarget(ra, dec))
        return false;

    char rtnCode;
    char szCommand[MAX_CMD_LEN];
    sprintf(szCommand, ":MS#");

    if(!CommandResponseChar(szCommand, "", &rtnCode))
    {
        LOG_ERROR("Goto / No response");
        return false;
    }
    if(rtnCode != '0')
    {
        LOGF_ERROR("Goto / Error Code = '%c'", rtnCode);
        return false;
    }

    TrackState = SCOPE_SLEWING;

    LOG_INFO("Slewing ...");
    return true;
}

/**************************************************************************************
** Client is asking us to abort our motion
***************************************************************************************/
bool TitanTCS::Abort()
{
    if(TrackState == SCOPE_PARKING)
        UnPark();

    LOG_DEBUG("Abort()");
    return SendCommand("#:Q#");
}

/**************************************************************************************
** Client is asking us to report telescope status
***************************************************************************************/
bool TitanTCS::ReadScopeStatus()
{
    LOGF_DEBUG("ReadScopeStatus(s %d)", TrackState);

    GetMountParams();

    return true;
}

//------------------------------------------------------------------------------
bool TitanTCS::GetParamStr(const char* pInStr, char* pOutStr, int len, const char* pResponse, char delimeter)
{
    //LOGF_DEBUG("GetParamStr('%s', '%s', '%c')", pInStr, pResponse, (delimeter == 0 ? '0' : delimeter));

    if((pResponse != NULL) && (*pResponse == 0))
        pResponse = NULL;

    if(pResponse)
    {
        const char* pFind = strstr(pInStr, pResponse);
        if(pFind == 0)
        {
            LOG_ERROR("Fail!");
            return false;
        }
        int l = strlen(pResponse);
        pInStr = pFind + l;
        len    -= l;
    }

    if(len <= 0)
        return false;

    for(int i = 0; i < 3; i++)
    {
        if(*pInStr == ' ')
        {
            pInStr++;
            len--;

            if(len <= 0)
                return false;
        }
        else
            break;
    }

    strncpy(pOutStr, pInStr, len);

    if(delimeter)
    {
        char* pDel = strchr(pOutStr, delimeter);
        if(pDel == 0)
        {
            LOG_ERROR("Fail!");
            return false;
        }

        *pDel = 0;
    }

    LOG_DEBUG(pOutStr);

    return true;
}

bool TitanTCS::GetParamNumber(const char* pInStr, char* pOutStr, int len, const char* pResponse, char delimeter,
                              double *pDouble, int *pInteger)
{
    if(!GetParamStr(pInStr, pOutStr, len, pResponse, delimeter))
        return false;

    if(pDouble)
        *pDouble = atof(pOutStr);
    if(pInteger)
        *pInteger = atoi(pOutStr);

    return true;
}

bool TitanTCS::GetParamHour(const char* pInStr, char* pOutStr, int len, const char* pResponse, char delimeter,
                            double *pHour)
{
    if(!GetParamStr(pInStr, pOutStr, len, pResponse, delimeter))
        return false;

    //LOGF_DEBUG("pResult = '%s'", pOutStr);
    if(HMS2Hour(pOutStr, pHour))
        return true;

    return false;
}

// -----------------------------------------------------------------------------
bool TitanTCS::SendCommand(const char *cmd)
{
    if (isSimulation())
        return false;

    // tcflush(PortFD, TCIOFLUSH);  // Error with Bluetooth!
    ReadFlush();

    int nbytes_written = 0;
    int err_code;

    if ((err_code = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written) != TTY_OK))
    {
        char titanfocus_error[256];
        tty_error_msg(err_code, titanfocus_error, 256);
        LOGF_ERROR("tty_write() error detected: %s", titanfocus_error);
        return false;
    }

    // tcflush(PortFD, TCIOFLUSH);  // Error with Bluetooth!

    return true;
}

bool TitanTCS::SendCommand(const char *cmd, int val)
{
    char szBuff[128];
    sprintf(szBuff, cmd, val);

    return SendCommand(szBuff);
}

bool TitanTCS::SendCommand(const char *cmd, double val)
{
    char szBuff[128];
    sprintf(szBuff, cmd, val);

    return SendCommand(szBuff);
}
// -----------------------------------------------------------------------------
void TitanTCS::ReadFlush()
{
    if (isSimulation())
        return;

    char buff[256];
    int bytesRead = 0;

    // tcflush(PortFD, TCIOFLUSH);  // Error with Bluetooth!

    for(int i = 0; i < 3; i++)
    {
        int err_code;
        if ((err_code = tty_read(PortFD, buff, sizeof(buff) - 1, 0, &bytesRead)) != TTY_OK)
        {
            return;
        }
        if(bytesRead <= 0)
            return;

        buff[bytesRead] = 0;
        LOGF_DEBUG("Buffer Flush '%s'", buff);
    }
}

int TitanTCS::ReadResponse(char *buf, int len, char delimeter, int timeout)
{
    if (isSimulation())
        return 0;

    *buf = 0;
    int bytesRead = 0;
    int recv_len = 0;

    // tcflush(PortFD, TCIOFLUSH);  // Error with Bluetooth!

    for(int i = 0; i < len; i++)
    {
        int err_code = tty_read(PortFD, buf + recv_len, 1, timeout, &bytesRead);
        if (err_code != TTY_OK)
        {
            //if(err_code == TTY_TIME_OUT)
            //     continue;

            char titanfocus_error[256] = {0};
            tty_error_msg(err_code, titanfocus_error, 256);
            LOGF_ERROR("tty_read() error detected: '%s' len %d, %s", buf, recv_len, titanfocus_error);
            return -1;
        }

        char read_ch = buf[recv_len];
        recv_len++;
        buf[recv_len] = 0;

        if(delimeter == 0)
        {
            if(recv_len >= len)
            {
                return recv_len;
            }
        }
        else
        {
            if((recv_len + 1) >= len)
            {
                LOGF_ERROR("TTY error detected: overflow %d, %d", recv_len, len);
                return 0;
            }
        }

        if(delimeter)
        {
            if(read_ch == delimeter)
            {
                return recv_len;
            }
        }
    }

    return -1;
}
// -----------------------------------------------------------------------------
bool TitanTCS::CommandResponse(const char* pCommand, const char* pResponse, char delimeter, double *pDouble, int *pInteger)
{
    ReadFlush();
    if (!SendCommand(pCommand))
        return false;

    char szResponse[MAX_CMD_LEN] = { 0 };
    char szResult[MAX_CMD_LEN] = { 0 };

    int rd_count = ReadResponse(szResponse, sizeof(szResponse), delimeter);
    if (rd_count <= 0)
    {
        LOGF_ERROR("No response '%s'", pCommand);
        return false;
    }
    LOGF_DEBUG("ReadResponse('%s')", szResponse);

    if(!GetParamNumber(szResponse, szResult, sizeof(szResult) - 1, pResponse, delimeter, pDouble, pInteger))
    {
        LOGF_DEBUG("CommandResponse('%s', '%s') Fail!", pCommand, pResponse);
        return false;
    }

    return true;
}

bool TitanTCS::CommandResponseHour(const char* pCommand, const char* pResponse, char delimeter, double* Hour)
{
    ReadFlush();
    if (!SendCommand(pCommand))
        return false;

    char szResponse[MAX_CMD_LEN] = { 0 };
    char szResult[MAX_CMD_LEN] = { 0 };

    int rd_count = ReadResponse(szResponse, sizeof(szResponse), delimeter);
    if (rd_count <= 0)
    {
        LOGF_ERROR("No response '%s'", pCommand);
        return false;
    }
    LOGF_DEBUG("ReadResponse('%s')", szResponse);

    if(!GetParamHour(szResponse, szResult, sizeof(szResult) - 1, pResponse, delimeter, Hour))
    {
        LOGF_DEBUG("CommandResponseHour('%s', '%s') Fail!", pCommand, pResponse);
        return false;
    }

    return true;
}

bool TitanTCS::CommandResponseStr(const char* pCommand, const char* pResponse, char delimeter, char* pReturn, int len)
{
    ReadFlush();
    if (!SendCommand(pCommand))
        return false;

    char szResponse[MAX_CMD_LEN * 2] = { 0 };

    int rd_count = ReadResponse(szResponse, sizeof(szResponse), delimeter);
    if (rd_count <= 0)
    {
        LOGF_ERROR("No response '%s'", pCommand);
        return false;
    }
    LOGF_DEBUG("ReadResponse('%s')", szResponse);

    if(!GetParamStr(szResponse, pReturn, len, pResponse, delimeter))
    {
        LOGF_DEBUG("CommandResponseStr('%s', '%s') Fail!", pCommand, pResponse);
        return false;
    }

    LOGF_DEBUG("%s : %s", pCommand, pReturn);

    return true;
}

bool TitanTCS::CommandResponseChar(const char* pCommand, const char* pResponse, char* pReturn)
{
    ReadFlush();
    if (!SendCommand(pCommand))
        return false;

    char szResponse[MAX_CMD_LEN] = { 0 };
    char szResult[MAX_CMD_LEN] = { 0 };

    int rd_count = ReadResponse(szResponse, 1, 0);
    if (rd_count <= 0)
    {
        LOGF_ERROR("No response '%s'", pCommand);
        return false;
    }
    //LOGF_DEBUG("ReadResponse('%s')", szResponse);

    if(!GetParamStr(szResponse, szResult, sizeof(szResult) - 1, pResponse, 0))
    {
        LOGF_DEBUG("CommandResponseChar('%s', '%s') Fail!", pCommand, pResponse);
        return false;
    }

    *pReturn = szResult[0];

    LOGF_DEBUG("%s : %c", pCommand, *pReturn);

    return true;
}

bool TitanTCS::SetTarget(double ra, double dec)
{
    char szRA[MAX_CMD_LEN], szDEC[MAX_CMD_LEN];
    formatRA(ra * 3600, szRA);
    formatDEC(dec * 3600, szDEC);

    char rtnCode = 0;
    char szCommand[MAX_CMD_LEN * 2];

    sprintf(szCommand, "#:Sr %s#", szRA);
    if(!CommandResponseChar(szCommand, "", &rtnCode))
    {
        LOG_ERROR("SetTarget RA / No response");
        return false;
    }
    if(rtnCode != '1')
    {
        LOGF_ERROR("SetTarget DEC / Error Code = '%c'", rtnCode);
        return false;
    }

    sprintf(szCommand, "#:Sd %s#", szDEC);
    if(!CommandResponseChar(szCommand, "", &rtnCode))
    {
        LOG_ERROR("SetTarget DEC / No response");
        return false;
    }
    if(rtnCode != '1')
    {
        LOGF_ERROR("SetTarget DEC / Error Code = '%c'", rtnCode);
        return false;
    }

    LOGF_INFO("Set target RA:%s, DEC:%s", szRA, szDEC);
    return true;
}

void TitanTCS::guideTimeoutNS()
{
    GuideNSNP.np[0].value = 0;
    GuideNSNP.np[1].value = 0;
    GuideNSNP.s           = IPS_IDLE;
    GuideNSTID            = 0;
    IDSetNumber(&GuideNSNP, nullptr);
}

void TitanTCS::guideTimeoutWE()
{
    GuideWENP.np[0].value = 0;
    GuideWENP.np[1].value = 0;
    GuideWENP.s           = IPS_IDLE;
    GuideWETID            = 0;
    IDSetNumber(&GuideWENP, nullptr);
}

void TitanTCS::guideTimeoutHelperNS(void * p)
{
    static_cast<TitanTCS *>(p)->guideTimeoutNS();
}

void TitanTCS::guideTimeoutHelperWE(void * p)
{
    static_cast<TitanTCS *>(p)->guideTimeoutWE();
}

bool TitanTCS::GetMountParams(bool bAll)
{
    INDI_UNUSED(bAll);

    static int cnt = 0;

    char szCommand[256];
    sprintf(szCommand, "#:\\GE($GR #:GR#"
            ":\\GE$GD #:GD#"
            "#:hP?#"
            ":\\?pe#"
            ":\\?tm#"
            ":\\?tr#"
            ":\\?ts#"
            ":\\GE%d)#", cnt++);

    //strcpy(szCommand, "#:hP?#" ":\\GE}#");

    char szResponse[256];
    if(!CommandResponseStr(szCommand, "(", ')', szResponse, sizeof(szResponse) - 1))
        return false;

    char szToken[128];
    // RA & DEC Coordinate
    if(GetParamHour(szResponse, szToken, sizeof(szToken) - 1, "$GR", '#', &info.ra))
    {
        if(GetParamHour(szResponse, szToken, sizeof(szToken) - 1, "$GD", '#', &info.dec))
        {
            LOGF_DEBUG("RA %g, DEC %g", info.ra, info.dec);
            NewRaDec(info.ra, info.dec);
        }
    }

#if USE_PEC
    // PEC Status
    if(GetParamNumber(szResponse, szToken, sizeof(szToken) - 1, "$?pe", '#', NULL, &info.PECStatus))
    {
        LOGF_DEBUG("PEC Status %d", info.PECStatus);
        _setPECState(info.PECStatus);
    }
#endif

    // Slewing Status  bit0:RA Tracking, bit1:DEC Tracking, bit2:RA Slewing, bit3:DEC Slewing, bit4,5:Goto status
    if(GetParamNumber(szResponse, szToken, sizeof(szToken) - 1, "$?ts", '#', NULL, &info.TrackingStatus))
    {
        LOGF_DEBUG("Tracking Status %d", info.TrackingStatus);

        if(info.TrackingStatus & 0x3C)
        {
            TrackState = SCOPE_SLEWING;
        }
        else if(info.TrackingStatus == 3)
        {
            TrackState = SCOPE_TRACKING;
        }
        else
        {
            TrackState = SCOPE_IDLE;
        }
    }
    // Parking Status
    if(GetParamNumber(szResponse, szToken, sizeof(szToken) - 1, "$hP", '#', NULL, &info.Parking))
    {
        LOGF_DEBUG("Parking Status %d", info.Parking);

        if(info.Parking == 1)
        {
            TrackState = SCOPE_PARKING;
            ParkS[0].s = ISS_ON;
            ParkS[1].s = ISS_OFF;
            ParkSP.s   = IPS_BUSY;
            IUSaveText(&MountInfoT[0], "Parking");
        }
        else if(info.Parking == 2)
        {
            TrackState = SCOPE_PARKED;
            ParkS[0].s = ISS_ON;
            ParkS[1].s = ISS_OFF;
            ParkSP.s   = IPS_IDLE;
            IUSaveText(&MountInfoT[0], "Parked");
        }
        else if(info.Parking == 0)
        {
            ParkSP.s   = IPS_IDLE;
            ParkS[0].s = ISS_OFF;
            ParkS[1].s = ISS_ON;
            IUSaveText(&MountInfoT[0], "Unpark");
        }

        IDSetSwitch(&ParkSP, nullptr);
    }
    // Tracking On / Off
    if((TrackState == SCOPE_SLEWING) || (TrackState == SCOPE_PARKING) || (TrackState == SCOPE_PARKED))
    {
        TrackStateS[TRACK_ON].s = ISS_OFF;
        TrackStateS[TRACK_OFF].s = ISS_ON;
        TrackStateSP.s = IPS_IDLE;
        IDSetSwitch(&TrackStateSP, nullptr);

        if(TrackState == SCOPE_PARKING)
            IUSaveText(&MountInfoT[1], "Parking");
        else if(TrackState == SCOPE_PARKED)
            IUSaveText(&MountInfoT[1], "Parked");
        else if(TrackState == SCOPE_SLEWING)
            IUSaveText(&MountInfoT[1], "Slewing");
    }
    else
    {
        if(GetParamNumber(szResponse, szToken, sizeof(szToken) - 1, "$?tm", '#', NULL, &info.Landscape))
        {
            LOGF_DEBUG("? %d, %d", TrackState, info.Landscape);

            if((TrackState == SCOPE_TRACKING) && (info.Landscape == 0))
            {
                TrackStateS[TRACK_ON].s = ISS_ON;
                TrackStateS[TRACK_OFF].s = ISS_OFF;
                TrackStateSP.s = IPS_IDLE;
                IDSetSwitch(&TrackStateSP, nullptr);

                IUSaveText(&MountInfoT[1], "Tracking ON / Skyview");
            }
            else
            {
                TrackStateS[TRACK_ON].s = ISS_OFF;
                TrackStateS[TRACK_OFF].s = ISS_ON;
                TrackStateSP.s = IPS_IDLE;
                IDSetSwitch(&TrackStateSP, nullptr);

                if(info.Landscape == 1)
                    IUSaveText(&MountInfoT[1], "Tracking OFF / Landscape");
                else
                    IUSaveText(&MountInfoT[1], "Tracking OFF / Idle");
            }
        }
    }
    //
    MountInfoTP.s = IPS_OK;
    IDSetText(&MountInfoTP, nullptr);
    //
    if(GetParamNumber(szResponse, szToken, sizeof(szToken) - 1, "$?tr", '#', NULL, &info.TrackingRate))
    {
        LOGF_DEBUG("Tracking rate %d", info.TrackingRate);

        TrackModeS[0].s = info.TrackingRate == 0 ? ISS_ON : ISS_OFF;
        TrackModeS[1].s = info.TrackingRate == 1 ? ISS_ON : ISS_OFF;
        TrackModeS[2].s = info.TrackingRate == 2 ? ISS_ON : ISS_OFF;
        IDSetSwitch(&TrackModeSP, nullptr);
    }
    //
    static int prev_TrackState = -1;
    if(prev_TrackState != TrackState)
    {
        prev_TrackState = TrackState;

        switch(TrackState)
        {
            case SCOPE_IDLE:
                LOG_INFO("Track State : IDLE");
                break;
            case SCOPE_SLEWING:
                LOG_INFO("Track State : SLEWING");
                break;
            case SCOPE_TRACKING:
                LOG_INFO("Track State : TRACKING");
                break;
            case SCOPE_PARKING:
                LOG_INFO("Track State : PARKING");
                break;
            case SCOPE_PARKED:
                LOG_INFO("Track State : PARKED");
                break;
        }
    }

    return true;
}
#if USE_PEC
void TitanTCS::_setPECState(int pec_status)
{
    if (_PECStatus != pec_status)
    {
        //LOGF_INFO("_setPECState(%x)", pec_status);
        char szText[128];

        _PECStatus = pec_status;
        // PEC Enabled      BIT 0
        // PEC Valid        BIT 1
        // PEC Training     BIT 2
        // PEC Stopping     BIT 3

        if(pec_status & 0x30)
        {
            // Training ...
            PECTrainingS[0].s = ISS_OFF;
            PECTrainingS[1].s = ISS_ON;

            sprintf(szText, "PEC Training %d %%", (pec_status >> 8));
            IUSaveText(&PECInfoT[1], szText);
        }
        else
        {
            PECTrainingS[0].s = ISS_ON;
            PECTrainingS[1].s = ISS_OFF;

            IUSaveText(&PECInfoT[1], "");
        }

        if(pec_status & 2)
        {
            // Valid
            if(pec_status & 1)
            {
                PECStateS[PEC_OFF].s = ISS_OFF;
                PECStateS[PEC_ON].s  = ISS_ON;

                IUSaveText(&PECInfoT[0], "PEC is running.");
            }
            else
            {
                PECStateS[PEC_OFF].s = ISS_OFF;
                PECStateS[PEC_ON].s  = ISS_ON;

                IUSaveText(&PECInfoT[0], "PEC is available.");
            }
            PECStateSP.s = IPS_OK;
        }
        else
        {
            // Invalid
            PECStateS[PEC_OFF].s = ISS_OFF;
            PECStateS[PEC_ON].s  = ISS_OFF;
            PECStateSP.s = IPS_ALERT;

            if(pec_status & 0x30)
                IUSaveText(&PECInfoT[0], "");
            else
                IUSaveText(&PECInfoT[0], "PEC training is required.");
        }

        PECStateSP.s = IPS_OK;
        IDSetSwitch(&PECStateSP, nullptr);

        PECTrainingSP.s = IPS_OK;
        IDSetSwitch(&PECTrainingSP, nullptr);
        //
        PECInfoTP.s = IPS_OK;
        IDSetText(&PECInfoTP, nullptr);
    }
}
#endif

// -----------------------------------------------------------------------------
bool TitanTCS::updateTime(ln_date *utc, double utc_offset)
{
    ln_zonedate ltm;

    if (isSimulation())
        return true;

    double JD = ln_get_julian_day(utc);

    LOGF_DEBUG("New JD is %.2f", JD);

    ln_date_to_zonedate(utc, &ltm, utc_offset * 3600);

    LOGF_DEBUG("Local time is %02d:%02d:%02g", ltm.hours, ltm.minutes, ltm.seconds);

    char szText[128];
    sprintf(szText, "#:SG %.1f#:SC %02d/%02d/%02d#:SL %02d:%02d:%02d#",
            -utc_offset,
            ltm.months, ltm.days, ltm.years % 100,
            ltm.hours, ltm.minutes, (int)ltm.seconds % 60);

    LOGF_INFO("Set datetime '%s'", szText);

    return SendCommand(szText);
}

// -----------------------------------------------------------------------------
bool TitanTCS::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);

    if((fabs(latitude) < 0.001) && (fabs(longitude) < 0.001))
        return false;

    int d = 0, m = 0, s = 0;
    char szCommand[128];
    char rtnCode = 0;

    getSexComponents(latitude, &d, &m, &s);
    sprintf(szCommand, "#:St %03d:%02d:%02d#", d, m, s);
    LOGF_INFO("Set latitude '%s'", szCommand);
    if(!CommandResponseChar(szCommand, NULL, &rtnCode))
        return false;

    getSexComponents(-longitude, &d, &m, &s);
    sprintf(szCommand, "#:Sg %03d:%02d:%02d#", d, m, s);
    LOGF_INFO("Set longitude '%s'", szCommand);
    return CommandResponseChar(szCommand, NULL, &rtnCode);
}

bool TitanTCS::Sync(double ra, double dec)
{
    if(!SetTarget(ra, dec))
        return false;

    char rtnCode[64] = { "" };
    char szCommand[MAX_CMD_LEN];
    sprintf(szCommand, ":CM#");

    if(!CommandResponseStr(szCommand, "", '#', rtnCode, sizeof(rtnCode) - 1))
    {
        LOG_ERROR("Sync / No response");
        return false;
    }
    if(strcmp(rtnCode, "1") != 0)
    {
        LOGF_ERROR("Sync / Error Code = '%s'", rtnCode);
        return false;
    }

    LOG_INFO("Sync");
    return true;
}
// -----------------------------------------------------------------------------
bool TitanTCS::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    char chDir = 0;

    switch(dir)
    {
        case DIRECTION_NORTH:
            chDir = 'n';
            break;
        case DIRECTION_SOUTH:
            chDir = 's';
            break;
        default:
            return false;
            break;
    }

    char szCommand[MAX_CMD_LEN];
    if(command == MOTION_START)
    {
        sprintf(szCommand, ":M%c#", chDir);
    }
    else
    {
        sprintf(szCommand, ":Q%c#", chDir);
    }

    TrackState = SCOPE_SLEWING;
    LOGF_INFO("Moving command:%s", szCommand);
    return SendCommand(szCommand);
}

// -----------------------------------------------------------------------------
bool TitanTCS::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    char chDir = 0;

    switch(dir)
    {
        case DIRECTION_EAST:
            chDir = 'e';
            break;
        case DIRECTION_WEST:
            chDir = 'w';
            break;
        default:
            return false;
            break;
    }

    char szCommand[MAX_CMD_LEN];
    if(command == MOTION_START)
    {
        sprintf(szCommand, ":M%c#", chDir);
    }
    else
    {
        sprintf(szCommand, ":Q%c#", chDir);
    }

    TrackState = SCOPE_SLEWING;
    LOGF_INFO("Moving command:%s", szCommand);
    return SendCommand(szCommand);
}
// -----------------------------------------------------------------------------
bool TitanTCS::Park()
{
    LOG_INFO("Parking ...");

    if(SendCommand(":hP8#"))
    {
        ParkSP.s   = IPS_BUSY;
        TrackState = SCOPE_PARKING;
        return true;
    }
    return false;
}
// -----------------------------------------------------------------------------
bool TitanTCS::UnPark()
{
    LOG_INFO("Unparking ...");

    if(SendCommand(":hP0#"))
    {
        ParkSP.s   = IPS_BUSY;
        TrackState = SCOPE_PARKING;
        return true;
    }
    return false;
}

// -----------------------------------------------------------------------------
bool TitanTCS::SetTrackMode(uint8_t mode)
{
    LOGF_INFO("SetTrackMode(%d)", mode);
    return SendCommand("#:\\T%d#", mode);
}
// -----------------------------------------------------------------------------
/*
T.B.D
bool TitanTCS::SetTrackRate(double raRate, double deRate)
{
    return true;
}
*/
// -----------------------------------------------------------------------------
bool TitanTCS::SetTrackEnabled(bool enabled)
{
    if(enabled)
    {
        LOG_INFO("Tracking ON");
        return SendCommand("#:\\t0#");   // Tracking
    }
    else
    {
        LOG_INFO("Tracking OFF");
        return SendCommand("#:\\t1#");   // Stop
    }
}

// -----------------------------------------------------------------------------
bool TitanTCS::SetParkPosition(double Axis1Value, double Axis2Value)
{
    INDI_UNUSED(Axis1Value);
    INDI_UNUSED(Axis2Value);

    return true;
}

bool TitanTCS::SetCurrentPark()
{
    return true;
}
// -----------------------------------------------------------------------------
bool TitanTCS::SetDefaultPark()
{
    return true;
}

bool TitanTCS::SetSlewRate(int index)
{
    LOGF_INFO("Set Slew Rate '%d'", index);

    switch (index)
    {
        case 3:
            return SendCommand(":RS#");
        case 2:
            return SendCommand(":RM#");
        case 1:
            return SendCommand(":RC#");
        case 0:
            return SendCommand(":RG#");
    }
    return false;
}
