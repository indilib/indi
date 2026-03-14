/*******************************************************************************
 Dome Simulator
 Copyright(c) 2014 Jasem Mutlaq. All rights reserved.

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

#include "indi_hakos_roof.h"

#include "indicom.h"

#include <cmath>
#include <cstring>
#include <ctime>
#include <curl/curl.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdlib.h>
#include <sstream>
#include <unistd.h>
#include <vector>


// We declare an auto pointer to HakosRoof.
std::unique_ptr<HakosRoof> hakosRoof(new HakosRoof());


static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

HakosRoof::HakosRoof()
{
    // Hakos Roof doesn't rely on the base dome class for connection
    setDomeConnection(CONNECTION_NONE);

    //Hakos Roof is a Roll Off Roof. We implement only basic Dome functions to open / close the roof
    SetDomeCapability(DOME_CAN_ABORT | DOME_CAN_PARK);
}

const char *HakosRoof::getDefaultName()
{
    return (const char *)"Hakos Roll Off Roof";
}


/************************************************************************************
 *
* ***********************************************************************************/
bool HakosRoof::initProperties()
{
    INDI::Dome::initProperties();

    SetParkDataType(PARK_NONE);  /*!< 2-state parking (Open or Closed only)  */

    addAuxControls();
  
    //Roof data
    IUFillText(&roofDataT[0], "MSG", "Last Command", nullptr);
    IUFillText(&roofDataT[1], "STAT", "Stat", nullptr);
    IUFillText(&roofDataT[2], "STEXT", "Status", nullptr);
    IUFillText(&roofDataT[3], "POS", "Position", nullptr);
    IUFillText(&roofDataT[4], "REL_POSITION", "Current Position %", "");
    IUFillTextVector(&roofDataTP, roofDataT, 5, getDeviceName(), "ROOF_DATA", "Roof Data", MAIN_CONTROL_TAB, IP_RO,
                     60, IPS_IDLE);
    
    //Source File
    IUFillText(&jsonDataT[0], "URL", "URL", nullptr);
    IUFillTextVector(&jsonDataTP, jsonDataT, 1, getDeviceName(), "DATA_SOURCE", "Data Source", OPTIONS_TAB, IP_RW,
                     60, IPS_IDLE);
    
    //Roof Token
    IUFillText(&roofTokenT[0], "TOKEN", "Token", nullptr);
    IUFillTextVector(&roofTokenTP, roofTokenT, 1, getDeviceName(), "ROOF_TOKEN", "Roof Token", OPTIONS_TAB, IP_RW,
                     60, IPS_IDLE);

    // Encoder Ticks
    IUFillNumber(&EncoderTicksN[0], "ENCODER_TICKS", "Encoder Ticks", "%6.0f", 0, 100000, 1, 0);
    IUFillNumberVector(&EncoderTicksNP, EncoderTicksN, 1, getDeviceName(), "ENCODER_TICKS", "Max Roof Travel", OPTIONS_TAB,
                       IP_RW, 60, IPS_IDLE);
    
    // Simulation Mode
    IUFillSwitch(&SimulationS[0], "SIMULATE", "Simulate Device", ISS_OFF);
    IUFillSwitchVector(&SimulationSP, SimulationS, 1, getDeviceName(), "SIMULATION", "Simulation", OPTIONS_TAB, IP_RW,
                       ISR_1OFMANY, 60, IPS_IDLE);
  
    return true;
}


bool HakosRoof::Connect()
{
    if (jsonDataT[0].text == nullptr || jsonDataT[0].text[0] == '\0')
    {
        //LOG_ERROR("A source file or url must be specified in options tab.");
        return true;
    }

    return true;
}
bool HakosRoof::Disconnect()
{
    INDI::Dome::Disconnect();
    return true;
}

bool HakosRoof::updateProperties()
{
    INDI::Dome::updateProperties();

    if (isConnected())
    {
        SetTimer(getCurrentPollingPeriod());
        SetupParms();
        defineProperty(&jsonDataTP);
        defineProperty(&roofDataTP);
        defineProperty(&roofTokenTP);
        defineProperty(&EncoderTicksNP);
        defineProperty(&SimulationSP);

     
    }
    else
    {
        deleteProperty(jsonDataTP.name);
        deleteProperty(roofDataTP.name);
        deleteProperty(roofTokenTP.name);
        deleteProperty(EncoderTicksNP.name);
        deleteProperty(SimulationSP.name);

    }

    return true;
}

bool HakosRoof::SetupParms()
{

    return true;
}

