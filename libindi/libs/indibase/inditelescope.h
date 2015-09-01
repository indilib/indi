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

#include <libnova.h>

#include "defaultdevice.h"
#include "indicontroller.h"

/**
 * \class INDI::Telescope
   \brief Class to provide general functionality of a telescope device.

   Developers need to subclass INDI::Telescope to implement any driver for telescopes within INDI.

   Implementing a basic telescope driver involves the child class performing the following steps:
   <ul>
   <li>The child class should define the telescope capabilities via the TelescopeCapability structure and sets in the default constructor.</li>
   <li>If the telescope has additional properties, the child class should override initProperties and initilize the respective additional properties.</li>
   <li>Once the parent class calls Connect(), the child class attempts to connect to the telescope and return either success of failure</li>
   <li>INDI::Telescope calls updateProperties() to enable the child class to define which properties to send to the client upon connection</li>
   <li>INDI::Telescope calls ReadScopeStatus() to check the link to the telescope and update its state and position. The child class should call newRaDec() whenever
   a new value is read from the telescope.</li>
   <li>The child class should implmenet Goto() and Sync(), and Park()/UnPark() if applicable.</li>
   <li>INDI::Telescope calls disconnect() when the client request a disconnection. The child class should remove any additional properties it defined in updateProperties() if applicable</li>
   </ul>

\author Jasem Mutlaq, Gerry Rozema
\see TelescopeSimulator and SynScan drivers for examples of implementations of INDI::Telescope.
*/
class INDI::Telescope : public INDI::DefaultDevice
{
    public:

        enum TelescopeStatus { SCOPE_IDLE, SCOPE_SLEWING, SCOPE_TRACKING, SCOPE_PARKING, SCOPE_PARKED };
        enum TelescopeMotionCommand { MOTION_START, MOTION_STOP };
        enum TelescopeSlewRate  { SLEW_GUIDE, SLEW_CENTERING, SLEW_FIND, SLEW_MAX };
        enum TelescopeTrackMode  { TRACK_SIDEREAL, TRACK_SOLAR, TRACK_LUNAR, TRACK_CUSTOM };
        enum TelescopeParkData  { PARK_NONE, PARK_RA_DEC, PARK_AZ_ALT, PARK_RA_DEC_ENCODER, PARK_AZ_ALT_ENCODER };
        enum TelescopeLocation { LOCATION_LATITUDE, LOCATION_LONGITUDE, LOCATION_ELEVATION };

        /** \struct TelescopeCapability
            \brief Holds the capabilities of a telescope.
        */
        typedef struct
        {
            /** Can the telescope sync to specific coordinates? */
            bool canSync;
            /** Can the telescope park? */
            bool canPark;
            /** Can the telescope abort motion? */
            bool canAbort;
            /** Does the telescope have configurable date and time settings? */
            bool hasTime;
            /** Does the telescope have configuration location settings? */
            bool hasLocation;
            /** Number of Slew Rate options. Set to 0 if telescope does not support slew rates. The minimum required # of slew rates is 4 */
            int nSlewRate;
        } TelescopeCapability;

        Telescope();
        virtual ~Telescope();

        virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
        virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
        virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
        virtual void ISGetProperties (const char *dev);
        virtual bool ISSnoopDevice(XMLEle *root);

        /**
         * @brief GetTelescopeCapability returns the capability of the Telescope
         */
        TelescopeCapability GetTelescopeCapability() const { return capability;}

        /**
         * @brief SetTelescopeCapability sets the Telescope capabilities. All capabilities must be initialized.
         * @param cap pointer to Telescope capability struct.
         */
        void SetTelescopeCapability(TelescopeCapability * cap);       

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
          \param baud Baud rate
          \return True if connection is successful, false otherwise
          \warning Do not call this function directly, it is called by INDI::Telescope Connect() function.
        */
        virtual bool Connect(const char *port, uint16_t baud);


        //Park
        /**
         * \brief setParkDataType Sets the type of parking data stored in the park data file and presented to the user.
         * \param type parking data type. If PARK_NONE then no properties will be presented to the user for custom parking position.
         */
        void SetParkDataType(TelescopeParkData type);

