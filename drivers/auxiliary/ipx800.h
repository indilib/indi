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
GNU General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with IPX800 INDI Driver.  If not, see
< http : //www.gnu.org/licenses/>.

*******************************************************************************/
#pragma once

#include "defaultdevice.h"
#include <indioutputinterface.h>
#include <indiinputinterface.h>
#include <indidevapi.h>
#include <indiapi.h>
 
class Ipx800 : public INDI::DefaultDevice, public INDI::InputInterface, public INDI::OutputInterface
 
{
  public:
  
	Ipx800();
    virtual ~Ipx800() override = default;
	
	virtual bool initProperties() override;
	virtual bool updateProperties() override;
	const char *getDefaultName() override;
	
	virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
    virtual bool ISNewNumber(const char *dev,const char *name,double values[],char *names[],int n) override;
	   
	//TO CONFIRM
	
    virtual bool ISSnoopDevice(XMLEle *root) override;
    virtual void ISGetProperties(const char *dev) ;
 protected:
	
	bool Handshake();
	bool Connect() override;
    bool Disconnect() override;
	void TimerHit() override;
	
    enum IPX800_command {
       GetR   = 1 << 0,
       GetD  = 1 << 1,
       SetR = 1 << 2,
       ClearR = 1 << 3
   } ;
       
	///////////////////////////////////////////
	// IPX800 Communication
	///////////////////////////////////////////
	bool updateIPXData();
    void updateObsStatus();
    bool readCommand(IPX800_command);
    bool writeCommand(IPX800_command, int toSet);
    bool checkAnswer();
    void readAnswer();
    void recordData(IPX800_command command);
    bool writeTCP(std::string toSend);
	bool firstFonctionTabInit();
	
    virtual bool UpdateDigitalInputs() override;
    virtual bool UpdateAnalogInputs() override; // IPX800 Analog Inputs not managed
    virtual bool UpdateDigitalOutputs() override;
    virtual bool CommandOutput(uint32_t index, OutputState command) override;
	virtual bool saveConfigItems(FILE *fp) override;
	
  private:
	bool roofPowerManagement = false;
	Connection::TCP *tcpConnection {nullptr};
	
	// TO manage Password in a next release
	//IText* getMyLogin();
    //ITextVectorProperty getMyLoginVector();
    
	ISwitchVectorProperty getMyRelayVector(int i);
    ISwitchVectorProperty getMyDigitsVector(int i);
	
	// List of possible commands 
    enum {
        UNUSED_RELAY,
        ROOF_ENGINE_POWER_SUPPLY,
        TUBE_VENTILATION,
        HEATING_RESISTOR_1,
        HEATING_RESISTOR_2,
        ROOF_CONTROL_COMMAND,
        MOUNT_POWER_SUPPLY,
        CAM_POWER_SUPPLY,
        OTHER_POWER_SUPPLY_1,
        OTHER_POWER_SUPPLY_2,
        OTHER_POWER_SUPPLY_3
        } IPXRelaysCommands;

	// List of possibles digital inputs
    enum  {
        UNUSED_DIGIT,
        DEC_AXIS_PARKED,
        RA_AXIS_PARKED,
        ROOF_OPENED,
        ROOF_CLOSED,
        ROOF_ENGINE_POWERED,
        RASPBERRY_SUPPLIED,
        MAIN_PC_SUPPLIED,
        OTHER_DIGITAL_1,
        OTHER_DIGITAL_2 } IPXDigitalRead;
	
    char tmpAnswer[8]= {0};
    bool setupParams();
    float CalcTimeLeft(timeval);

    ISState fullOpenLimitSwitch { ISS_ON };
    ISState fullClosedLimitSwitch { ISS_OFF };
    double MotionRequest { 0 };
    struct timeval MotionStart { 0, 0 };

    ISwitch RelaisInfoS[11] {};
    ISwitch Relais1InfoS[11], Relais2InfoS[11],Relais3InfoS[11],Relais4InfoS[11],Relais5InfoS[11],Relais6InfoS[11],Relais7InfoS[11],Relais8InfoS[11] {};
    ISwitchVectorProperty RelaisInfoSP[8] {};