void HakosRoof::ISGetProperties(const char *dev)
{
    INDI::Dome::ISGetProperties(dev);

    defineProperty(&jsonDataTP);
    defineProperty(&roofDataTP);
    defineProperty(&roofTokenTP);
    defineProperty(&EncoderTicksNP);
    defineProperty(&SimulationSP);

    
    loadConfig(true, jsonDataTP.name);
    loadConfig(true, roofTokenTP.name);
    loadConfig(true, EncoderTicksNP.name);
    loadConfig(true, SimulationSP.name);
}
bool HakosRoof::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    // Make sure it is for us.
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
         if (!strcmp(EncoderTicksNP.name, name))
        {
            IUUpdateNumber(&EncoderTicksNP, values, names, n);
            EncoderTicksNP.s = IPS_OK;
            IDSetNumber(&EncoderTicksNP, nullptr);
            return true;
        }
   }

    // Nobody has claimed this, so let the parent handle it
    return INDI::Dome::ISNewNumber(dev, name, values, names, n);
}

bool HakosRoof::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    // Make sure it is for us.
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(SimulationSP.name, name))
        {
            IUUpdateSwitch(&SimulationSP, states, names, n);
            isSimulated = (SimulationS[0].s == ISS_ON);
            SimulationSP.s = IPS_OK;
            IDSetSwitch(&SimulationSP, nullptr);
            
            if (isSimulated)
            {
                LOG_INFO("Simulation mode enabled.");
                simulatedPosition = 0;
            }
            else
            {
                LOG_INFO("Simulation mode disabled.");
            }
            return true;
        }
    }

    // Nobody has claimed this, so let the parent handle it
    return INDI::Dome::ISNewSwitch(dev, name, states, names, n);
}

bool HakosRoof::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    // Make sure it is for us.
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(jsonDataTP.name, name))
        {
            IUUpdateText(&jsonDataTP, texts, names, n);
            jsonDataTP.s = IPS_OK;
            IDSetText(&jsonDataTP, nullptr);
            
            LOGF_INFO("Data source URL: %s.", jsonDataT[0].text);
            return true;
        }
        
        if (!strcmp(roofTokenTP.name, name))
        {
            IUUpdateText(&roofTokenTP, texts, names, n);
            roofTokenTP.s = IPS_OK;
            IDSetText(&roofTokenTP, nullptr);
            
            LOGF_INFO("Roof Token: %s.", roofTokenT[0].text);
            return true;
        }

        if (!strcmp(roofDataTP.name, name))
        {
            IUUpdateText(&roofDataTP, texts, names, n);
            roofDataTP.s = IPS_OK;
            IDSetText(&roofDataTP, nullptr);
            

            return true;
        }
    }

    return INDI::Dome::ISNewText(dev, name, texts, names, n);
}

bool HakosRoof::ISSnoopDevice(XMLEle *root)
{
    return INDI::Dome::ISSnoopDevice(root);
}