        /**
         * @brief InitPark Loads parking data (stored in ~/.indi/ParkData.xml) that contains parking status
         * and parking position.
         * @return True if loading is successful and data is read, false otherwise. On success, you must call
         * SetAxis1ParkDefault() and SetAxis2ParkDefault() to set the default parking values. On failure, you must call
         * SetAxis1ParkDefault() and SetAxis2ParkDefault() to set the default parking values in addition to SetAxis1Park()
         * and SetAxis2Park() to set the current parking position.
         */
        bool InitPark();

        /**
         * @brief isParked is mount currently parked?
         * @return True if parked, false otherwise.
         */
        bool isParked();

        /**
         * @brief SetParked Change the mount parking status. The data park file (stored in ~/.indi/ParkData.xml) is updated in the process.
         * @param isparked set to true if parked, false otherwise.
         */
        void SetParked(bool isparked);

        /**
         * @return Get current RA/AZ parking position.
         */
        double GetAxis1Park();

        /**
         * @return Get default RA/AZ parking position.
         */
        double GetAxis1ParkDefault();

        /**
         * @return Get current DEC/ALT parking position.
         */
        double GetAxis2Park();

        /**
         * @return Get defailt DEC/ALT parking position.
         */
        double GetAxis2ParkDefault();

        /**
         * @brief SetRAPark Set current RA/AZ parking position. The data park file (stored in ~/.indi/ParkData.xml) is updated in the process.
         * @param value current Axis 1 value (RA or AZ either in angles or encoder values as specificed by the TelescopeParkData type).
         */
        void SetAxis1Park(double value);

        /**
         * @brief SetRAPark Set default RA/AZ parking position.
         * @param value Default Axis 1 value (RA or AZ either in angles or encoder values as specificed by the TelescopeParkData type).
         */
        void SetAxis1ParkDefault(double steps);

        /**
         * @brief SetDEPark Set current DEC/ALT parking position. The data park file (stored in ~/.indi/ParkData.xml) is updated in the process.
         * @param value current Axis 1 value (DEC or ALT either in angles or encoder values as specificed by the TelescopeParkData type).
         */
        void SetAxis2Park(double steps);

        /**
         * @brief SetDEParkDefault Set default DEC/ALT parking position.
         * @param value Default Axis 2 value (DEC or ALT either in angles or encoder values as specificed by the TelescopeParkData type).
         */
        void SetAxis2ParkDefault(double steps);

        // Joystick helpers
        static void joystickHelper(const char * joystick_n, double mag, double angle, void *context);
        static void buttonHelper(const char * button_n, ISState state, void *context);

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
        virtual bool ReadScopeStatus()=0;

        /** \brief Move the scope to the supplied RA and DEC coordinates
            \return True if successful, false otherewise
            \note This function is not implemented in INDI::Telescope, it must be implemented in the child class
        */
        virtual bool Goto(double ra,double dec)=0;

        /** \brief Set the telescope current RA and DEC coordinates to the supplied RA and DEC coordinates
            \return True if successful, false otherewise
            *\note This function implemented INDI::Telescope always returns false. Override the function to return true.
        */
        virtual bool Sync(double ra,double dec);