    ISwitch DigitalInputS[10] {};
    ISwitch Digital1InputS[10], Digital2InputS[10], Digital3InputS[10], Digital4InputS[10], Digital5InputS[10], Digital6InputS[10], Digital7InputS[10], Digital8InputS[10];
    ISwitchVectorProperty DigitalInputSP[8] {};

    ISwitch Relay1StateS[2],Relay2StateS[2],Relay3StateS[2],Relay4StateS[2],Relay5StateS[2],Relay6StateS[2],Relay7StateS[2],Relay8StateS[2] ;
    ISwitchVectorProperty RelaysStatesSP[8] ;
    ISwitch Digit1StateS[2],Digit2StateS[2],Digit3StateS[2],Digit4StateS[2],Digit5StateS[2],Digit6StateS[2],Digit7StateS[2],Digit8StateS[2] ;
    ISwitchVectorProperty DigitsStatesSP[8] ;

    //TO manage Password in a next release
    //IText LoginPwdT[2];
    //ITextVectorProperty LoginPwdTP;
	//std::string myPasswd = "";
    //std::string myLogin = "";

    enum {
        ROOF_IS_OPENED ,
        ROOF_IS_CLOSED ,
        UNKNOWN_STATUS
    }
    Roof_Status;

    enum {
        RA_PARKED    ,
        DEC_PARKED   ,
        BOTH_PARKED  ,
        NONE_PARKED
    }
    Mount_Status;

	const char *ROLLOFF_TAB        = "Roll Off";
	const char *RELAYS_CONFIGURATION_TAB        = "Relays Outputs";
	const char *DIGITAL_INPUT_CONFIGURATION_TAB        = "Digital Inputs";
	const char *RAW_DATA_TAB = "Status";

	const int DIGITAL_INTPUTS = 8;
	const int RELAYS_OUTPUTS = 8;


    // Relay_Fonction_Tab provide relay output in charge of the function
    // fonctions are ordered arbitrarly as following. 
    // Only 8 functions applicable - only 8 relays output on IPX800
	// Others are spares
	int Relay_Fonction_Tab [11] = {0};
    /* 0: UNUSED_RELAY,
    ROOF_ENGINE_POWER_SUPPLY,
    TUBE_VENTILATION,
    HEATING_RESISTOR_1,
    HEATING_RESISTOR_2,
    ROOF_CONTROL_COMMAND,
    MOUNT_POWER_SUPPLY,
    CAM_POWER_SUPPLY,
    OTHER_POWER_SUPPLY_1,
    OTHER_POWER_SUPPLY_2,
    10 : OTHER_POWER_SUPPLY_3 */

    // Digital_Fonction_Tab provide digital input in charge of the function
    // fonctions are ordered arbitrarly as following. 
	// Only 8 functions applicable - only 8 digital inputs on IPX800
	// Others are spares
	int Digital_Fonction_Tab [11] = {0};
    /*
       0:  UNUSED_DIGIT,
        DEC_AXIS_PARKED,
        RA_AXIS_PARKED,
        ROOF_OPENED,
        ROOF_CLOSED,
        ROOF_ENGINE_POWERED,
        RASPBERRY_SUPPLIED,
        MAIN_PC_SUPPLIED,
        OTHER_DIGITAL_1,
        9 : OTHER_DIGITAL_2
    */
	
	// status of each relay output and digital input
	// ordered the same way in IPX800
	bool relayState[8];
    bool digitalState[8];
	
    int mount_Status = RA_PARKED | DEC_PARKED | BOTH_PARKED | NONE_PARKED;
    int roof_Status  = ROOF_IS_OPENED | ROOF_IS_CLOSED | UNKNOWN_STATUS;
	bool enginePowered = false ; //  True = on / false = Off 
	bool first_Start = false;
	
	ISwitch IPXVersionS[5];
	ISwitch roofEnginePowerS[2];
	ISwitchVectorProperty IPXVersionSP, roofEnginePowerSP;
	
	
};
