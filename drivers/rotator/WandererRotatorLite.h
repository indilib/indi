#pragma once

#include "defaultdevice.h"
#include "indirotator.h"
#include "indirotatorinterface.h"
#include "indipropertyswitch.h"
class WandererRotatorLite : public INDI::Rotator
{
public:
    WandererRotatorLite();
            
    virtual bool initProperties() override;
    virtual bool updateProperties() override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[],int n) override;
        virtual bool ISNewNumber (const char * dev, const char * name, double values[], char * names[], int n) override;
         

protected:
        const char * getDefaultName() override;
        virtual bool saveConfigItems(FILE *fp) override;
         virtual IPState MoveRotator(double angle) override;
         virtual IPState HomeRotator() override;
         virtual bool SyncRotator(double angle) override;
         virtual bool ReverseRotator(bool enabled) override;

         virtual bool AbortRotator() override;
         virtual void TimerHit() override;


         
     private:
         bool Handshake() override;
        bool sendCommand(const char *cmd);
        bool Move(const char *cmd);
        ISwitchVectorProperty HomeSP;
        ISwitch HomeS[1];
        bool SetHomePosition();
        bool haltcommand=false;
        bool ReverseState;
        double positiontemp;
        int reversecoefficient;
        bool SetRotatorBacklash(double angle) ;
        double backlash;
        double positionhistory;
        double backlashcompensation;
        double backlashcompensationcount;
        int positioncount;
         INumberVectorProperty RotatorBacklashNP;
         INumber RotatorBacklashN[1];

         ISwitch HomeRotatorS[1];
         ISwitchVectorProperty HomeRotatorSP;
        

};
 
  
 

  
 
