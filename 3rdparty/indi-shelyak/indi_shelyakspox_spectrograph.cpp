/*******************************************************************************
  Copyright(c) 2017 Simon Holmbo. All rights reserved.
  Copyright(c) 2018 Jean-Baptiste Butet. All rights reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <memory>

#include "indicom.h"
#include "indi_shelyakspox_spectrograph.h"
#include "config.h"

//const char *SPECTROGRAPH_SETTINGS_TAB = "Spectrograph Settings";
const char *CALIBRATION_UNIT_TAB      = "Calibration Module";

std::unique_ptr<ShelyakSpox> shelyakSpox(new ShelyakSpox()); // create std:unique_ptr (smart pointer) to  our spectrograph object

void ISGetProperties(const char *dev)
{
  shelyakSpox->ISGetProperties(dev);
}

/* The next 4 functions are executed when the indiserver requests a change of
 * one of the properties. We pass the request on to our spectrograph object.
 */
void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
  shelyakSpox->ISNewSwitch(dev, name, states, names, num);
}
void ISNewText( const char *dev, const char *name, char *texts[], char *names[], int num)
{
  shelyakSpox->ISNewText(dev, name, texts, names, num);
}
void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
  shelyakSpox->ISNewNumber(dev, name, values, names, num);
}
void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
  INDI_UNUSED(dev);
  INDI_UNUSED(name);
  INDI_UNUSED(sizes);
  INDI_UNUSED(blobsizes);
  INDI_UNUSED(blobs);
  INDI_UNUSED(formats);
  INDI_UNUSED(names);
  INDI_UNUSED(n);
}

/* This function is fired when a property we are snooping on is changed. We
 * pass it on to our spectrograph object.
 */
void ISSnoopDevice (XMLEle *root)
{
  shelyakSpox->ISSnoopDevice(root);
}

ShelyakSpox::ShelyakSpox()
{
  PortFD = -1;

  setVersion(SHELYAK_SPOX_VERSION_MAJOR, SHELYAK_SPOX_VERSION_MINOR);
}

ShelyakSpox::~ShelyakSpox()
{

}

/* Returns the name of the device. */
const char *ShelyakSpox::getDefaultName()
{
  return (char *)"Shelyak Spox";
}

/* Initialize and setup all properties on startup. */
bool ShelyakSpox::initProperties()
{
  INDI::DefaultDevice::initProperties();

  //--------------------------------------------------------------------------------
  // Calibration Unit
  //--------------------------------------------------------------------------------


  // setup the lamp switches
  
  IUFillSwitch(&LampS[2], "CALIBRATION", "CALIBRATION", ISS_OFF);
  IUFillSwitch(&LampS[1], "FLAT", "FLAT", ISS_OFF);
  IUFillSwitch(&LampS[0], "DARK", "DARK", ISS_OFF);
  IUFillSwitchVector(&LampSP, LampS, 3, getDeviceName(), "CALIBRATION", "Calibration lamps", CALIBRATION_UNIT_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);
   

  //--------------------------------------------------------------------------------
  // Options
  //--------------------------------------------------------------------------------

  // setup the text input for the serial port
  IUFillText(&PortT[0], "PORT", "Port", "/dev/ttyUSB0");
  IUFillTextVector(&PortTP, PortT, 1, getDeviceName(), "DEVICE_PORT", "Ports", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);


  return true;
}

void ShelyakSpox::ISGetProperties(const char *dev)
{
  INDI::DefaultDevice::ISGetProperties(dev);
  defineText(&PortTP);
  defineNumber(&SettingsNP);
  loadConfig(true, PortTP.name);
}

bool ShelyakSpox::updateProperties()
{
  INDI::DefaultDevice::updateProperties();
  if (isConnected())
  {
    // create properties if we are connected
    defineSwitch(&LampSP);
  }
  else
  {
    // delete properties if we arent connected
    deleteProperty(LampSP.name);
  }
  return true;
}

