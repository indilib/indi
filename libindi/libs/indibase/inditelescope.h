/*******************************************************************************
  Copyright(c) 2011 Gerry Rozema, Jasem Mutlaq. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#ifndef INDI_TELESCOPE_H
#define INDI_TELESCOPE_H

#include "defaultdevice.h"

/**
 * \class INDI::Telescope
   \brief Class to provide general functionality of a telescope device.

   Developers need to subclass INDI::Telescope to implement any driver for telescopes within INDI.

   Implementing a basic telescope driver involves the child class performing the following steps:
   <ul>
   <li>If the telescope has additional properties, the child class should override initProperties and initilize the respective additional properties.</li>
   <li>Once the parent class calls Connect(), the child class attempts to connect to the telescope and return either success of failure</li>
   <li>INDI::Telescope calls updateProperties() to enable the child class to define which properties to send to the client upon connection</li>
   <li>INDI::Telescope calls ReadScopeStatus() to check the link to the telescope and update its state and position. The child class should call newRaDec() whenever
   a new value is read from the telescope.</li>
   <li>The child class should implmenet Goto() and Sync(), and Park() if applicable.</li>
   <li>INDI::Telescope calls disconnect() when the client request a disconnection. The child class should remove any additional properties it defined in updateProperties() if applicable</li>
   </ul>

\author Gerry Rozema, Jasem Mutlaq
\see TelescopeSimulator and SynScan drivers for examples of implementations of INDI::Telescope.
*/
class INDI::Telescope : public INDI::DefaultDevice
{
    private:

    public:
        Telescope();
        virtual ~Telescope();

        enum TelescopeStatus { SCOPE_IDLE, SCOPE_SLEWING, SCOPE_TRACKING, SCOPE_PARKING, SCOPE_PARKED };
        enum TelescopeMotionNS { MOTION_NORTH, MOTION_SOUTH };
        enum TelescopeMotionWE { MOTION_WEST, MOTION_EAST };

        virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
        virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
        virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
        virtual void ISGetProperties (const char *dev);

        /** \brief Called to initialize basic properties required all the time */
        virtual bool initProperties();
        /** \brief Called when connected state changes, to add/remove properties */
        virtual bool updateProperties();

        /** \brief Called when setTimer() time is up */
        virtual void TimerHit();

        /** \brief Connect to the telescope.
          \return True if connection is successful, false otherwise
        */
        virtual bool Connect();

        /** \brief Disconnect from telescope
            \return True if successful, false otherwise */
        virtual bool Disconnect();

        /** \brief INDI::Telescope implementation of Connect() assumes 9600 baud, 8 bit word, even parity, and no stop bit. Override function if communication paramaters
          are different
          \param port Port to connect to
          \return True if connection is successful, false otherwise
          \warning Do not call this function directly, it is called by INDI::Telescope Connect() function.
        */
        virtual bool Connect(const char *port);

        protected:

        virtual bool saveConfigItems(FILE *fp);

        /** \brief The child class calls this function when it has updates */
        void NewRaDec(double ra,double dec);

        /** \brief Read telescope status.
         This function checks the following:
         <ol>
           <li>Check if the link to the telescope is alive.</li>
           <li>Update telescope status: Idle, Slewing, Parking..etc.</li>
           <li>Read coordinates</li>
         </ol>
          \return True if reading scope status is OK, false if an error is encounterd.
          \note This function is not implemented in INDI::Telescope, it must be implemented in the child class */
        virtual bool ReadScopeStatus();

        /** \brief Move the scope to the supplied RA and DEC coordinates
            \return True if successful, false otherewise
            \note This function is not implemented in INDI::Telescope, it must be implemented in the child class
        */
        virtual bool Goto(double ra,double dec);

        /** \brief Set the telescope current RA and DEC coordinates to the supplied RA and DEC coordinates
            \return True if successful, false otherewise
            \note This function is not implemented in INDI::Telescope, it must be implemented in the child class
        */
        virtual bool Sync(double ra,double dec);

        /** \brief Move the telescope in the direction dir.
            \return True if successful, false otherewise
            \note This function is not implemented in INDI::Telescope, it must be implemented in the child class
        */
        virtual bool MoveNS(TelescopeMotionNS dir);

        /** \brief Move the telescope in the direction dir.
            \return True if successful, false otherewise
            \note This function is not implemented in INDI::Telescope, it must be implemented in the child class
        */
        virtual bool MoveWE(TelescopeMotionWE dir);

        /** \brief Part the telescope to its home position.
            \return True if successful, false otherewise
            \note This function is not implemented in INDI::Telescope, it must be implemented in the child class
        */
        virtual bool Park();

        virtual bool Abort();


        //  Since every mount I know of actually uses a serial port for control
        //  We put the serial helper into the base telescope class
        //  One less piece to worry about in the hardware specific
        //  low level stuff
        int PortFD;

        //  This is a variable filled in by the ReadStatus telescope
        //  low level code, used to report current state
        //  are we slewing, tracking, or parked.
        TelescopeStatus TrackState;

        //  All telescopes should produce equatorial co-ordinates
        INumberVectorProperty EqNV;
        INumber EqN[2];

        //  And we need a vector to store requests, ie, where are we asked to go
        INumberVectorProperty EqReqNV;
        INumber EqReqN[2];

        ISwitchVectorProperty AbortSV; // Abort motion
        ISwitch AbortS[1];

        ISwitchVectorProperty CoordSV; //  A switch vector that stores how we should readct
        ISwitch CoordS[3];              //  On a coord_set message, sync, or slew

        ISwitchVectorProperty ConfigSV; //  A switch vector that stores how we should readct
        ISwitch ConfigS[3];              //  On a coord_set message, sync, or slew

        INumberVectorProperty LocationNV;   //  A number vector that stores lattitude and longitude
        INumber LocationN[2];

        ISwitchVectorProperty ParkSV; //  A Switch in the client interface to park the scope
        ISwitch ParkS[1];

        ITextVectorProperty PortTP; //  A text vector that stores out physical port name
        IText PortT[1];

        ISwitch MovementNSS[2];     // A switch for North/South motion
        ISwitchVectorProperty MovementNSSP;

        ISwitch MovementWES[2];     // A switch for West/East motion
        ISwitchVectorProperty MovementWESP;

        INumber ScopeParametersN[4];
        INumberVectorProperty ScopeParametersNP;


};

#endif // INDI::Telescope_H