        /** \brief Start or Stop the telescope motion in the direction dir.
         *  \param dir direction of motion
         *  \param command Start or Stop command
            \return True if successful, false otherewise
            \note This function is not implemented in INDI::Telescope, it must be implemented in the child class
        */
        virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command);

        /** \brief Move the telescope in the direction dir.
            \param dir direction of motion
            \param command Start or Stop command
            \return True if successful, false otherewise
            \note This function is not implemented in INDI::Telescope, it must be implemented in the child class
        */
        virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command);

        /** \brief Park the telescope to its home position.
            \return True if successful, false otherewise
            *\note This function defaults to return false unless subclassed by the child class.
        */
        virtual bool Park();

        /** \brief Unpark the telescope if already parked.
            \return True if successful, false otherewise
            *\note This function defaults to return false unless subclassed by the child class.
        */
        virtual bool UnPark();

        /** \brief Abort telescope motion
            \return True if successful, false otherewise
            \note This function is not implemented in INDI::Telescope, it must be implemented in the child class
        */
        virtual bool Abort()=0;

        /** \brief Update telescope time, date, and UTC offset.
         *  \param utc UTC time.
         *  \param utc_offset UTC offset in hours.
            \return True if successful, false otherewise
            \note This function performs no action unless subclassed by the child class if required.
        */
        virtual bool updateTime(ln_date *utc, double utc_offset);

        /** \brief Update telescope location settings
         *  \param latitude Site latitude in degrees.
         *  \param longitude Site latitude in degrees increasing eastward from Greenwich (0 to 360).
         *  \param elevation Site elevation in meters.
            \return True if successful, false otherewise
            \note This function performs no action unless subclassed by the child class if required.
        */
        virtual bool updateLocation(double latitude, double longitude, double elevation);        

        /**
         * @brief SetCurrentPark Set current coordinates/encoders value as the desired parking position
         * \note This function performs no action unless subclassed by the child class if required.
         */
        virtual void SetCurrentPark();

        /**
         * @brief SetDefaultPark Set default coordinates/encoders value as the desired parking position
         * \note This function performs no action unless subclassed by the child class if required.
         */
        virtual void SetDefaultPark();

        /**
         * @brief SetSlewRate Set desired slew rate index.
         * @param index Index of slew rate where 0 is slowest rate and capability.nSlewRate-1 is maximum rate.
         * @return True is operation successful, false otherwise.
         *
         * \note This function as implemented in INDI::Telescope performs no function and always return true. Only reimplement it if you need to issue a command to change the slew rate at the hardware level. Most
         * telescope drivers only utilize slew rate when issuing a motion command.
         */
        virtual bool SetSlewRate(int index);

        // Joystick
        void processNSWE(double mag, double angle);
        void processJoystick(const char * joystick_n, double mag, double angle);
        void processSlewPresets(double mag, double angle);
        void processButton(const char * button_n, ISState state);

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
        INumberVectorProperty EqNP;
        INumber EqN[2];

        // Abort motion
        ISwitchVectorProperty AbortSP;
        ISwitch AbortS[1];

        //  On a coord_set message, sync, or slew
        ISwitchVectorProperty CoordSP;
        ISwitch CoordS[3];

        //  A number vector that stores lattitude and longitude
        INumberVectorProperty LocationNP;
        INumber LocationN[3];

        // A Switch in the client interface to park the scope
        ISwitchVectorProperty ParkSP;
        ISwitch ParkS[2];

        // Custom parking position
        INumber ParkPositionN[2];
        INumberVectorProperty ParkPositionNP;

        // Custom parking options
        ISwitch ParkOptionS[3];
        ISwitchVectorProperty ParkOptionSP;

        // Device physical port
        ITextVectorProperty PortTP;
        IText PortT[1];

        // A switch for North/South motion
        ISwitch MovementNSS[2];
        ISwitchVectorProperty MovementNSSP;

        // A switch for West/East motion
        ISwitch MovementWES[2];
        ISwitchVectorProperty MovementWESP;

        // Slew Rate
        ISwitchVectorProperty SlewRateSP;
        ISwitch *SlewRateS;

        // Telescope & guider aperture and focal length
        INumber ScopeParametersN[4];
        INumberVectorProperty ScopeParametersNP;

        // UTC and UTC Offset
        IText TimeT[2];
        ITextVectorProperty TimeTP;

        // Active devices to snoop
        ITextVectorProperty ActiveDeviceTP;
        IText ActiveDeviceT[1];

        ISwitch BaudRateS[6];
        ISwitchVectorProperty BaudRateSP;

        TelescopeCapability capability;        
        int last_we_motion, last_ns_motion;

        //Park
        char *LoadParkData();
        bool WriteParkData();        

private:

        bool processTimeInfo(const char *utc, const char *offset);
        bool processLocationInfo(double latitude, double longitude, double elevation);

        TelescopeParkData parkDataType;
        bool IsParked;        
        const char *ParkDeviceName;
        const char * Parkdatafile;
        XMLEle *ParkdataXmlRoot, *ParkdeviceXml, *ParkstatusXml, *ParkpositionXml, *ParkpositionAxis1Xml, *ParkpositionAxis2Xml;

        double Axis1ParkPosition;
        double Axis1DefaultParkPosition;
        double Axis2ParkPosition;
        double Axis2DefaultParkPosition;

        IPState lastEqState;

        INDI::Controller *controller;

};

#endif // INDI::Telescope_H