bool ShelyakSpox::Connect()
{
  int rc;
  char errMsg[MAXRBUF];
  if ((rc = tty_connect(PortT[0].text, 9600, 8, 0, 1, &PortFD)) != TTY_OK)
  {
    tty_error_msg(rc, errMsg, MAXRBUF);
    DEBUGF(INDI::Logger::DBG_ERROR, "Failed to connect to port %s. Error: %s", PortT[0].text, errMsg);
    return false;
  }
  DEBUGF(INDI::Logger::DBG_SESSION, "%s is online.", getDeviceName());
  sleep(0.5); 
  //read the serial to flush welcome message of SPOX.
    char line[80];

    int bytes_read=0;
    int tty_rc = tty_nread_section(PortFD, line, 80, 0x0a, 3, &bytes_read);
    
    line[bytes_read] = '\n';
    DEBUGF(INDI::Logger::DBG_SESSION, "bytes read :  %i", bytes_read);

  // Actually nothing is done with theses informations. But code is here.
  //pollingLamps();
  
    resetLamps();
  
  return true;
}

// bool ShelyakSpox::pollingLamps()
// {//send polling message
//     lastLampOn = "None";
// 
//     sleep(0.1); 
//     int rc, nbytes_written;
//     char c[3] = {'1','?',0x0a};
// 
//     if ((rc = tty_write(PortFD, c, 3, &nbytes_written)) != TTY_OK)
// 
//       {
//         char errmsg[MAXRBUF];
//         tty_error_msg(rc, errmsg, MAXRBUF);
//         DEBUGF(INDI::Logger::DBG_ERROR, "error: %s.", errmsg);
//         return false;
//       } else {
//           DEBUGF(INDI::Logger::DBG_SESSION, "sent on serial: %s.", c);
// 
//       }
//       sleep(0.1);                                    
//       
// //read message send in response by spectrometer
//     char lineCalib[80];
// 
//     int bytes_read=0;
//     int tty_rc = tty_nread_section(PortFD, lineCalib, 80, 0x0a, 3, &bytes_read);
//     if (tty_rc < 0)
//     {
//         LOGF_ERROR("Error getting device readings: %s", strerror(errno));
//         return false;
//     }
//     lineCalib[bytes_read] = '\n';
//   
// 
//   DEBUGF(INDI::Logger::DBG_SESSION,"State of Calib lamp: #%s#", lineCalib );
//      
//      
//     sleep(0.1); 
//     int rc2, nbytes_written2;
//     char c2[3] = {'2','?',0x0a};
// 
//     if ((rc2 = tty_write(PortFD, c2, 3, &nbytes_written2)) != TTY_OK)
//       {
//         char errmsg[MAXRBUF];
//         tty_error_msg(rc, errmsg, MAXRBUF);
//         DEBUGF(INDI::Logger::DBG_ERROR, "error: %s.", errmsg);
//         return false;
//       } else {
//           DEBUGF(INDI::Logger::DBG_SESSION, "sent on serial: %s.", c2);
// 
//       }
//       sleep(0.1);                   
//       
// //read message send in response by spectrometer
//     char lineFlat[80];
// 
//     int bytes_read2=0;
//     int tty_rc2 = tty_nread_section(PortFD, lineFlat, 80, 0x0a, 3, &bytes_read2);
//     if (tty_rc < 0)
//     {
//         LOGF_ERROR("Error getting device readings: %s", strerror(errno));
//         return false;
//     }
//     lineFlat[bytes_read] = '\n';
//   
// 
//   DEBUGF(INDI::Logger::DBG_SESSION,"State of Flat lamp: #%s#", lineFlat );
//     
//      if (strcmp(lineCalib,"11")==13)  {
//          lastLampOn = "CALIB";
//      }
//      
//      if (strcmp(lineFlat,"21")==13) {
//          lastLampOn = "FLAT";
//      }
//     
//      if ((strcmp(lineCalib,"11")==13)&&(strcmp(lineFlat,"21")== 13)) {
//          lastLampOn = "DARK";
//      }
//     
//   DEBUGF(INDI::Logger::DBG_SESSION,"Spectrometer has %s state", lastLampOn.c_str());   
//      
//    return true;
//     
// }


bool ShelyakSpox::Disconnect()
{ 
  
  sleep(1); // wait for the calibration unit to actually flip the switch  
    
  tty_disconnect(PortFD);
  DEBUGF(INDI::Logger::DBG_SESSION, "%s is offline.", getDeviceName());
  return true;
}

/* Handle a request to change a switch. */

