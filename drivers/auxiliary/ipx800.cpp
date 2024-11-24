/*******************************************************************************
This file is part of the IPX800 INDI Driver.
A driver for the IPX800 (GCE Electronics - https://www.gce-electronics.com)

Copyright (C) 2024 Arnaud Dupont (aknotwot@protonmail.com)

IPX800 INDI Driver is free software : you can redistribute it
and / or modify it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3 of the License,
or (at your option) any later version.

IPX800 INDI Driver is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the Lesser GNU General Public License
along with IPX800 INDI Driver.  If not, see
< http : //www.gnu.org/licenses/>.

This driver is adapted from RollOff ino drivers developped by Jasem Mutlaq.
The main purpose of this driver is to connect to IPX to driver, communicate, and manage 
opening and closing of roof. 
It is able to read IPX800 digital datas to check status and position of the roof.
User can select, partially, for this first release, how IPX 800 is configured 
*******************************************************************************/

#include "ipx800.h"

#include "indicom.h"
#include "config.h"

#include "connectionplugins/connectiontcp.h"

#include <cmath>
#include <cstring>
#include <ctime>
#include <memory>
#include <stdlib.h>
#include <stdio.h>
#include <cstdlib>
#include <string.h>
#include <algorithm>
#include <chrono>
#include <thread>
#include <functional>
#include <regex>

//Network related includes:
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>


#define DEFAULT_POLLING_TIMER 2000

// Read only
#define ROOF_OPENED_SWITCH 0
#define ROOF_CLOSED_SWITCH 1

// Write only
#define ROOF_OPEN_RELAY     "OPEN"
#define ROOF_CLOSE_RELAY    "CLOSE"
#define ROOF_ABORT_RELAY    "ABORT"

// Rollfino
#define INACTIVE_STATUS  5 

// We declare an auto pointer to ipx800.
std::unique_ptr<Ipx800> ipx800(new Ipx800());

void ISPoll(void *p);

/*************************************************************************/
/** Constructor                                                         **/
/*************************************************************************/

Ipx800::Ipx800() : INDI::InputInterface(this), INDI::OutputInterface(this)
{
    
	Roof_Status = UNKNOWN_STATUS;
	Mount_Status = NONE_PARKED;

	setVersion(IPX800_VERSION_MAJOR,IPX800_VERSION_MINOR);
}

