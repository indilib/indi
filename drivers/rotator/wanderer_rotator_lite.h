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




protected:
        const char * getDefaultName() override;
         virtual IPState MoveRotator(double angle) override;
         virtual IPState HomeRotator() override;
         virtual bool ReverseRotator(bool enabled) override;

         virtual bool AbortRotator() override;
         virtual void TimerHit() override;
        virtual bool SetRotatorBacklash(int32_t steps) override;
        virtual bool SetRotatorBacklashEnabled(bool enabled) override;


         
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
        double backlash;
        double positionhistory;
        double backlashcompensation;
        double backlashcompensationcount;
        int positioncount;


        

};
 
  
 

  
 