void HakosRoof::TimerHit()
{    

    if (!isConnected())
        return; //  No need to reset timer if we are not connected anymore

    if (isSimulated)
    {
        simulateRoofCommand("status");
    }
    else
    {
        sendRoofCommand("status");
    }

    if (getDomeState() == DOME_IDLE)
    {
        LOG_INFO("Dome state IDLE.");
    }
    else if (getDomeState() == DOME_UNPARKED )
    {
        LOG_INFO("Dome state UNPARKED.");
    }
    else if (getDomeState() == DOME_PARKED )
    {
        LOG_INFO("Dome state PARKED.");
    }
    else if (getDomeState() == DOME_SYNCED )
    {
        LOG_INFO("Dome state SYNCED.");
    }
    else if (getDomeState() == DOME_UNKNOWN )
    {
        LOG_INFO("Dome state UNKNOWN.");
    }
    else if (getDomeState() == DOME_UNPARKING )
    {
        LOG_INFO("Dome state UNPARKING.");
    }
    else if (getDomeState() == DOME_PARKING )
    {
        LOG_INFO("Dome state PARKING.");
    }
    else if (getDomeState() == DOME_MOVING )
    {
        LOG_INFO("Dome state MOVING.");
    }

    // If the roof was in mount 'not parked' status but the mount
    //is now parked, the roof has to be closed.
    if (strcmp(roofDataT[2].text ,"not parked") == 0 )
    {

        setDomeState(DOME_IDLE);

        if(getMountSensorSwitch() )
        {
            usleep(2000000);
            LOG_INFO("Resuming Park command.");
            Park();
            SetTimer(getCurrentPollingPeriod());

            return;
        }

    }

    if (strcmp(roofDataT[2].text ,"open") == 0)
    {
        LOG_INFO("Roof is open.");
        setDomeState(DOME_UNPARKED);
        SetParked(false);
        SetTimer(getCurrentPollingPeriod());

        return;
    }
    if (strcmp(roofDataT[2].text ,"closed") == 0)
    {
        LOG_INFO("Roof is closed.");
        setDomeState(DOME_UNPARKED);
        SetParked(true);
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    if (strcmp(roofDataT[2].text ,"opening") == 0)
    {
        setDomeState(DOME_UNPARKING);
        SetParked(false);
    }
    if (strcmp(roofDataT[2].text ,"closing") == 0)
    {
        setDomeState(DOME_PARKING);
        SetParked(false);
    }


    // Abort called
    if (strcmp(roofDataT[2].text ,"idle") == 0)
    {
        LOG_INFO("Roof motion is stopped.");
        setDomeState(DOME_IDLE);
        AbortDriver();
        SetTimer(getCurrentPollingPeriod());
        return;
    }
    if (DomeMotionSP.getState() == IPS_BUSY)
    {
        SetTimer(1000);
    }
    else
    {
        SetTimer(getCurrentPollingPeriod());

    }

}

bool HakosRoof::saveConfigItems(FILE *fp)
{

    INDI::Dome::saveConfigItems(fp);

    IUSaveConfigText(fp, &roofTokenTP);
    IUSaveConfigText(fp, &jsonDataTP);
    IUSaveConfigNumber(fp, &EncoderTicksNP);

    return true;
    
}

IPState HakosRoof::Move(DomeDirection dir, DomeMotionCommand operation)
{
    // If the roof is 'not parked' then just stop any motion

    if (operation == MOTION_START)
    {
        // DOME_CW --> OPEN. If can we are ask to "open" while we are fully opened as the limit switch indicates, then we simply return false.
  
        if (dir == DOME_CW && strcmp(roofDataT[2].text ,"open") == 0)
        {
            LOG_WARN("Roof is already fully opened.");
            return IPS_ALERT;
        }
        else if (dir == DOME_CCW && strcmp(roofDataT[2].text ,"closed") == 0)
        {
            LOG_WARN("Roof is already fully closed.");
            return IPS_ALERT;
        }
        else if (dir == DOME_CCW && INDI::Dome::isLocked())
        {
            DEBUG(INDI::Logger::DBG_WARNING,
                  "Cannot close dome when mount is locking. See: Telescope parking policy, in options tab");
            return IPS_ALERT;
        }

        SetTimer(1000);
        return IPS_BUSY;
    }

    return (Dome::Abort() ? IPS_OK : IPS_ALERT);
}

IPState HakosRoof::Park()
{
    IPState rc = INDI::Dome::Move(DOME_CCW, MOTION_START);
    if (rc == IPS_BUSY)
    {
        sendRoofCommand("close");
        LOG_INFO("Roof is parking...");
        return IPS_BUSY;
    }
    else
        return IPS_ALERT;
}

IPState HakosRoof::UnPark()
{
    IPState rc = INDI::Dome::Move(DOME_CW, MOTION_START);
    if (rc == IPS_BUSY)
    {
        sendRoofCommand("open");
        LOG_INFO("Roof is unparking...");
        return IPS_BUSY;
    }
    else
        return IPS_ALERT;
}

bool HakosRoof::Abort()
{
    ParkSP.reset();
    ParkSP.setState(IPS_IDLE);
    ParkSP.apply();
    sendRoofCommand("stop");
    LOG_INFO("Roof stopped");

    return true;
}
bool HakosRoof::AbortDriver()
{
    ParkSP.reset();
    ParkSP.setState(IPS_IDLE);
    ParkSP.apply();
    LOG_INFO("Roof stopped");

    return true;
}

bool HakosRoof::sendRoofCommand(const char *action)
{
    //LOGF_INFO("Sending command:  %s", action);

    // Check if required data is available
    if (jsonDataT[0].text == nullptr || jsonDataT[0].text[0] == '\0')
    {
        LOG_WARN("Data source URL not configured. Falling back to simulation mode.");
        return simulateRoofCommand(action);
    }
    
    if (roofTokenT[0].text == nullptr || roofTokenT[0].text[0] == '\0')
    {
        LOG_WARN("Roof token not configured. Falling back to simulation mode.");
        return simulateRoofCommand(action);
    }

    std::string readBuffer = "";

    curl_global_init(CURL_GLOBAL_ALL);
    CURL* easyhandle = curl_easy_init();
    CURLcode res = CURLE_OK;

    std::string reqCommand = jsonDataT[0].text;
    reqCommand += "/remobs?key=";
    reqCommand += roofTokenT[0].text;
    reqCommand += "&action=";
    reqCommand += action;

    curl_easy_setopt(easyhandle, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(easyhandle, CURLOPT_VERBOSE, 1);
    curl_easy_setopt(easyhandle, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(easyhandle, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(easyhandle, CURLOPT_TIMEOUT, 2); 
    curl_easy_setopt(easyhandle, CURLOPT_URL, reqCommand.c_str()); //http://jsonkeeper.com/b/4488
       
    res = curl_easy_perform(easyhandle);
    curl_easy_cleanup(easyhandle);

    if (res != CURLE_OK) {
        LOGF_WARN("Error %s. Falling back to simulation mode.", curl_easy_strerror(res));
        return simulateRoofCommand(action);
    }
    else
    {
        LOGF_INFO("json data %s", readBuffer.c_str());
        if (strcmp(action ,"status") == 0)
        {
            json j = json::parse(readBuffer);
 
            std::string msg = j["msg"];
            std::string stat= j["stat"];
            std::string stext= j["stext"];
            int i = j["pos"];
            std::string  pos = std::to_string(i);

            IUSaveText(&roofDataT[0], msg.c_str());
            IUSaveText(&roofDataT[1], stat.c_str());
            IUSaveText(&roofDataT[2], stext.c_str());
            IUSaveText(&roofDataT[3], pos.c_str());

            IDSetText(&roofDataTP, NULL);

           
        }    
    }

    return true;
}

bool HakosRoof::getMountSensorSwitch()
{
    std::string statString = roofDataT[1].text;
    std::vector<std::string> statVector{};
    std::stringstream sstream(statString);
    std::string statElement;
    while (std::getline(sstream, statElement, ',')){
        statVector.push_back(statElement);
    }

    if (strcmp(statVector.at(9).c_str() ,"0") == 0)
    {
        return true;
    }
    else
        return false;
}

bool HakosRoof::simulateRoofCommand(const char *action)
{
    // Simulate roof behavior for testing without hardware
    // Transition time: 10 seconds from 0 to 100%
    const int SIMULATION_TIME_MS = 10000;  // 10 seconds
    
    if (strcmp(action, "open") == 0)
    {
        simulationTargetPosition = 100;
        gettimeofday(&simulationStartTime, nullptr);
        isMoving = true;
        LOG_INFO("Simulated: Roof opening (10 second transition)...");
    }
    else if (strcmp(action, "close") == 0)
    {
        simulationTargetPosition = 0;
        gettimeofday(&simulationStartTime, nullptr);
        isMoving = true;
        LOG_INFO("Simulated: Roof closing (10 second transition)...");
    }
    else if (strcmp(action, "stop") == 0)
    {
        isMoving = false;
        LOG_INFO("Simulated: Roof stopped.");
    }
    else if (strcmp(action, "status") == 0)
    {
        // Update position based on elapsed time if moving
        if (isMoving)
        {
            struct timeval now;
            gettimeofday(&now, nullptr);
            
            // Calculate elapsed time in milliseconds
            long elapsedMs = (now.tv_sec - simulationStartTime.tv_sec) * 1000 +
                           (now.tv_usec - simulationStartTime.tv_usec) / 1000;
            
            // Calculate progress (0.0 to 1.0)
            float progress = (float)elapsedMs / SIMULATION_TIME_MS;
            
            if (progress >= 1.0f)
            {
                // Movement complete - snap to target
                simulatedPosition = simulationTargetPosition;
                isMoving = false;
                const char *endMsg = (simulationTargetPosition == 100) ? "Roof fully open" : "Roof fully closed";
                LOG_INFO(endMsg);
            }
            else
            {
                // Calculate intermediate position
                int startPos = (simulationTargetPosition == 100) ? 0 : 100;
                simulatedPosition = startPos + (simulationTargetPosition - startPos) * progress;
            }
        }
        
        // Update with simulated status data
        const char *status_msg = isMoving ? "Moving..." : "Idle";
        // Status text is now based on actual position after movement completes
        const char *stext = (simulatedPosition >= 100) ? "open" : (simulatedPosition <= 0) ? "closed" : "moving";
        char pos_str[10];
        snprintf(pos_str, sizeof(pos_str), "%d", simulatedPosition);

        const char *stat = "0,0,0,0,0,0,0,0,0,1";  // Mount sensor at position 9

        IUSaveText(&roofDataT[0], status_msg);
        IUSaveText(&roofDataT[1], stat);
        IUSaveText(&roofDataT[2], stext);
        IUSaveText(&roofDataT[3], pos_str);
        IUSaveText(&roofDataT[4], pos_str);

        IDSetText(&roofDataTP, NULL);
        
        if (isMoving)
        {
            LOGF_INFO("Simulated roof moving: Position=%d%%, Target=%d%%, Status=%s", simulatedPosition, simulationTargetPosition, stext);
        }
        else
        {
            LOGF_INFO("Simulated roof status: Position=%d%%, Status=%s", simulatedPosition, stext);
        }
    }

    return true;
}