/************************************************************************************
*
************************************************************************************/
bool Ipx800::initProperties()
{
    LOG_INFO("Starting device...");
    
	INDI::DefaultDevice::initProperties();
	INDI::InputInterface::initProperties("Inputs&Outputs", DIGITAL_INTPUTS, 0, "Digital");
    INDI::OutputInterface::initProperties("Inputs&Outputs", RELAYS_OUTPUTS, "Relay");
		
   // SetParkDataType(PARK_NONE);
    //addDebugControl(); 
    addAuxControls();         // This is for standard controls not the local auxiliary switch
	addConfigurationControl();
	

	//Rolling list of possible functions managed by relays
    IUFillSwitch(&RelaisInfoS[0], "Unused", "",ISS_ON);
    IUFillSwitch(&RelaisInfoS[1], "Roof Engine Power", "", ISS_OFF);
    IUFillSwitch(&RelaisInfoS[2], "Telescope Ventilation", "", ISS_OFF);
    IUFillSwitch(&RelaisInfoS[3], "Heating Resistor 1", "", ISS_OFF);
    IUFillSwitch(&RelaisInfoS[4], "Heating Resistor 2", "", ISS_OFF);
    IUFillSwitch(&RelaisInfoS[5], "Roof Control Command", "", ISS_OFF);
    IUFillSwitch(&RelaisInfoS[6], "Mount Power Supply", "", ISS_OFF);
    IUFillSwitch(&RelaisInfoS[7], "Camera Power Supply ", "", ISS_OFF);
    IUFillSwitch(&RelaisInfoS[8], "Other Power Supply 1", "", ISS_OFF);
    IUFillSwitch(&RelaisInfoS[9], "Other Power Supply 2", "", ISS_OFF);
    IUFillSwitch(&RelaisInfoS[10], "Other Power Supply 3", "", ISS_OFF);
    
	//set default value of each relay 
	for(int i=0;i<11;i++)
    {
        Relais1InfoS[i] = RelaisInfoS[i];
        Relais2InfoS[i] = RelaisInfoS[i];
        Relais3InfoS[i] = RelaisInfoS[i];
        Relais4InfoS[i] = RelaisInfoS[i];
        Relais5InfoS[i] = RelaisInfoS[i];
        Relais6InfoS[i] = RelaisInfoS[i];
        Relais7InfoS[i] = RelaisInfoS[i];
        Relais8InfoS[i] = RelaisInfoS[i];
    }

    //creation du selecteur de configuration des relais
    IUFillSwitchVector(&RelaisInfoSP[0], Relais1InfoS, 11, getDeviceName(), "RELAY_1_CONFIGURATION", "Relay 1", RELAYS_CONFIGURATION_TAB,
                     IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitchVector(&RelaisInfoSP[1], Relais2InfoS, 11, getDeviceName(), "RELAY_2_CONFIGURATION", "Relay 2", RELAYS_CONFIGURATION_TAB,
                     IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitchVector(&RelaisInfoSP[2], Relais3InfoS, 11, getDeviceName(), "RELAY_3_CONFIGURATION", "Relay 3", RELAYS_CONFIGURATION_TAB,
                     IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitchVector(&RelaisInfoSP[3], Relais4InfoS, 11, getDeviceName(), "RELAIS_4_CONFIGURATION", "Relay 4", RELAYS_CONFIGURATION_TAB,
                     IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitchVector(&RelaisInfoSP[4], Relais5InfoS,11, getDeviceName(), "RELAIS_5_CONFIGURATION", "Relay 5", RELAYS_CONFIGURATION_TAB,
                     IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitchVector(&RelaisInfoSP[5], Relais6InfoS, 11, getDeviceName(), "RELAIS_6_CONFIGURATION", "Relay 6", RELAYS_CONFIGURATION_TAB,
                     IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitchVector(&RelaisInfoSP[6], Relais7InfoS,11, getDeviceName(), "RELAIS_7_CONFIGURATION", "Relay 7", RELAYS_CONFIGURATION_TAB,
                     IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitchVector(&RelaisInfoSP[7], Relais8InfoS, 11, getDeviceName(), "RELAIS_8_CONFIGURATION", "Relay 8", RELAYS_CONFIGURATION_TAB,
                     IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    //creation liste deroulante entrees discretes
    IUFillSwitch(&DigitalInputS[0], "Unused", "",ISS_ON);
    IUFillSwitch(&DigitalInputS[1], "DEC Axis Parked", "", ISS_OFF);
    IUFillSwitch(&DigitalInputS[2], "RA Axis Parked", "", ISS_OFF);
    IUFillSwitch(&DigitalInputS[3], "Roof Opened", "", ISS_OFF);
    IUFillSwitch(&DigitalInputS[4], "Roof Closed", "", ISS_OFF);
    IUFillSwitch(&DigitalInputS[5], "Roof Engine Supplied", "", ISS_OFF);
    IUFillSwitch(&DigitalInputS[6], "Raspberry Power Supplied", "", ISS_OFF);
    IUFillSwitch(&DigitalInputS[7], "Main PC Supplied", "", ISS_OFF);
    IUFillSwitch(&DigitalInputS[8], "Other Digital 1", "", ISS_OFF);
    IUFillSwitch(&DigitalInputS[9], "Other Digital 2", "", ISS_OFF);
	
	//set default value of each digital input 
    for(int i=0;i<10;i++)
    {
        Digital1InputS[i] = DigitalInputS[i];
        Digital2InputS[i] = DigitalInputS[i];
        Digital3InputS[i] = DigitalInputS[i];
        Digital4InputS[i] = DigitalInputS[i];
        Digital5InputS[i] = DigitalInputS[i];
        Digital6InputS[i] = DigitalInputS[i];
        Digital7InputS[i] = DigitalInputS[i];
        Digital8InputS[i] = DigitalInputS[i];
    }
	
	//creation du selecteur de configuration des entrées discretes
    IUFillSwitchVector(&DigitalInputSP[0], Digital1InputS, 10, getDeviceName(), "DIGITAL_1_CONFIGURATION", "Digital 1", DIGITAL_INPUT_CONFIGURATION_TAB,
                     IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitchVector(&DigitalInputSP[1], Digital2InputS, 10, getDeviceName(), "DIGITAL_2_CONFIGURATION", "Digital 2", DIGITAL_INPUT_CONFIGURATION_TAB,
                     IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitchVector(&DigitalInputSP[2], Digital3InputS, 10, getDeviceName(), "DIGITAL_3_CONFIGURATION", "Digital 3", DIGITAL_INPUT_CONFIGURATION_TAB,
                     IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitchVector(&DigitalInputSP[3], Digital4InputS, 10, getDeviceName(), "DIGITAL_4_CONFIGURATION", "Digital 4", DIGITAL_INPUT_CONFIGURATION_TAB,
                     IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitchVector(&DigitalInputSP[4], Digital5InputS, 10, getDeviceName(), "DIGITAL_5_CONFIGURATION", "Digital 5", DIGITAL_INPUT_CONFIGURATION_TAB,
                     IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitchVector(&DigitalInputSP[5], Digital6InputS, 10, getDeviceName(), "DIGITAL_6_CONFIGURATION", "Digital 6", DIGITAL_INPUT_CONFIGURATION_TAB,
                     IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitchVector(&DigitalInputSP[6], Digital7InputS, 10, getDeviceName(), "DIGITAL_7_CONFIGURATION", "Digital 7", DIGITAL_INPUT_CONFIGURATION_TAB,
                     IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitchVector(&DigitalInputSP[7], Digital8InputS, 10, getDeviceName(), "DIGITAL_8_CONFIGURATION", "Digital 8", DIGITAL_INPUT_CONFIGURATION_TAB,
                     IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    //TO Manage in a next release
    //IUFillText(&LoginPwdT[0], "LOGIN_VAL", "Login", "");
    //IUFillText(&LoginPwdT[1], "PASSWD_VAL", "Password", "");
    //IUFillTextVector(&LoginPwdTP, LoginPwdT, 2, getDeviceName(), "ACCESS_IPX", "IPX Access", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);
	//defineProperty(&LoginPwdTP);
	// 
	
    //enregistrement des onglets de configurations
    for(int i=0;i<8;i++)
    {
        defineProperty(&RelaisInfoSP[i]);
        defineProperty(&DigitalInputSP[i]);
    }

    ///////////////////////////////////////////////
    //Page de presentation de l'état des relais
	///////////////////////////////////////////////
    //char name[3] = 'yyy', nameM[3] = 'xxx';
	const char *name = "";
	const char *nameM = "";
	
    for(int i=0;i<2;i++)
    {
        if (i ==0) {
            name = "On";
            nameM = "ON"; }
        else if (i ==1) {
            name = "Off";
            nameM = "OFF";
        }
		else {
			LOG_ERROR ("Initialization Error...");
			return false;
		}
        IUFillSwitch(&Relay1StateS[i], name, nameM, ISS_OFF);
        IUFillSwitch(&Relay2StateS[i], name, nameM, ISS_OFF);
        IUFillSwitch(&Relay3StateS[i], name, nameM, ISS_OFF);
        IUFillSwitch(&Relay4StateS[i], name, nameM, ISS_OFF);
        IUFillSwitch(&Relay5StateS[i], name, nameM, ISS_OFF);
        IUFillSwitch(&Relay6StateS[i], name, nameM, ISS_OFF);
        IUFillSwitch(&Relay7StateS[i], name, nameM, ISS_OFF);
        IUFillSwitch(&Relay8StateS[i], name, nameM, ISS_OFF);
    }
    IUFillSwitchVector(&RelaysStatesSP[0], Relay1StateS, 2, getDeviceName(), "RELAY_1_STATE", "Relay 1", RAW_DATA_TAB,
                     IP_RO,ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitchVector(&RelaysStatesSP[1], Relay2StateS, 2, getDeviceName(), "RELAY_2_STATE", "Relay 2", RAW_DATA_TAB,
                     IP_RO,ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitchVector(&RelaysStatesSP[2], Relay3StateS, 2, getDeviceName(), "RELAY_3_STATE", "Relay 3", RAW_DATA_TAB,
                     IP_RO,ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitchVector(&RelaysStatesSP[3], Relay4StateS, 2, getDeviceName(), "RELAY_4_STATE", "Relay 4", RAW_DATA_TAB,
                     IP_RO,ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitchVector(&RelaysStatesSP[4], Relay5StateS, 2, getDeviceName(), "RELAY_5_STATE", "Relay 5", RAW_DATA_TAB,
                     IP_RO,ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitchVector(&RelaysStatesSP[5], Relay6StateS, 2, getDeviceName(), "RELAY_6_STATE", "Relay 6", RAW_DATA_TAB,
                     IP_RO,ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitchVector(&RelaysStatesSP[6], Relay7StateS, 2, getDeviceName(), "RELAY_7_STATE", "Relay 7", RAW_DATA_TAB,
                     IP_RO,ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitchVector(&RelaysStatesSP[7], Relay8StateS, 2, getDeviceName(), "RELAY_8_STATE", "Relay 8", RAW_DATA_TAB,
                     IP_RO,ISR_1OFMANY, 60, IPS_IDLE);
					 
    //////////////////////////////////////////////////////////
    //page de presentation de l'état des entrées discretes
	//////////////////////////////////////////////////////////
	
    for(int i=0;i<2;i++)
    {
        if (i ==0) {
            name = "On";
            nameM = "ON"; }
        else if (i ==1) {
            name = "Off";
            nameM = "OFF";
        }
		else {
			LOG_ERROR ("Initialization Error...");
			return false;
		}
        IUFillSwitch(&Digit1StateS[i], name, nameM, ISS_OFF);
        IUFillSwitch(&Digit2StateS[i], name, nameM,ISS_OFF);
        IUFillSwitch(&Digit3StateS[i], name, nameM, ISS_OFF);
        IUFillSwitch(&Digit4StateS[i], name, nameM, ISS_OFF);
        IUFillSwitch(&Digit5StateS[i], name, nameM, ISS_OFF);
        IUFillSwitch(&Digit6StateS[i], name, nameM, ISS_OFF);
        IUFillSwitch(&Digit7StateS[i], name, nameM, ISS_OFF);
        IUFillSwitch(&Digit8StateS[i], name, nameM,ISS_OFF);
    }
    IUFillSwitchVector(&DigitsStatesSP[0], Digit1StateS, 2, getDeviceName(), "DIGIT_1_STATE", "Digital 1", RAW_DATA_TAB,
                     IP_RO,ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitchVector(&DigitsStatesSP[1], Digit2StateS, 2, getDeviceName(), "DIGIT_2_STATE", "Digital 2", RAW_DATA_TAB,
                     IP_RO,ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitchVector(&DigitsStatesSP[2], Digit3StateS, 2, getDeviceName(), "DIGIT_3_STATE", "Digital 3", RAW_DATA_TAB,
                     IP_RO,ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitchVector(&DigitsStatesSP[3], Digit4StateS, 2, getDeviceName(), "DIGIT_4_STATE", "Digital 4", RAW_DATA_TAB,
                     IP_RO,ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitchVector(&DigitsStatesSP[4], Digit5StateS, 2, getDeviceName(), "DIGIT_5_STATE", "Digital 5", RAW_DATA_TAB,
                     IP_RO,ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitchVector(&DigitsStatesSP[5], Digit6StateS, 2, getDeviceName(), "DIGIT_6_STATE", "Digital 6", RAW_DATA_TAB,
                     IP_RO,ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitchVector(&DigitsStatesSP[6], Digit7StateS, 2, getDeviceName(), "DIGIT_7_STATE", "Digital 7", RAW_DATA_TAB,
                     IP_RO,ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitchVector(&DigitsStatesSP[7], Digit8StateS, 2, getDeviceName(), "DIGIT_8_STATE", "Digital 8", RAW_DATA_TAB,
                     IP_RO,ISR_1OFMANY, 60, IPS_IDLE);
	
	// Initialisation des commutateurs ON/OFF
    IUFillSwitch(&roofEnginePowerS[0], "POWER_ON", "On", ISS_OFF);  // Par défaut sur OFF
    IUFillSwitch(&roofEnginePowerS[1], "POWER_OFF", "Off", ISS_ON); // Par défaut sur ON

    // Initialisation du vecteur de commutateurs avec ISRVector (type radio)
    IUFillSwitchVector(&roofEnginePowerSP, roofEnginePowerS, 2, getDeviceName(),
                           "ROOF_POWER_MNGT",         // Nom interne du vecteur
                           "Roof Engine Power Mngt",  // Label affiché
                           "Options",                 // Groupe (onglet Options)
                           IP_RW,                     // Permissions (lecture/écriture)
                           ISR_1OFMANY,               // Comportement exclusif (radio buttons)
                           0,                         // Timeout (si nécessaire, mettre 0 pour ignorer)
                           IPS_IDLE);                 // État initial (inactif)
	
	    // Ajouter la propriété à l'onglet OPTIONS_TAB
    defineProperty(&roofEnginePowerSP);	
	
	// Initialisation des commutateurs ON/OFF
    IUFillSwitch(&IPXVersionS[0], "VERSION_3", "V3", ISS_OFF);  // Par défaut sur OFF
    IUFillSwitch(&IPXVersionS[1], "VERSION_4", "V4", ISS_ON); // Par défaut sur ON
	IUFillSwitch(&IPXVersionS[2], "VERSION_5", "V5", ISS_OFF); // Par défaut sur ON

    // Initialisation du vecteur de commutateurs avec ISRVector (type radio)
    IUFillSwitchVector(&IPXVersionSP, IPXVersionS, 3, getDeviceName(),
                           "VERSION_SELECTION",         // Nom interne du vecteur
                           "IPX800 Version",  			// Label affiché
                           "Main Control",                 // Groupe (onglet Options)
                           IP_RO,                     // Permissions (lecture/écriture)
                           ISR_1OFMANY,               // Comportement exclusif (radio buttons)
                           0,                         // Timeout (si nécessaire, mettre 0 pour ignorer)
                           IPS_IDLE);                 // État initial (inactif)

    // Ajouter la propriété à l'onglet OPTIONS_TAB
    defineProperty(&IPXVersionSP);
	
	setDefaultPollingPeriod(DEFAULT_POLLING_TIMER);
	
	tcpConnection = new Connection::TCP(this);
	tcpConnection->setConnectionType(Connection::TCP::TYPE_TCP);
	tcpConnection->setDefaultHost("192.168.1.1");
	tcpConnection->setDefaultPort(666);
	
	LOG_DEBUG("Updating Connection - Handshake");
	tcpConnection->registerHandshake([&]()
	{
		LOG_DEBUG("Updating Connection - Call Handshake");
		return Handshake();
	}
	
	);
	registerConnection(tcpConnection);	
		
	return true;
}

bool Ipx800::Handshake()
{
	bool status = false;
	bool res = false;
    if (isSimulation())
    {
        LOGF_INFO("Connected successfuly to simulated %s.", getDeviceName());
        return true;
    }
	else {
		res = readCommand(GetR);
		readAnswer();
		if (res==false) {
			LOG_ERROR("Handshake with IPX800 failed");
			return false;
		}
		else {
			LOG_INFO("Handshake with IPX800 successfull");
			return true;
		}
		readAnswer();
		if (!checkAnswer())
		{
				LOG_ERROR("Handshake with IPX800 failed - Wrong answer");
				res = false;
		}
		else {
				recordData(GetR); 
		}
	}		
    return status;
}

void Ipx800::ISGetProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);

}

void ISSnoopDevice(XMLEle *root)
{
    ipx800->ISSnoopDevice(root);
}

bool Ipx800::ISSnoopDevice(XMLEle *root)
{
    return INDI::DefaultDevice::ISSnoopDevice(root);
}

/********************************************************************************************
** Establish conditions on a connect.
*********************************************************************************************/
bool Ipx800::setupParams()
{	
    LOG_DEBUG("Setting Params...");
    updateObsStatus(); 


    return true;
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    ipx800->ISNewSwitch(dev, name, states, names, n);
}

bool Ipx800::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    ISwitch *myRelaisInfoS;
    ISwitchVectorProperty myRelaisInfoSP;

    ISwitch *myDigitalInputS;
    ISwitchVectorProperty myDigitalInputSP;

    int currentRIndex, currentDIndex =0;
    bool infoSet = false;
	
	
	// Make sure the call is for our device, and Fonctions Tab are initialized
   if(dev != nullptr && !strcmp(dev,getDeviceName()))
   {
	   if (INDI::OutputInterface::processSwitch(dev, name, states, names, n))
            return true;
		
		// Vérifie que l'événement concerne le vecteur de commutateurs Roof Engine Power Management - Options Tab
        if (strcmp(name, roofEnginePowerSP.name) == 0)
        {
            // Parcours des commutateurs pour traiter les changements d'état
            for (int i = 0; i < n; i++)
            {
                if (strcmp(names[i], "POWER_ON") == 0)
                {
                    // Si POWER_ON est activé
                    if (states[i] == ISS_ON)
                    {
                        IDMessage(getDeviceName(), "Roof Engine Power Management: ON");
                        roofEnginePowerS[0].s = ISS_ON;
                        roofEnginePowerS[1].s = ISS_OFF;  // Désactive l'autre switch
						roofPowerManagement = true;
                    }
                }
                else if (strcmp(names[i], "POWER_OFF") == 0)
                {
                    // Si POWER_OFF est activé
                    if (states[i] == ISS_ON)
                    {
                        IDMessage(getDeviceName(), "Roof Engine Power Management: OFF");
                        roofEnginePowerS[0].s = ISS_OFF;
                        roofEnginePowerS[1].s = ISS_ON;  // Désactive l'autre switch
						roofPowerManagement = false;
                    }
                }
            }

            // Marque le vecteur de commutateurs comme mis à jour et notifie l'interface
            roofEnginePowerSP.s = IPS_OK;
            IDSetSwitch(&roofEnginePowerSP, nullptr);
        }
		
		for(int i=0;i<8;i++)
		{
			myRelaisInfoSP = ipx800->getMyRelayVector(i);
			myDigitalInputSP = ipx800->getMyDigitsVector(i);
			
			////////////////////////////////////////////////////
			// Relay Configuration
			////////////////////////////////////////////////////
			
			
			if (!strcmp(name, myRelaisInfoSP.name))
				{	
					LOGF_DEBUG("Relay function selected - SP : %s", myRelaisInfoSP.name);
					IUUpdateSwitch(&myRelaisInfoSP,states,names,n);
					
					myRelaisInfoS = myRelaisInfoSP.sp;
					myRelaisInfoSP.s = IPS_OK;
					IDSetSwitch(&myRelaisInfoSP,nullptr);
					
					currentRIndex = IUFindOnSwitchIndex(&myRelaisInfoSP);
					
					if (currentRIndex != -1) {
						Relay_Fonction_Tab [currentRIndex] = i;
						
						LOGF_DEBUG("Relay fonction index : %d", currentRIndex);

						defineProperty(&RelaysStatesSP[i]);}
					else 
						LOG_DEBUG("No On Switches found"); 
					
					infoSet = true;
					return true;
			}
				
			////////////////////////////////////////////////////
			// Digits Configuration
			////////////////////////////////////////////////////
			if (!strcmp(name, myDigitalInputSP.name))
				{ 
				LOGF_DEBUG("Digital init : %s", myDigitalInputSP.name);
				IUUpdateSwitch(&myDigitalInputSP,states,names,n);
				
				myDigitalInputS = myDigitalInputSP.sp;
				// myRelaisInfoS[].s  = ISS_ON;
				myDigitalInputSP.s = IPS_OK;
				IDSetSwitch(&myDigitalInputSP,nullptr);
				//sauvegarde de la configuration
				currentDIndex = IUFindOnSwitchIndex(&myDigitalInputSP);
				if (currentDIndex != -1) {
					Digital_Fonction_Tab [currentDIndex] = i;
					LOGF_DEBUG("Digital Inp. fonction index : %d", currentDIndex);
					defineProperty(&DigitsStatesSP[i]);
			
				 }
				else 
					LOG_DEBUG("No On Switches found"); 
				
				infoSet = true; 
				return true;
			}

		}
		
		LOG_DEBUG("ISNewSwitch - First Init + UpDate");
		updateIPXData();
			
		if (infoSet) 
			updateObsStatus();
		
   }
   return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);	

}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    ipx800->ISNewText(dev, name, texts, names, n);
}

bool Ipx800::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    ////////////////////////////////////////////////////
    // IPX Access - If password protected
	// To manage in a next release
    ////////////////////////////////////////////////////
    /*
	IText* myLoginT = ipx800->getMyLogin();
    ITextVectorProperty myLoginVector = ipx800->getMyLoginVector();
    if (dev && !strcmp(name, myLoginVector.name))
    {	
        IUUpdateText(&myLoginVector, texts, names, n);
        myLoginVector.s = IPS_OK;
        myPasswd = myLoginT[1].text;
        myLogin = myLoginT[0].text;
        IDSetText(&myLoginVector, nullptr);
		LOG_INFO("Password updated");
     }
	 */
	 
	 
	 if (INDI::InputInterface::processText(dev, name, texts, names, n))
            return true;
     if (INDI::OutputInterface::processText(dev, name, texts, names, n))
            return true;
	
    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n); 
  }

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    ipx800->ISNewNumber(dev, name, values, names, n);
}

bool Ipx800::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
		return true;

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

///////////////////////////////////////////
// When IPX800 is connected two more tabs appear : Relays Status
//  Digital inputs Status
///////////////////////////////////////////
bool Ipx800::Connect()
{
	bool status = INDI::DefaultDevice::Connect();
	LOG_DEBUG("Connecting to device...");

    return status;
    
}

bool Ipx800::Disconnect()
{
    bool status = INDI::DefaultDevice::Disconnect();
	
    return status;
}

const char *Ipx800::getDefaultName()
{
    return (const char *)"Ipx800";
}

/////////////////////////////////////////
// Used after connection / Disconnection
/////////////////////////////////////////
bool Ipx800::updateProperties()
{
	INDI::DefaultDevice::updateProperties();
	
	LOG_DEBUG("updateProperties - Starting");
	///////////////////////////
    if (isConnected())
    { // Connect both states tabs 
		updateIPXData();
		//firstFonctionTabInit();
		//updateObsStatus();
		INDI::InputInterface::updateProperties();
		INDI::OutputInterface::updateProperties();
		defineProperty(&roofEnginePowerSP);
		defineProperty(&IPXVersionSP);
        for(int i=0;i<8;i++)
        {
            defineProperty(&RelaysStatesSP[i]);
			//MàJ DomeState
        }
        for(int i=0;i<8;i++)
        {
             defineProperty(&DigitsStatesSP[i]);
        }
	
        setupParams(); 

    }
    else { // Disconnect both "States TAB"
      
		for(int i=0;i<8;i++)
        {
            deleteProperty(RelaysStatesSP[i].name);

        }
        for(int i=0;i<8;i++)
        {
             deleteProperty(DigitsStatesSP[i].name);
        }
		deleteProperty(roofEnginePowerSP.name);
		deleteProperty(IPXVersionSP.name);

    }
    return true;
}

//////////////////////////////////////////
// ** complete **
//////////////////////////////////////////
void Ipx800::TimerHit()
{
  
    if (!isConnected())  {
        return; //  No need to reset timer if we are not connected anymore
	}
	
	updateIPXData();
    
    SetTimer(getPollingPeriod());
}
//////////////////////////////////////
/* Save conf */
bool Ipx800::saveConfigItems(FILE *fp)
{

	INDI::DefaultDevice::saveConfigItems(fp);
    //IUSaveConfigText(fp, &LoginPwdTP);
	
	/** sauvegarde de la configuration des relais et entrées discretes **/ 
    ////////////////////////////
	for(int i=0;i<8;i++)
    {
        IUSaveConfigSwitch(fp, &RelaisInfoSP[i]);
        IUSaveConfigSwitch(fp, &DigitalInputSP[i]);
    }
	INDI::InputInterface::saveConfigItems(fp);
    INDI::OutputInterface::saveConfigItems(fp);
    return true;////////
}

//////////////////////////////////////
/* readCommand */
bool Ipx800::readCommand(IPX800_command rCommand) {

    bool rc = false;
    std::string ipx_url = "";
    //int bytesWritten = 0, totalBytes = 0;
    switch (rCommand) {
		case GetR :
			ipx_url = "Get=R";
			break;
		case GetD :
			ipx_url = "Get=D";
			break;
		default :
		{
			LOGF_ERROR("readCommand - Unknown Command %s", rCommand);
			return false;
		}
    }
    LOGF_DEBUG ("readCommand - Sending %s",ipx_url.c_str());
    rc = writeTCP(ipx_url);

    return rc;
};

//////////////////////////////////////
/* writeCommand */
bool Ipx800::writeCommand(IPX800_command wCommand, int toSet) {

    std::string ipx_url;
    std::string number;
	
	
    bool rc = false;
    //int bytesWritten = 0, totalBytes = 0;
    if (toSet <10)
        number = "0"+ std::to_string(toSet);
    else {
        number = std::to_string(toSet);
    }
    switch (wCommand) {
		case SetR :
			ipx_url = "SetR=" + number ;
			LOGF_DEBUG ("Sending Set R %s",number.c_str());
			break;
		case ClearR :
			ipx_url = "ClearR=" + number;
			LOGF_DEBUG ("Sending Clear R %s",number.c_str());
			break;
		default :
		{
			LOGF_ERROR("Unknown Command %s", wCommand);
			return false;
		}

    }
    rc = writeTCP(ipx_url); 
	
    return rc;
};
//////////////////////////////////////
/* readAnswer */
// TCP Answer reading 
void Ipx800::readAnswer(){
    int received = 0;
    int bytes, total = 0;
    int portFD = tcpConnection->getPortFD();
    char tmp[58] = "";
    total = 58;
	int i = 0;
    do {
        bytes = read(portFD,tmp+received,total-received);

        if (bytes < 0) {
            LOGF_ERROR("readAnswer - ERROR reading response from socket : %s", strerror(errno));
            //std::this_thread::sleep_for(std::chrono::milliseconds(500));
			i++;
			if (i>2)
				break;
			}
        else if (bytes == 0) {
            LOG_DEBUG("readAnswer : end of stream");
            break; }
        received+=bytes;
    } while (received < total);

    LOGF_DEBUG("readAnswer - Longeur reponse : %i", received);
	
    strncpy(tmpAnswer,tmp,8);
	
    LOGF_DEBUG ("readAnswer - Reponse reçue : %s", tmpAnswer);

  };

//////////////////////////////////////
/* recordData */
void Ipx800::recordData(IPX800_command recCommand) {
    int i = -1;
	int tmpDR = UNUSED_DIGIT;
	switch (recCommand) {
    case GetD :
			for (i=0;i<8;i++){				
				DigitsStatesSP[i].s = IPS_OK;
				DigitalInputsSP[i].reset();
				if (tmpAnswer[i] == '0') {
					LOGF_DEBUG("recordData - Digital Input N° %d is %s",i+1,"OFF");
					DigitalInputsSP[i][0].setState(ISS_ON);
					DigitsStatesSP[i].sp[0].s = ISS_OFF;
					DigitsStatesSP[i].sp[1].s = ISS_ON;
					digitalState[i] = false;}
				else if(tmpAnswer[i] == '1'){
					LOGF_DEBUG("recordData - Digital Input N° %d is %s",i+1,"ON");
					DigitalInputsSP[i][1].setState(ISS_ON);
					DigitsStatesSP[i].sp[0].s = ISS_ON ;
					DigitsStatesSP[i].sp[1].s = ISS_OFF;
					digitalState[i] = true;
				}
				tmpAnswer[i] = ' ';
			    DigitalInputsSP[i].setState(IPS_OK);
                DigitalInputsSP[i].apply();
			    defineProperty(&DigitsStatesSP[i]);
				DigitsStatesSP[i].s = IPS_OK;
				IDSetSwitch(&DigitsStatesSP[i], nullptr);
			}
			
			for (i=0;i<10;i++) {
				tmpDR = Digital_Fonction_Tab[i];
				if (tmpDR >= 0) {
					 
					if (tmpDR == ROOF_ENGINE_POWERED ) {
						DigitalInputsSP[Digital_Fonction_Tab[ROOF_ENGINE_POWERED]].reset();
						if (DigitsStatesSP[Digital_Fonction_Tab[ROOF_ENGINE_POWERED]].sp[0].s == ISS_OFF) {
							LOG_DEBUG("recordData - inverting ROOF_ENGINE_POWERED TO ON");
							
							DigitsStatesSP[Digital_Fonction_Tab[ROOF_ENGINE_POWERED]].sp[0].s  = ISS_ON;
							DigitsStatesSP[Digital_Fonction_Tab[ROOF_ENGINE_POWERED]].sp[1].s = ISS_OFF;
							digitalState[Digital_Fonction_Tab[ROOF_ENGINE_POWERED]] = true;
							DigitalInputsSP[Digital_Fonction_Tab[ROOF_ENGINE_POWERED]][1].setState(ISS_ON);}
						else {
							LOG_DEBUG("recordData - inverting ROOF_ENGINE_POWERED TO OFF");
							DigitsStatesSP[Digital_Fonction_Tab[ROOF_ENGINE_POWERED]].sp[0].s = ISS_OFF;
							DigitsStatesSP[Digital_Fonction_Tab[ROOF_ENGINE_POWERED]].sp[1].s = ISS_ON;
							digitalState[Digital_Fonction_Tab[ROOF_ENGINE_POWERED]] = false;
							DigitalInputsSP[Digital_Fonction_Tab[ROOF_ENGINE_POWERED]][0].setState(ISS_ON);}
						
						
						DigitalInputsSP[Digital_Fonction_Tab[ROOF_ENGINE_POWERED]].setState(IPS_OK);
						DigitalInputsSP[Digital_Fonction_Tab[ROOF_ENGINE_POWERED]].apply();		
						defineProperty(&DigitsStatesSP[Digital_Fonction_Tab[ROOF_ENGINE_POWERED]]);
						DigitsStatesSP[Digital_Fonction_Tab[ROOF_ENGINE_POWERED]].s = IPS_OK;
						IDSetSwitch(&DigitsStatesSP[Digital_Fonction_Tab[ROOF_ENGINE_POWERED]], nullptr);
						
					}
					else if(tmpDR == RASPBERRY_SUPPLIED) {
						DigitalInputsSP[Digital_Fonction_Tab[ROOF_ENGINE_POWERED]].reset();
						if (DigitsStatesSP[Digital_Fonction_Tab[RASPBERRY_SUPPLIED]].sp[0].s == ISS_OFF) {
							LOG_DEBUG("recordData - inverting RASPBERRY_SUPPLIED TO ON");
							DigitsStatesSP[Digital_Fonction_Tab[RASPBERRY_SUPPLIED]].sp[0].s  = ISS_ON;
							DigitsStatesSP[Digital_Fonction_Tab[RASPBERRY_SUPPLIED]].sp[1].s = ISS_OFF;
							digitalState[Digital_Fonction_Tab[RASPBERRY_SUPPLIED]] = true;
							DigitalInputsSP[Digital_Fonction_Tab[RASPBERRY_SUPPLIED]][1].setState(ISS_ON);}
						else {
							LOG_DEBUG("recordData - inverting RASPBERRY_SUPPLIED TO OFF");
							DigitsStatesSP[Digital_Fonction_Tab[RASPBERRY_SUPPLIED]].sp[0].s = ISS_OFF;
							DigitsStatesSP[Digital_Fonction_Tab[RASPBERRY_SUPPLIED]].sp[1].s = ISS_ON;
							digitalState[Digital_Fonction_Tab[RASPBERRY_SUPPLIED]] = false;
							DigitalInputsSP[Digital_Fonction_Tab[RASPBERRY_SUPPLIED]][0].setState(ISS_ON);}
							
						DigitalInputsSP[Digital_Fonction_Tab[RASPBERRY_SUPPLIED]].setState(IPS_OK);
						DigitalInputsSP[Digital_Fonction_Tab[RASPBERRY_SUPPLIED]].apply();	
						defineProperty(&DigitsStatesSP[Digital_Fonction_Tab[RASPBERRY_SUPPLIED]]);
						DigitsStatesSP[Digital_Fonction_Tab[RASPBERRY_SUPPLIED]].s = IPS_OK;
						IDSetSwitch(&DigitsStatesSP[Digital_Fonction_Tab[RASPBERRY_SUPPLIED]], nullptr);
						
					}
					else if (tmpDR == MAIN_PC_SUPPLIED) {
						DigitalInputsSP[Digital_Fonction_Tab[MAIN_PC_SUPPLIED]].reset();						
						if (DigitsStatesSP[Digital_Fonction_Tab[MAIN_PC_SUPPLIED]].sp[0].s == ISS_OFF) {
							LOG_DEBUG("recordData - inverting MAIN_PC_SUPPLIED TO ON");
							DigitsStatesSP[Digital_Fonction_Tab[MAIN_PC_SUPPLIED]].sp[0].s  = ISS_ON;
							DigitsStatesSP[Digital_Fonction_Tab[MAIN_PC_SUPPLIED]].sp[1].s = ISS_OFF;
							digitalState[Digital_Fonction_Tab[MAIN_PC_SUPPLIED]] = true; 
							DigitalInputsSP[Digital_Fonction_Tab[MAIN_PC_SUPPLIED]][1].setState(ISS_ON);}
						else {
							LOG_DEBUG("recordData - inverting MAIN_PC_SUPPLIED TO OFF");
							DigitsStatesSP[Digital_Fonction_Tab[MAIN_PC_SUPPLIED]].sp[0].s = ISS_OFF;
							DigitsStatesSP[Digital_Fonction_Tab[MAIN_PC_SUPPLIED]].sp[1].s = ISS_ON;
							digitalState[Digital_Fonction_Tab[MAIN_PC_SUPPLIED]] = false;
							DigitalInputsSP[Digital_Fonction_Tab[MAIN_PC_SUPPLIED]][0].setState(ISS_ON);}
						
						DigitalInputsSP[Digital_Fonction_Tab[MAIN_PC_SUPPLIED]].setState(IPS_OK);
						DigitalInputsSP[Digital_Fonction_Tab[MAIN_PC_SUPPLIED]].apply();	
						defineProperty(&DigitsStatesSP[Digital_Fonction_Tab[MAIN_PC_SUPPLIED]]);
						DigitsStatesSP[Digital_Fonction_Tab[MAIN_PC_SUPPLIED]].s = IPS_OK;
						IDSetSwitch(&DigitsStatesSP[Digital_Fonction_Tab[MAIN_PC_SUPPLIED]], nullptr);
					
					}
				}
				else {
					LOGF_WARN("Wrong Digital ROOF_ENGINE_POWERED initialisation. Now it's = %d",tmpDR);
				}

		enginePowered = digitalState[Digital_Fonction_Tab [ROOF_ENGINE_POWERED]];
			}
		break;
    case GetR :
        for (int i=0;i<8;i++){
            RelaysStatesSP[i].s = IPS_OK;
			DigitalOutputsSP[i].reset();
            if (tmpAnswer[i] == '0') {
                LOGF_DEBUG("recordData - Relay N° %d is %s",i+1,"OFF");
                RelaysStatesSP[i].sp[0].s = ISS_OFF;
                RelaysStatesSP[i].sp[1].s = ISS_ON;
				DigitalOutputsSP[i][0].setState(ISS_ON);
                //relayState[i]= false;
            }
            else {
                LOGF_DEBUG("recordData - Relay N° %d is %s",i+1,"ON");
                RelaysStatesSP[i].sp[0].s  = ISS_ON;
                RelaysStatesSP[i].sp[1].s  = ISS_OFF;
				DigitalOutputsSP[i][1].setState(ISS_ON);
                //relayState[i]=true;
            }
            tmpAnswer[i] = ' ';
			DigitalOutputsSP[i].setState(IPS_OK);
            DigitalOutputsSP[i].apply();
			RelaysStatesSP[i].s = IPS_OK;
			
			IDSetSwitch(&RelaysStatesSP[i], nullptr);
			//defineProperty(&RelaysStatesSP[i]);
			
        }
        break;
    default :
        LOGF_ERROR("recordData - Unknown Command %s", recCommand);
        break;
    }
	
    LOG_DEBUG("recordData - Switches States Recorded");

};

//////////////////////////////////////
/* writeTCP Write Command on TCP socket */
bool Ipx800::writeTCP(std::string toSend) {

    int bytesWritten = 0, totalBytes = 0;
    totalBytes = toSend.length();
    int portFD = tcpConnection->getPortFD();

    LOGF_DEBUG("writeTCP - Command to send %s", toSend.c_str());
    LOGF_DEBUG ("writeTCP - Numéro de socket %i", portFD);

    if (!isSimulation()) {
        while (bytesWritten < totalBytes)
        {
			int bytesSent = write(portFD, toSend.c_str(), totalBytes - bytesWritten);
            if (bytesSent >= 0)
                bytesWritten += bytesSent;
				
            else
            {
                LOGF_ERROR("writeTCP - Error request to IPX800. %s", strerror(errno));
                return false;
            }
        }
    }

    LOGF_DEBUG ("writeTCP - bytes to send : %s", toSend.c_str());
    LOGF_DEBUG ("writeTCP - Number of bytes sent : %d", bytesWritten);
    return true;
}



//////////////////////////////////////
/* updateIPXData */
/* prepare commands to update Inputs Status */
//////////////////////////////////////
bool Ipx800::updateIPXData()
{
    bool res = false;
	LOG_DEBUG("Updating IPX Data...");
	
    res = UpdateDigitalOutputs();
    if (res==false) {
        LOG_ERROR("updateIPXData - Send Command GetR failed");
		return false;
	}
   
    res = UpdateDigitalInputs();
    if (res==false) {
        LOG_ERROR("updateIPXData - Send Command GetD failed");
		return false;
    }
  
    return true;
}

//////////////////////////////////////
/* updateObsStatus */
void Ipx800::updateObsStatus()
{
	
}

//////////////////////////////////////
/* randomInit */
bool Ipx800::firstFonctionTabInit()
{
	int currentDIndex = -1;
	int currentRIndex = -1;
	//int cptD, cptR =0;
	for(int i=0;i<8;i++)
	{
		currentRIndex = IUFindOnSwitchIndex(&RelaisInfoSP[i]);
		if (currentRIndex != -1) {
			Relay_Fonction_Tab [currentRIndex] = i;
			LOGF_DEBUG("firstFonctionTabInit - Relay %d is supporting function %d ",i+1, currentRIndex);
			currentRIndex = -1; 
			//if (currentRIndex >0)
			//	cptR = cptR +1;
			}
		else
			LOGF_DEBUG("firstFonctionTabInit - Function unknown for Relay %d", i+1);
		
		currentDIndex = IUFindOnSwitchIndex(&DigitalInputSP[i]);
		if (currentDIndex != -1) {
			Digital_Fonction_Tab [currentDIndex] = i;
			LOGF_DEBUG("firstFonctionTabInit - Digital Input %d is supporting function %d ",i+1, currentDIndex);
			currentDIndex = -1;
			//if (currentDIndex >0)
			//	cptD = cptD +1;
			}
		else
			LOGF_DEBUG("firstFonctionTabInit - Function unknown for Digital Input %d", i+1);
	}
	
	return true;
}	

//////////////////////////////////////
/* checkAnswer */
bool Ipx800::checkAnswer()
{
    for (int i=0;i<8;i++)
    {
        if ((tmpAnswer[i] == '0') || (tmpAnswer[i] == '1'))
        {
			return true;
        }
        else {
            LOGF_ERROR("Wrong data in IPX answer : %s", tmpAnswer[i]);
            return false;
        }
    }

    return true;
}
/*
Password Management for a next release
IText* Ipx800::getMyLogin()
{
    return LoginPwdT;
}

ITextVectorProperty Ipx800::getMyLoginVector()
{
     return LoginPwdTP;
}
*/

ISwitchVectorProperty Ipx800::getMyRelayVector(int i)
{
     return RelaisInfoSP[i];
}

ISwitchVectorProperty Ipx800::getMyDigitsVector(int i)
{
     return DigitalInputSP[i];
}


//////////////////////////////////////
/* UpdateDigitalInputs */
bool Ipx800::UpdateDigitalInputs() 
{	
	// update of all digital inputs
	bool res = readCommand(GetD);
	readAnswer();
	if (res==false) {
		LOG_ERROR("UpdateDigitalInputs - Send Command GetD failed");
		return res;
		}
	else {
		LOG_DEBUG("UpdateDigitalInputs - Send Command GetD successfull");
		
		if (!checkAnswer())
		{
			LOG_ERROR("UpdateDigitalInputs - Wrong Command GetD send");
			res = false;
		}
		else {
			recordData(GetD); 
		}
	}
	return true; 
}

bool Ipx800::UpdateAnalogInputs()
{
	return true;
}

//////////////////////////////////////
/* UpdateDigitalOutputs */
// Update Relays Status
bool Ipx800::UpdateDigitalOutputs()
{
			// update of all digital inputs
		bool res = readCommand(GetR);
		readAnswer();
		if (res==false) {
			LOG_ERROR("UpdateDigitalOutputs - Send Command GetR failed");
			
			return res;
			}
		else {
			LOG_DEBUG("UpdateDigitalOutputs - Send Command GetR successfull");
			
			if (!checkAnswer())
			{
				LOG_ERROR("UpdateDigitalOutputs - Wrong Command GetR send");
				res = false;
			}
			else {
				recordData(GetR); 
			}
		}
		return true;
}

//////////////////////////////////////
/* CommandOutput */
// 
bool Ipx800::CommandOutput(uint32_t index, OutputState command) 
{
	//check index is controling enginepower
	int relayNumber = index+1;
	bool rc = false;
	//modifier pour permettre l'emission de commande pour toutes les commandes....sans lien avec le moteur
	if (roofPowerManagement && enginePowered==false && static_cast<uint32_t>(Relay_Fonction_Tab[ROOF_CONTROL_COMMAND])==static_cast<uint32_t>(index)) {
		LOG_WARN("Please switch on roof engine power");
		return false; }
	else {
		if (command ==  INDI::OutputInterface::On) 
			rc = writeCommand(SetR, relayNumber);
		else
			rc = writeCommand(ClearR, relayNumber);
		readAnswer(); 
		return rc;
	}
		
}
