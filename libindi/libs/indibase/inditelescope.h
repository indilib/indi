/*******************************************************************************
  Copyright(c) 2010 Gerry Rozema. All rights reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

#ifndef INDI_TELESCOPE_H
#define INDI_TELESCOPE_H

#include "defaultdriver.h"

#define PARKED 0
#define SLEWING 1
#define TRACKING 2
#define PARKING 3

class INDI::Telescope : public INDI::DefaultDriver
{
    protected:
        //bool Connected;

    private:

    public:
        Telescope();
        virtual ~Telescope();


        //  All telescopes should produce equatorial co-ordinates
        INumberVectorProperty EqNV;
        INumber EqN[2];

        //  And we need a vector to store requests, ie, where are we asked to go
        INumberVectorProperty EqReqNV;
        INumber EqReqN[2];

        ISwitchVectorProperty CoordSV; //  A switch vector that stores how we should readct
        ISwitch CoordS[3];              //  On a coord_set message, sync, or slew

        INumberVectorProperty LocationNV;   //  A number vector that stores lattitude and longitude
        INumber LocationN[2];

        ISwitchVectorProperty ParkSV; //  A Switch in the client interface to park the scope
        ISwitch ParkS[1];

        //  I dont know of any telescopes that dont
        //  need a port for connection
        //  So lets put all the port connect framework
        //  into our generic telescope super class
        ITextVectorProperty PortTV; //  A text vector that stores out physical port name
        IText PortT[1];


        //  Ok, we do need our virtual functions from the base class for processing
        //  client requests
        //  We process numbers,switches and text in the telescope
        //  These are the base IndiDevice overrides we process
        virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
        virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
        virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
        virtual void ISGetProperties (const char *dev);

        //  overrides of base class virtual functions
        //  that are specific to our way of implementing Indi
        virtual bool initProperties();      //  Called to initialize basic properties required all the time
        virtual bool updateProperties();    //  Called when connected state changes, to add/remove properties

        virtual void TimerHit();
        virtual bool Connect();
        virtual bool Disconnect();

        virtual bool Connect(const char *);


        //  Since every mount I know of actually uses a serial port for control
        //  We put the serial helper into the base telescope class
        //  One less piece to worry about in the hardware specific
        //  low level stuff
        int PortFD;


        //  This is a variable filled in by the ReadStatus telescope
        //  low level code, used to report current state
        //  are we slewing, tracking, or parked.
        int TrackState;


        //  These functions are telescope specific
        //  and meant to make life really easy for deriving
        //  hardware specific telescope classes
        int NewRaDec(double,double);    //  The child class will call this when it has updates


        //  And these are the hardware specific functions our children need to override
        //  They are not pure virtual, because, not all functions are relavent for all types
        //  of mount, ie, a Goto is not relavent for a Push-to mount

        /** \brief Read telescope status.
         This function checks the following:
         <ol>
           <li>Check if the link to the telescope is alive.</li>
           <li>Update telescope status: Idle, Slewing, Parking..etc.</li>
           <li>Read coordinates</li>
         </ol>
          \return True if reading scope status is OK, false if an error is encounterd. */
        virtual bool ReadScopeStatus();

        virtual bool Goto(double ra,double dec);
        virtual bool Sync(double ra,double dec);
        virtual bool Park();

};

#endif // INDI::Telescope_H
