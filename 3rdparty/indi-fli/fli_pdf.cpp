#if 0
    FLI PDF
    INDI Interface for Finger Lakes Instrument Focusers
    Copyright (C) 2003-2012 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

#endif

#include <memory>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>

#include "indidevapi.h"
#include "eventloop.h"

#include "fli_pdf.h"

#define POLLMS		1000		/* Polling time (ms) */

std::auto_ptr<FLIPDF> fliPDF(0);

const flidomain_t Domains[] = { FLIDOMAIN_USB, FLIDOMAIN_SERIAL, FLIDOMAIN_PARALLEL_PORT,  FLIDOMAIN_INET };

 void ISInit()
 {
    static int isInit =0;

    if (isInit == 1)
        return;

     isInit = 1;
     if(fliPDF.get() == 0) fliPDF.reset(new FLIPDF());

 }

 void ISGetProperties(const char *dev)
 {
         ISInit();
         fliPDF->ISGetProperties(dev);
 }

 void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
 {
         ISInit();
         fliPDF->ISNewSwitch(dev, name, states, names, num);
 }

 void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
 {
         ISInit();
         fliPDF->ISNewText(dev, name, texts, names, num);
 }

 void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
 {
         ISInit();
         fliPDF->ISNewNumber(dev, name, values, names, num);
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
 void ISSnoopDevice (XMLEle *root)
 {
     ISInit();
     fliPDF->ISSnoopDevice(root);
 }


FLIPDF::FLIPDF()
{
   sim = false;
   setFocuserFeatures(true, true, false, false);
}


FLIPDF::~FLIPDF()
{

}

const char * FLIPDF::getDefaultName()
{
    return (char *)"FLI PDF";
}

bool FLIPDF::initProperties()
{
    // Init parent properties first
    INDI::Focuser::initProperties();

    IUFillSwitch(&PortS[0], "USB", "USB", ISS_ON);
    IUFillSwitch(&PortS[1], "SERIAL", "Serial", ISS_OFF);
    IUFillSwitch(&PortS[2], "PARALLEL", "Parallel", ISS_OFF);
    IUFillSwitch(&PortS[3], "INET", "INet", ISS_OFF);
    IUFillSwitchVector(&PortSP, PortS, 4, getDeviceName(), "PORTS", "Port", MAIN_CONTROL_TAB, IP_WO, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&HomeS[0], "Go", "", ISS_OFF);
    IUFillSwitchVector(&HomeSP, HomeS, 1, getDeviceName(), "Home", "", MAIN_CONTROL_TAB, IP_WO, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillText(&FocusInfoT[0],"Model","","");
    IUFillText(&FocusInfoT[1],"HW Rev","","");
    IUFillText(&FocusInfoT[2],"FW Rev","","");
    IUFillTextVector(&FocusInfoTP,FocusInfoT,3,getDeviceName(),"Model","","Focuser Info",IP_RO,60,IPS_IDLE);

}


void FLIPDF::ISGetProperties(const char *dev)
{
    INDI::Focuser::ISGetProperties(dev);

    defineSwitch(&PortSP);

    addAuxControls();
}

bool FLIPDF::updateProperties()
{

    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineNumber(&FocusAbsPosNP);
        defineNumber(&FocusRelPosNP);
        defineSwitch(&HomeSP);
        defineText(&FocusInfoTP);
        setupParams();

        timerID = SetTimer(POLLMS);
    }
    else
    {
        deleteProperty(FocusAbsPosNP.name);
        deleteProperty(FocusRelPosNP.name);
        deleteProperty(HomeSP.name);
        deleteProperty(FocusInfoTP.name);

        rmTimer(timerID);
    }

    return true;
}


bool FLIPDF::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{

    if(strcmp(dev,getDeviceName())==0)
    {

        /* Home */
        if (!strcmp (name, HomeSP.name))
        {
          if (IUUpdateSwitch(&HomeSP, states, names, n) < 0)
              return false;

          goHomePosition();
          return true;
        }

        /* Ports */
        if (!strcmp (name, PortSP.name))
        {
          if (IUUpdateSwitch(&PortSP, states, names, n) < 0)
              return false;

          PortSP.s = IPS_OK;
          IDSetSwitch(&PortSP, NULL);
          return true;
        }

    }

    //  Nobody has claimed this, so, ignore it
    return INDI::Focuser::ISNewSwitch(dev,name,states,names,n);
}

bool FLIPDF::Connect()
{
    int err=0;

    IDMessage(getDeviceName(), "Attempting to find the FLI PDF...");

    sim = isSimulation();

    if (sim)
        return true;

    if (isDebug())
    {
        IDLog ("Connecting PDF\n");
        IDLog("Attempting to find the focuser\n");
    }

    int portSwitchIndex = IUFindOnSwitchIndex(&PortSP);

    if (findFLIPDF(Domains[portSwitchIndex]) == false)
    {
        IDMessage(getDeviceName(), "Error: no focusers were detected.");

        if (isDebug())
            IDLog("Error: no focusers were detected.\n");

        return false;
    }



    if ((err = FLIOpen(&fli_dev, FLIFocus.name, FLIDEVICE_FOCUSER | FLIFocus.domain)))
    {

        IDMessage(getDeviceName(), "Error: FLIOpen() failed. %s.", strerror( (int) -err));

        if (isDebug())
            IDLog("Error: FLIOpen() failed. %s.\n", strerror( (int) -err));

        return false;
    }

    /* Success! */
    IDMessage(getDeviceName(), "Focuser is online. Retrieving basic data.");
    if (isDebug())
       IDLog("Focuser is online. Retrieving basic data.\n");

        return true;
}


bool FLIPDF::Disconnect()
{
    int err;

    if (sim)
        return true;

    if ((err = FLIClose(fli_dev)))
    {
        IDMessage(getDeviceName(), "Error: FLIClose() failed. %s.", strerror( (int) -err));

        if (isDebug())
            IDLog("Error: FLIClose() failed. %s.\n", strerror( (int) -err));

        return false;
    }

    IDMessage(getDeviceName(), "Focuser is offline.");
    return true;
}


bool FLIPDF::setupParams()
{

    int err=0;

    if (isDebug())
        IDLog("In setupParams\n");

    char hw_rev[16], fw_rev[16];

    //////////////////////
    // 1. Get Focuser Model
    //////////////////////
    if (!sim && (err = FLIGetModel (fli_dev, FLIFocus.model, 200))) //ToDo: lazy
    {
      IDMessage(getDeviceName(), "FLIGetModel() failed. %s.", strerror((int)-err));

      if (isDebug())
        IDLog("FLIGetModel() failed. %s.\n", strerror((int)-err));
	return false;
    }

    if (sim)
        IUSaveText(&FocusInfoT[0], getDeviceName());
    else
        IUSaveText(&FocusInfoT[0], FLIFocus.model);

    ///////////////////////////
    // 2. Get Hardware revision
    ///////////////////////////
    if (sim)
        FLIFocus.HWRevision = 1;
    else if (( err = FLIGetHWRevision(fli_dev, &FLIFocus.HWRevision)))
    {
      IDMessage(getDeviceName(), "FLIGetHWRevision() failed. %s.", strerror((int)-err));

      if (isDebug())
        IDLog("FLIGetHWRevision() failed. %s.\n", strerror((int)-err));

      return false;
    }

    snprintf(hw_rev, 16, "%ld", FLIFocus.HWRevision);
    IUSaveText(&FocusInfoT[1], hw_rev);

    ///////////////////////////
    // 3. Get Firmware revision
    ///////////////////////////
    if (sim)
        FLIFocus.FWRevision = 1;
    else if (( err = FLIGetFWRevision(fli_dev, &FLIFocus.FWRevision)))
    {
      IDMessage(getDeviceName(), "FLIGetFWRevision() failed. %s.", strerror((int)-err));

      if (isDebug())
        IDLog("FLIGetFWRevision() failed. %s.\n", strerror((int)-err));

      return false;
    }

    snprintf(fw_rev, 16, "%ld", FLIFocus.FWRevision);
    IUSaveText(&FocusInfoT[2], fw_rev);


    IDSetText(&FocusInfoTP, NULL);
    ///////////////////////////
    // 4. Focuser position
    ///////////////////////////
    if (sim)
        FLIFocus.current_pos = 3500;
    else if (( err = FLIGetStepperPosition(fli_dev, &FLIFocus.current_pos)))
    {
        IDMessage(getDeviceName(), "FLIGetStepperPosition() failed. %s.", strerror((int)-err));

        if (isDebug())
            IDLog("FLIGetStepperPosition() failed. %s.\n", strerror((int)-err));
        return false;
    }

    ///////////////////////////
    // 5. Focuser max limit
    ///////////////////////////
    if (sim)
        FLIFocus.max_pos = 50000;
    else if (( err = FLIGetFocuserExtent(fli_dev, &FLIFocus.max_pos)))
    {
        IDMessage(getDeviceName(), "FLIGetFocuserExtent() failed. %s.", strerror((int)-err));

        if (isDebug())
            IDLog("FLIGetFocuserExtent() failed. %s.\n", strerror((int)-err));
        return false;
    }

    FocusAbsPosN[0].min = 1;
    FocusAbsPosN[0].max = FLIFocus.max_pos;
    FocusAbsPosN[0].value = FLIFocus.current_pos;

    IUUpdateMinMax(&FocusAbsPosNP);
    IDSetNumber(&FocusAbsPosNP, "Setting intial absolute position");

    FocusRelPosN[0].min   = 1.;
    FocusRelPosN[0].max   = FLIFocus.max_pos;
    FocusRelPosN[0].value = 0.;

    IUUpdateMinMax(&FocusRelPosNP);
    IDSetNumber(&FocusRelPosNP, "Setting initial relative position");

    /////////////////////////////////////////
    // 6. Focuser speed is set to 100 tick/sec
    //////////////////////////////////////////
    FocusSpeedN[0].value= 100;
    IDSetNumber(&FocusSpeedNP, "Setting initial speed");

    return true;
}

void FLIPDF::goHomePosition()
{
   int err=0;

    if (!sim && (err = FLIHomeFocuser(fli_dev)))
    {
        IDMessage(getDeviceName(), "FLIHomeFocuser() failed. %s.", strerror((int)-err));

        if (isDebug())
            IDLog("FLIHomeFocuser() failed. %s.", strerror((int)-err));

        return;
    }

    HomeSP.s = IPS_OK;
    IUResetSwitch(&HomeSP);
    IDSetSwitch(&HomeSP, "Moving to home position...");
}

void FLIPDF::TimerHit()
{
    int timerID=-1;
    int err=0;

    if(isConnected() == false)
        return;  //  No need to Home timer if we are not connected anymore

    if (InStep)
    {
        if (sim)
        {
            if (FLIFocus.current_pos < StepRequest)
            {
                FLIFocus.current_pos += 250;
                if (FLIFocus.current_pos > StepRequest)
                    FLIFocus.current_pos = StepRequest;
            }
            else
            {
                FLIFocus.current_pos -= 250;
                if (FLIFocus.current_pos < StepRequest)
                    FLIFocus.current_pos = StepRequest;

            }
        }
	// while moving, display the remaing steps
        else if ((err = FLIGetStepsRemaining(fli_dev, &FLIFocus.steps_remaing)))
        {
            IDMessage(getDeviceName(), "FLIGetStepsRemaining() failed. %s.", strerror((int)-err));

            if (isDebug())
                IDLog("FLIGetStepsRemaining() failed. %s.", strerror((int)-err));

            SetTimer(POLLMS);
            return;
        }
        if (!FLIFocus.steps_remaing)
        {
            InStep = false;
            FocusAbsPosNP.s = IPS_OK;
            if (FocusRelPosNP.s == IPS_BUSY)
            {
                FocusRelPosNP.s = IPS_OK;
                IDSetNumber(&FocusRelPosNP, NULL);
            }
        }

        FocusAbsPosN[0].value = FLIFocus.steps_remaing;
        IDSetNumber(&FocusAbsPosNP, NULL);

    } else // we need to display the current position after move finished
    {
        if (( err = FLIGetStepperPosition(fli_dev, &FLIFocus.current_pos)))
        {
           IDMessage(getDeviceName(), "FLIGetStepperPosition() failed. %s.", strerror((int)-err));

           if (isDebug())
              IDLog("FLIGetStepperPosition() failed. %s.\n", strerror((int)-err));
           return;
        }
        FocusAbsPosN[0].value = FLIFocus.current_pos;
        IDSetNumber(&FocusAbsPosNP, NULL);
    }

    if (timerID == -1)
        SetTimer(POLLMS);
    return;
}

int FLIPDF::MoveAbs(int targetTicks)
{
    int err=0;

    if (targetTicks < FocusAbsPosN[0].min || targetTicks > FocusAbsPosN[0].max)
    {
        IDMessage(getDeviceName(), "Error, requested absolute position is out of range.");
        return -1;
    }
    long current;
    if (( err = FLIGetStepperPosition(fli_dev, &current)))
    {
        IDMessage(getDeviceName(), "FLIPDF::MoveAbs: FLIGetStepperPosition() failed. %s.", strerror((int)-err));

        if (isDebug())
            IDLog("FLIPDF::MoveAbs: FLIGetStepperPosition() failed. %s.\n", strerror((int)-err));
        return false;
    }
    err = FLIStepMotorAsync(fli_dev, (targetTicks - current));
    if (!sim && (err))
    {
        IDMessage(getDeviceName(), "FLIStepMotor() failed. %s.", strerror((int)-err));

        if (isDebug())
            IDLog("FLIStepMotor() failed. %s.", strerror((int)-err));
        return false;
    }

    StepRequest = targetTicks;
    InStep      = true;

    // We are still moving, didn't reach target yet
    return 1;
}

int FLIPDF::MoveRel(FocusDirection dir, unsigned int ticks)
{

      long cur_rpos=0 ;
      long new_rpos = 0 ;

      cur_rpos= FLIFocus.current_pos;

       if( dir == FOCUS_INWARD)
           new_rpos = cur_rpos+ticks;
       else
           new_rpos = cur_rpos-ticks;

       return MoveAbs(new_rpos);
}

bool FLIPDF::findFLIPDF(flidomain_t domain)
{
  char **tmplist;
  long err;

  if (isDebug())
    IDLog("In find Focuser, the domain is %ld\n", domain);

  if (( err = FLIList(domain | FLIDEVICE_FOCUSER, &tmplist)))
  {
      if (isDebug())
        IDLog("FLIList() failed. %s\n", strerror((int)-err));
    return false;
  }

  if (tmplist != NULL && tmplist[0] != NULL)
  {

    for (int i = 0; tmplist[i] != NULL; i++)
    {
      for (int j = 0; tmplist[i][j] != '\0'; j++)
            if (tmplist[i][j] == ';')
            {
                tmplist[i][j] = '\0';
                break;
            }
    }

   FLIFocus.domain = domain;

    switch (domain)
    {
        case FLIDOMAIN_PARALLEL_PORT:
            FLIFocus.dname = strdup("parallel port");
            break;

        case FLIDOMAIN_USB:
            FLIFocus.dname = strdup("USB");
            break;

        case FLIDOMAIN_SERIAL:
            FLIFocus.dname = strdup("serial");
            break;

        case FLIDOMAIN_INET:
            FLIFocus.dname = strdup("inet");
            break;

        default:
            FLIFocus.dname = strdup("Unknown domain");
    }

      FLIFocus.name = strdup(tmplist[0]);

     if ((err = FLIFreeList(tmplist)))
     {
         if (isDebug())
            IDLog("FLIFreeList() failed. %s.\n", strerror((int)-err));
       return false;
     }

   } /* end if */
   else
   {
     if ((err = FLIFreeList(tmplist)))
     {
         if (isDebug())
            IDLog("FLIFreeList() failed. %s.\n", strerror((int)-err));
       return false;
     }

     return false;
   }

  if (isDebug())
    IDLog("FindFLIPDF() finished successfully.\n");

  return true;
}