bool ShelyakSpox::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
  if (!strcmp(dev, getDeviceName())) // check if the message is for our device
  {
    if (!strcmp(LampSP.name, name)) // check if its lamp request
    {
        
      LampSP.s = IPS_OK; // set state to ok (change later if something goes wrong)
      for (int i=0; i<n; i++)
      {
        ISwitch *s = IUFindSwitch(&LampSP, names[i]);

        if (states[i] != s->s) { // check if state has changed
          bool rc = calibrationUnitCommand(COMMANDS[states[i]],PARAMETERS[names[i]]);
          if (!rc) LampSP.s = IPS_ALERT;
            
        }
      }
      IUUpdateSwitch(&LampSP, states, names, n); // update lamps
      IDSetSwitch(&LampSP, NULL); // tell clients to update
      return true;
    }

  }

  return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n); // send it to the parent classes
}


/* Handle a request to change text. */
bool ShelyakSpox::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
  if(!strcmp(dev, getDeviceName())) // check if the message is for our device
  {
    if (!strcmp(PortTP.name, name)) //check if is a port change request
    {
      IUUpdateText(&PortTP, texts, names, n); // update port
      PortTP.s = IPS_OK; // set state to ok
      IDSetText(&PortTP, NULL); // tell clients to update the port
      return true;
    }
  }

  return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

/* Construct a reset command*/
bool ShelyakSpox::resetLamps()
{
     // wait for the calibration unit to actually flip the switch
    int rc, nbytes_written;
    char c[3] = {'0','0',0x0a};

    if ((rc = tty_write(PortFD, c, 3, &nbytes_written)) != TTY_OK)

      {
        char errmsg[MAXRBUF];
        tty_error_msg(rc, errmsg, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "error: %s.", errmsg);
        return false;
      } else {
          DEBUGF(INDI::Logger::DBG_SESSION, "RESET : sent on serial: %s.", c);

      }
      sleep(1); // wait for the calibration unit to actually flip the switch
      lastLampOn = "None";
      sleep(0.5);
      return true;
}




/* Construct a command and send it to the spectrograph. It doesn't return
 * anything so we have to sleep until we know it has flipped the switch.
 */
bool ShelyakSpox::calibrationUnitCommand(char command, char parameter)
{
int rc, nbytes_written;
    
if (parameter==0x33){ //special for dark : have to put both lamps on
    char cmd[6] = {0x31,0x31,0x0a,0x32,0x31,0x0a};//"11\n21\n" 

    if(command==0x31){// dark is on
        DEBUGF(INDI::Logger::DBG_SESSION, "sent on serial: %s.", "dark is on");
        lastLampOn = "Dark";
        
        char c[3] = {parameter,command,0x0a};

        if ((rc = tty_write(PortFD, c, 3, &nbytes_written)) != TTY_OK)

        {
            char errmsg[MAXRBUF];
            tty_error_msg(rc, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "error: %s.", errmsg);
            return false;
        } else {
            DEBUGF(INDI::Logger::DBG_SESSION, "sent on serial: %s.", c);

        }
        sleep(1); // wait for the calibration unit to actually flip the switch
        

        if ((rc = tty_write(PortFD, cmd, 6, &nbytes_written)) != TTY_OK)

        {
            char errmsg[MAXRBUF];
            tty_error_msg(rc, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "error: %s.", errmsg);
            return false;
        } else {
            DEBUGF(INDI::Logger::DBG_SESSION, "sent on serial: %s.", cmd);

        }
        sleep(1); // wait for the calibration unit to actually flip the switch
        return true;
    } else {
        DEBUGF(INDI::Logger::DBG_SESSION, "sent on serial: %s.", "dark is off");
        resetLamps();
        
    return true;
    }
    

}

//other lamps 
        char c[3] = {parameter,command,0x0a};
        
        if (strcmp(lastLampOn,"Dark")!=0){ //if dark is set before, lamps are not shut off as is still done.
        
            if ((rc = tty_write(PortFD, c, 3, &nbytes_written)) != TTY_OK)

            {
                char errmsg[MAXRBUF];
                tty_error_msg(rc, errmsg, MAXRBUF);
                DEBUGF(INDI::Logger::DBG_ERROR, "error: %s.", errmsg);
                return false;
            } else {
                DEBUGF(INDI::Logger::DBG_SESSION, "sent on serial: %s.", c);

            }
            sleep(0.5); // wait for the calibration unit to actually flip the switch
            
            
            if(command!=0x31){//on lamp is on

                DEBUGF(INDI::Logger::DBG_SESSION, "last lamp is: %s.", lastLampOn);
            }
            
        }
        return true;
}
