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

#pragma once

#include "defaultdevice.h"
#include "libastro.h"
#include "indipropertyswitch.h"
#include <libnova/julian_day.h>

#include <string>

/**
 * \class Telescope
 * \brief Class to provide general functionality of a telescope device.
 *
 * Developers need to subclass Telescope to implement any driver for telescopes within INDI.
 *
 * Implementing a basic telescope driver involves the child class performing the following steps:
 * <ul>
 * <li>The child class should define the telescope capabilities via the TelescopeCapability structure
 * and sets in the default constructor.</li>
 * <li>If the telescope has additional properties, the child class should override initProperties and
 * initialize the respective additional properties.</li>
 * <li>The child class can optionally set the connection mode in initProperties(). By default the driver
 * provide controls for both serial and TCP/IP connections.</li>
 * <li>Once the parent class calls Connect(), the child class attempts to connect to the telescope and
 * return either success of failure</li>
 * <li>Telescope calls updateProperties() to enable the child class to define which properties to
 * send to the client upon connection</li>
 * <li>Telescope calls ReadScopeStatus() to check the link to the telescope and update its state
 * and position. The child class should call newRaDec() whenever
 * a new value is read from the telescope.</li>
 * <li>The child class should implement Goto() and Sync(), and Park()/UnPark() if applicable.</li>
 * <li>Telescope calls disconnect() when the client request a disconnection. The child class
 * should remove any additional properties it defined in updateProperties() if applicable</li>
 * </ul>
 *
 * TrackState is used to monitor changes in Tracking state. There are three main tracking properties:
 * + TrackMode: Changes tracking mode or rate. Common modes are TRACK_SIDEREAL, TRACK_LUNAR, TRACK_SOLAR, and TRACK_CUSTOM
 * + TrackRate: If the mount supports custom tracking rates, it should set the capability flag TELESCOPE_HAS_TRACK_RATE. If the user
 *              changes the custom tracking rates while the mount is tracking, it it sent to the child class via SetTrackRate(...) function.
 *              The base class will reject any track rates that switch from positive to negative (reverse) tracking rates as the mount must be stopped before
 *              such change takes place.
 * + TrackState: Engages or Disengages tracking. When engaging tracking, the child class should take the necessary steps to set the appropriate TrackMode and TrackRate
 *               properties before or after engaging tracking as governed by the mount protocol.
 *
 * Home position is the default starting position of the mount where both axis indexes are in the zero or home position. This can be different from the parking
 * position which can be set to avoid obstacles (e.g. roof) while in the parked state. For many mounts, the parking and home positions are identical. If homing is supported,
 * the following operations may be supported:
 * 1. Find: Search for the home indexes.
 * 2. Set: Use current position as the home position
 * 3. Go: Go to the stored home position.
 *
 * Ideally, the child class should avoid changing property states directly within a function call from the base class as such state changes take place in the base class
 * after checking the return values of such functions.
 * \author Jasem Mutlaq, Gerry Rozema
 * \see TelescopeSimulator and SynScan drivers for examples of implementations of Telescope.
 */
namespace INDI
{

class Telescope : public DefaultDevice
{
    public:
        enum TelescopeStatus
        {
            SCOPE_IDLE,
            SCOPE_SLEWING,
            SCOPE_TRACKING,
            SCOPE_PARKING,
            SCOPE_PARKED
        };
        enum TelescopeMotionCommand
        {
            MOTION_START = 0,
            MOTION_STOP
        };
        enum TelescopeSlewRate
        {
            SLEW_GUIDE,
            SLEW_CENTERING,
            SLEW_FIND,
            SLEW_MAX
        };
        enum TelescopeTrackMode
        {
            TRACK_SIDEREAL,
            TRACK_SOLAR,
            TRACK_LUNAR,
            TRACK_CUSTOM
        };
        enum TelescopeTrackState
        {
            TRACK_ON,
            TRACK_OFF,
            TRACK_UNKNOWN
        };
        enum TelescopeParkData
        {
            PARK_NONE,              /*!< Mount does not support any form of parking */
            PARK_RA_DEC,            /*!< Park to specific RA/DE coordinate. Deprecated, do not use */
            PARK_HA_DEC,            /*!< Park to specific HA/DE coordinate. Hour Angle & Declination degrees */
            PARK_AZ_ALT,            /*!< Park to specific AZ/ALT coordinate. Azimuth & Altitude degrees */
            PARK_RA_DEC_ENCODER,    /*!< Park to specific HA/DE encoder. Hour Angle & Declination steps */
            PARK_AZ_ALT_ENCODER,    /*!< Park to specific AZ/ALT encoder. Azimuth & Altitude steps */
            PARK_SIMPLE             /*!< Only Park or Unpark is known but location is unknown. */
        };
        enum TelescopeLocation
        {
            LOCATION_LATITUDE,
            LOCATION_LONGITUDE,
            LOCATION_ELEVATION
        };
        enum TelescopePierSide
        {
            PIER_UNKNOWN = -1,
            PIER_WEST    = 0,
            PIER_EAST    = 1
        };

        enum TelescopeHomeAction
        {
            HOME_FIND,              /*!< Command mount to search for home position.  */
            HOME_SET,               /*!< Command mount to accept current position as the home position. */
            HOME_GO,                /*!< Command mount to slew to home position. */
        };

        enum TelescopePECState
        {
            PEC_UNKNOWN = -1,
            PEC_OFF     = 0,
            PEC_ON      = 1
        };

        /*! Dome Locking Policy */
        enum DomeLockingPolicy
        {
            DOME_IGNORED,       /*!< Dome is ignored. Mount can park or unpark irrespective of dome parking status. */
            DOME_LOCKS,         /*!< Dome locks. Mount can only unpark if dome is already unparked. */
        };

        /**
         * \struct TelescopeConnection
         * \brief Holds the connection mode of the telescope.
         */
        enum
        {
            CONNECTION_NONE   = 1 << 0, /** Do not use any connection plugin */
            CONNECTION_SERIAL = 1 << 1, /** For regular serial and bluetooth connections */
            CONNECTION_TCP    = 1 << 2  /** For Wired and WiFI connections */
        } TelescopeConnection;

        /**
         * \struct TelescopeCapability
         * \brief Holds the capabilities of a telescope.
         */
        enum
        {
            TELESCOPE_CAN_GOTO                    = 1 << 0,  /** Can the telescope go to to specific coordinates? */
            TELESCOPE_CAN_SYNC                    = 1 << 1,  /** Can the telescope sync to specific coordinates? */
            TELESCOPE_CAN_PARK                    = 1 << 2,  /** Can the telescope park? */
            TELESCOPE_CAN_ABORT                   = 1 << 3,  /** Can the telescope abort motion? */
            TELESCOPE_HAS_TIME                    = 1 << 4,  /** Does the telescope have configurable date and time settings? */
            TELESCOPE_HAS_LOCATION                = 1 << 5,  /** Does the telescope have configuration location settings? */
            TELESCOPE_HAS_PIER_SIDE               = 1 << 6,  /** Does the telescope have pier side property? */
            TELESCOPE_HAS_PEC                     = 1 << 7,  /** Does the telescope have PEC playback? */
            TELESCOPE_HAS_TRACK_MODE              = 1 << 8,  /** Does the telescope have track modes (sidereal, lunar, solar..etc)? */
            TELESCOPE_CAN_CONTROL_TRACK           = 1 << 9,  /** Can the telescope engage and disengage tracking? */
            TELESCOPE_HAS_TRACK_RATE              = 1 << 10, /** Does the telescope have custom track rates? */
            TELESCOPE_HAS_PIER_SIDE_SIMULATION    = 1 << 11, /** Does the telescope simulate the pier side property? */
            TELESCOPE_CAN_TRACK_SATELLITE         = 1 << 12, /** Can the telescope track satellites? */
            TELESCOPE_CAN_FLIP                    = 1 << 13, /** Does the telescope have a command for flipping? */
            TELESCOPE_CAN_HOME_FIND               = 1 << 14, /** Can the telescope find home position? */
            TELESCOPE_CAN_HOME_SET                = 1 << 15, /** Can the telescope set the current position as the new home position? */
            TELESCOPE_CAN_HOME_GO                 = 1 << 16, /** Can the telescope slew to home position? */
        } TelescopeCapability;

        Telescope();
        virtual ~Telescope();

        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual void ISGetProperties(const char *dev) override;
        virtual bool ISSnoopDevice(XMLEle *root) override;

        /**
         * @brief GetTelescopeCapability returns the capability of the Telescope
         */
        uint32_t GetTelescopeCapability() const
        {
            return capability;
        }

        /**
         * @brief SetTelescopeCapability sets the Telescope capabilities. All capabilities must be initialized.
         * @param cap ORed list of telescope capabilities.
         * @param slewRateCount Number of slew rates supported by the telescope. If < 4,
         * no slew rate properties will be defined to the client. If >=4, the driver will construct the default
         * slew rate property TELESCOPE_SLEW_RATE with SLEW_GUIDE, SLEW_CENTERING, SLEW_FIND, and SLEW_MAX
         * members where SLEW_GUIDE is the at the lowest setting and SLEW_MAX is at the highest.
         */
        void SetTelescopeCapability(uint32_t cap, uint8_t slewRateCount);

        /**
         * @return True if telescope support goto operations
         */
        bool CanGOTO()
        {
            return capability & TELESCOPE_CAN_GOTO;
        }

        /**
         * @return True if telescope support sync operations
         */
        bool CanSync()
        {
            return capability & TELESCOPE_CAN_SYNC;
        }

        /**
         * @return True if telescope support sync operations
         */
        bool CanFlip()
        {
            return capability & TELESCOPE_CAN_FLIP;
        }

        /**
         * @return True if telescope can abort motion.
         */
        bool CanAbort()
        {
            return capability & TELESCOPE_CAN_ABORT;
        }

        /**
         * @return True if telescope can park.
         */
        bool CanPark()
        {
            return capability & TELESCOPE_CAN_PARK;
        }

        /**
         * @return True if telescope can enagle and disengage tracking.
         */
        bool CanControlTrack()
        {
            return capability & TELESCOPE_CAN_CONTROL_TRACK;
        }

        /**
         * @return True if telescope time can be updated.
         */
        bool HasTime()
        {
            return capability & TELESCOPE_HAS_TIME;
        }

        /**
         * @return True if telescope location can be updated.
         */
        bool HasLocation()
        {
            return capability & TELESCOPE_HAS_LOCATION;
        }

        /**
         * @return True if telescope supports pier side property
         */
        bool HasPierSide()
        {
            return capability & TELESCOPE_HAS_PIER_SIDE;
        }

        /**
         * @return True if telescope simulates pier side property
         */
        bool HasPierSideSimulation()
        {
            return capability & TELESCOPE_HAS_PIER_SIDE_SIMULATION;
        }
        /**
         * @return True if telescope can track satellites
         */
        bool CanTrackSatellite()
        {
            return capability & TELESCOPE_CAN_TRACK_SATELLITE;
        }
        /**
         * @return True if telescope supports PEC playback property
         */
        bool HasPECState()
        {
            return capability & TELESCOPE_HAS_PEC;
        }

        /**
         * @return True if telescope supports track modes
         */
        bool HasTrackMode()
        {
            return capability & TELESCOPE_HAS_TRACK_MODE;
        }

        /**
         * @return True if telescope supports custom tracking rates.
         */
        bool HasTrackRate()
        {
            return capability & TELESCOPE_HAS_TRACK_RATE;
        }

        /**
         * @return True if telescope can search for home position.
         */
        bool CanHomeFind()
        {
            return capability & TELESCOPE_CAN_HOME_FIND;
        }

        /**
         * @return True if telescope can set current position as the home position.
         */
        bool CanHomeSet()
        {
            return capability & TELESCOPE_CAN_HOME_SET;
        }

        /**
         * @return True if telescope can go to the home position.
         */
        bool CanHomeGo()
        {
            return capability & TELESCOPE_CAN_HOME_GO;
        }

        /** \brief Called to initialize basic properties required all the time */
        virtual bool initProperties() override;
        /** \brief Called when connected state changes, to add/remove properties */
        virtual bool updateProperties() override;

        /** \brief perform handshake with device to check communication */
        virtual bool Handshake();

        /** \brief Called when setTimer() time is up */
        virtual void TimerHit() override;

        /**
         * \brief setParkDataType Sets the type of parking data stored in the park data file and
         * presented to the user.
         * \param type parking data type. If PARK_NONE then no properties will be presented to the
         * user for custom parking position.
         */
        void SetParkDataType(TelescopeParkData type);

        /**
         * @brief InitPark Loads parking data (stored in ~/.indi/ParkData.xml) that contains parking status
         * and parking position.
         * @return True if loading is successful and data is read, false otherwise. On success, you must call
         * SetAxis1ParkDefault() and SetAxis2ParkDefault() to set the default parking values. On failure,
         * you must call SetAxis1ParkDefault() and SetAxis2ParkDefault() to set the default parking values
         * in addition to SetAxis1Park() and SetAxis2Park() to set the current parking position.
         */
        bool InitPark();

        /**
         * @brief isParked is mount currently parked?
         * @return True if parked, false otherwise.
         */
        bool isParked();

        /**
         * @brief SetParked Change the mount parking status. The data park file (stored in
         * ~/.indi/ParkData.xml) is updated in the process.
         * @param isparked set to true if parked, false otherwise.
         */
        virtual void SetParked(bool isparked);

        /**
         * @return Get current RA/AZ parking position.
         */
        double GetAxis1Park() const;

        /**
         * @return Get default RA/AZ parking position.
         */
        double GetAxis1ParkDefault() const;

        /**
         * @return Get current DEC/ALT parking position.
         */
        double GetAxis2Park() const;

        /**
         * @return Get default DEC/ALT parking position.
         */
        double GetAxis2ParkDefault() const;

        /**
         * @brief SetRAPark Set current RA/AZ parking position. The data park file (stored in
         * ~/.indi/ParkData.xml) is updated in the process.
         * @param value current Axis 1 value (RA or AZ either in angles or encoder values as specified
         * by the TelescopeParkData type).
         */
        void SetAxis1Park(double value);

        /**
         * @brief SetRAPark Set default RA/AZ parking position.
         * @param value Default Axis 1 value (RA or AZ either in angles or encoder values as specified
         * by the TelescopeParkData type).
         */
        void SetAxis1ParkDefault(double steps);

        /**
         * @brief SetDEPark Set current DEC/ALT parking position. The data park file (stored in
         * ~/.indi/ParkData.xml) is updated in the process.
         * @param value current Axis 1 value (DEC or ALT either in angles or encoder values as specified
         * by the TelescopeParkData type).
         */
        void SetAxis2Park(double steps);

        /**
         * @brief SetDEParkDefault Set default DEC/ALT parking position.
         * @param value Default Axis 2 value (DEC or ALT either in angles or encoder values as specified
         * by the TelescopeParkData type).
         */
        void SetAxis2ParkDefault(double steps);

        /**
         * @brief isLocked is mount currently locked?
         * @return true if lock status equals true and Dome Policy is Dome Locks.
         */
        bool isLocked() const;

        // Joystick helpers
        static void joystickHelper(const char *joystick_n, double mag, double angle, void *context);
        static void axisHelper(const char *axis_n, double value, void *context);
        static void buttonHelper(const char *button_n, ISState state, void *context);

        /**
         * @brief setTelescopeConnection Set telescope connection mode. Child class should call this
         * in the constructor before Telescope registers any connection interfaces
         * @param value ORed combination of TelescopeConnection values.
         */
        void setTelescopeConnection(const uint8_t &value);

        /**
         * @return Get current telescope connection mode
         */
        uint8_t getTelescopeConnection() const;

        void setPierSide(TelescopePierSide side);
        TelescopePierSide getPierSide()
        {
            return currentPierSide;
        }

        void setPECState(TelescopePECState state);
        TelescopePECState getPECState()
        {
            return currentPECState;
        }

    protected:
        virtual bool saveConfigItems(FILE *fp) override;

        /** \brief The child class calls this function when it has updates */
        void NewRaDec(double ra, double dec);

        /**
         * \brief Read telescope status.
         *
         * This function checks the following:
         * <ol>
         *   <li>Check if the link to the telescope is alive.</li>
         *   <li>Update telescope status: Idle, Slewing, Parking..etc.</li>
         *   <li>Read coordinates</li>
         * </ol>
         * \return True if reading scope status is OK, false if an error is encountered.
         * \note This function is not implemented in Telescope, it must be implemented in the
         * child class
         */
        virtual bool ReadScopeStatus() = 0;

        /**
        * \brief Move and flip the scope to the supplied RA and DEC coordinates
        * \return True if successful, false otherwise
        * \note If not implemented by the child class, this function by default returns false with a
        * warning message.
        */
        virtual bool Flip(double ra, double dec);

        /**
        * \brief Move the scope to the supplied RA and DEC coordinates
        * \return True if successful, false otherwise
        * \note If not implemented by the child class, this function by default returns false with a
        * warning message.
        */
        virtual bool Goto(double ra, double dec);

        /**
         * \brief Set the telescope current RA and DEC coordinates to the supplied RA and DEC coordinates
         * \return True if successful, false otherwise
         * \note If not implemented by the child class, this function by default returns false with a
         * warning message.
         */
        virtual bool Sync(double ra, double dec);

        /**
         * \brief Start or Stop the telescope motion in the direction dir.
         * \param dir direction of motion
         * \param command Start or Stop command
         * \return True if successful, false otherwise
         * \note If not implemented by the child class, this function by default returns false with a
         * warning message.
         */
        virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command);

        /**
         * \brief Move the telescope in the direction dir.
         * \param dir direction of motion
         * \param command Start or Stop command
         * \return True if successful, false otherwise
         * \note If not implemented by the child class, this function by default returns false with a
         * warning message.
         */
        virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command);

        /**
         * \brief Park the telescope to its home position.
         * \return True if successful, false otherwise
         * \note If not implemented by the child class, this function by default returns false with a
         * warning message.
         */
        virtual bool Park();

        /**
         * \brief Unpark the telescope if already parked.
         * \return True if successful, false otherwise
         * \note If not implemented by the child class, this function by default returns false with a
         * warning message.
         */
        virtual bool UnPark();

        /**
         * \brief Abort any telescope motion including tracking if possible.
         * \return True if successful, false otherwise
         * \note If not implemented by the child class, this function by default returns false with a
         * warning message.
         */
        virtual bool Abort();

        /**
         * @brief SetTrackMode Set active tracking mode. Do not change track state.
         * @param mode Index of track mode.
         * @return True if successful, false otherwise
         * @note If not implemented by the child class, this function by default returns false with a
         * warning message.
         */
        virtual bool SetTrackMode(uint8_t mode);

        /**
         * @brief SetTrackRate Set custom tracking rates.
         * @param raRate RA tracking rate in arcsecs/s
         * @param deRate DEC tracking rate in arcsecs/s
         * @return True if successful, false otherwise
         * @note If not implemented by the child class, this function by default returns false with a
         * warning message.
         */
        virtual bool SetTrackRate(double raRate, double deRate);

        /**
         * @brief AddTrackMode
         * @param name Name of track mode. It is recommended to use standard properties names such as TRACK_SIDEREAL..etc.
         * @param label Label of track mode that appears at the client side.
         * @param isDefault Set to true to mark the track mode as the default. Only one mode should be marked as default.
         * @return Index of added track mode
         * @note Child class should add all track modes be
         */
        virtual int AddTrackMode(const char *name, const char *label, bool isDefault = false);

        /**
         * @brief SetTrackEnabled Engages or disengages mount tracking. If there are no tracking modes available, it is assumed sidereal. Otherwise,
         * whatever tracking mode should be activated or deactivated accordingly.
         * @param enabled True to engage tracking, false to stop tracking completely.
         * @return True if successful, false otherwise
         * @note If not implemented by the child class, this function by default returns false with a
         * warning message.
         */
        virtual bool SetTrackEnabled(bool enabled);

        /**
         * \brief Update telescope time, date, and UTC offset.
         * \param utc UTC time.
         * \param utc_offset UTC offset in hours.
         * \return True if successful, false otherwise
         * \note If not implemented by the child class, this function by default returns false with a
         * warning message.
         */
        virtual bool updateTime(ln_date *utc, double utc_offset);

        /**
         * \brief Update telescope location settings
         * \param latitude Site latitude in degrees.
         * \param longitude Site latitude in degrees increasing eastward from Greenwich (0 to 360).
         * \param elevation Site elevation in meters.
         * \return True if successful, false otherwise
         * \note If not implemented by the child class, this function by default returns false with a
         * warning message.
         */
        virtual bool updateLocation(double latitude, double longitude, double elevation);

        /**
         * \brief Update location settings of the observer
         * \param latitude Site latitude in degrees.
         * \param longitude Site latitude in degrees increasing eastward from Greenwich (0 to 360).
         * \param elevation Site elevation in meters.
         * \return True if successful, false otherwise
         * \note If not implemented by the child class, this function by default returns false with a
         * warning message.
         */
        void updateObserverLocation(double latitude, double longitude, double elevation);

        /**
         * \brief SetParkPosition Set desired parking position to the supplied value. This ONLY sets the
         * desired park position value and does not perform parking.
         * \param Axis1Value First axis value
         * \param Axis2Value Second axis value
         * \return True if desired parking position is accepted and set. False otherwise.
         * \note If not implemented by the child class, this function by default returns false with a
         * warning message.
         */
        virtual bool SetParkPosition(double Axis1Value, double Axis2Value);

        /**
         * \brief Execute a homing action
         * \param action Depending on the telescope supported homing capabilities, this could be either
         * FIND, SET, or GO.
         * \return State of executed action. IPS_OK if action is completed successfully, IPS_BUSY if in progress, IPS_ALERT if failed.
         */
        virtual IPState ExecuteHomeAction(TelescopeHomeAction action);

        /**
         * @brief SetCurrentPark Set current coordinates/encoders value as the desired parking position
         * @return True if current mount coordinates are set as parking position, false on error.
         * \note If not implemented by the child class, this function by default returns false with a
         * warning message.
         */
        virtual bool SetCurrentPark();

        /**
         * @brief SetDefaultPark Set default coordinates/encoders value as the desired parking position
         * @return True if default park coordinates are set as parking position, false on error.
         * \note If not implemented by the child class, this function by default returns false with a
         * warning message.
         */
        virtual bool SetDefaultPark();


        /**
         * @brief SyncParkStatus Update the state and switches for parking
         * @param isparked True if parked, false otherwise.
         */
        virtual void SyncParkStatus(bool isparked);

        /**
         * @brief SetSlewRate Set desired slew rate index.
         * @param index Index of slew rate where 0 is slowest rate and capability.nSlewRate-1 is maximum rate.
         * @return True is operation successful, false otherwise.
         *
         * \note This function as implemented in Telescope performs no function and always return
         * true. Only reimplement it if you need to issue a command to change the slew rate at the hardware
         * level. Most telescope drivers only utilize slew rate when issuing a motion command.
         */
        virtual bool SetSlewRate(int index);

        /**
         * @brief callHandshake Helper function that sets the port file descriptor before calling the
         * actual Handshake function implenented in drivers
         * @return Result of actual device Handshake()
         */
        bool callHandshake();

        // Joystick
        void processNSWE(double mag, double angle);
        void processJoystick(const char *joystick_n, double mag, double angle);
        void processAxis(const char *axis_n, double value);
        void processSlewPresets(double mag, double angle);
        void processButton(const char *button_n, ISState state);

        /**
         * @brief Calculate the expected pier side for scopes that do not report
         * this property themselves.
         */
        TelescopePierSide expectedPierSide(double ra);

        // Geographic Location
        IGeographicCoordinates m_Location { 0, 0, 0 };

        /**
         * @brief Validate a file name
         * @param file_name File name
         * @return True if the file name is valid otherwise false.
         */
        std::string GetHomeDirectory() const;

        /**
         * @brief Get the scope config index
         * @return The scope config index
         */
        int GetScopeConfigIndex() const;

        /**
         * @brief Check if a file exists and it is readable
         * @param file_name File name
         * @param writable Additional check if the file is writable
         * @return True if the checks are successful otherwise false.
         */
        bool CheckFile(const std::string &file_name, bool writable) const;

        /**
         * This is a variable filled in by the ReadStatus telescope
         * low level code, used to report current state
         * are we slewing, tracking, or parked.
         */
        TelescopeStatus TrackState {SCOPE_IDLE};

        /**
         * @brief RememberTrackState Remember last state of Track State to fall back to in case of errors or aborts.
         */
        TelescopeStatus RememberTrackState {SCOPE_IDLE};

        /**
        * \defgroup INDI Mount Standard Properties
        * @{
        */

        // All telescopes should produce equatorial co-ordinates
        INDI::PropertyNumber EqNP {2};

        // When a goto is issued, domes will snoop the target property
        // to start moving the dome when a telescope moves
        INDI::PropertyNumber TargetNP {2};

        // Abort motion
        INDI::PropertySwitch AbortSP {1};

        // On a coord_set message, sync, or slew
        INDI::PropertySwitch CoordSP {4};

        // A number vector that stores latitude and longitude
        INDI::PropertyNumber LocationNP {3};

        // A Switch in the client interface to park the scope
        INDI::PropertySwitch ParkSP {2};
        enum
        {
            PARK,
            UNPARK
        };

        // Custom parking position
        INDI::PropertyNumber ParkPositionNP {2};

        /**
        + NAME: TELESCOPE_PARK_POSITION
        + DESCRIPTION: Mount parking position option. Set mount parking position from current or default positions and write or purge data from the standard ParkData.xml file.
          ParkData.xml contains all the parking positions for all INDI devices and is read on startup. It indicates the parking position and status (Parked/Unparked)
        + TYPE: SWITCH
        + MEMBMERS:
            + PARK_CURRENT: Use current mount position as the parking position
            + PARK_DEFAULT: Use driver own default position (defined by driver) as the parking position
            + PARK_WRITE_DATA: Write current parking information to ParkData.xml file.
            + PARK_PURGE_DATA: Remove Park data from ParkData.xml file.
        **/
        INDI::PropertySwitch ParkOptionSP {4};
        enum
        {
            PARK_CURRENT,
            PARK_DEFAULT,
            PARK_WRITE_DATA,
            PARK_PURGE_DATA,
        };

        // A switch for North/South motion
        INDI::PropertySwitch MovementNSSP {2};

        // A switch for West/East motion
        INDI::PropertySwitch MovementWESP {2};

        // Reverse NS or WE
        INDI::PropertySwitch ReverseMovementSP {2};
        enum
        {
            REVERSE_NS,
            REVERSE_WE
        };

        // Slew Rate
        INDI::PropertySwitch SlewRateSP {0};

        // UTC and UTC Offset
        INDI::PropertyText TimeTP {2};
        enum
        {
            UTC,
            OFFSET
        };
        void sendTimeFromSystem();

        // Active GPS/Dome device to snoop
        INDI::PropertyText ActiveDeviceTP {2};
        enum
        {
            ACTIVE_GPS,
            ACTIVE_DOME
        };

        // Switch to lock if dome is closed.
        INDI::PropertySwitch DomePolicySP {2};

        // Switch for choosing between motion control by 4-way joystick or two separate axes
        INDI::PropertySwitch MotionControlModeTP {2};
        enum
        {
            MOTION_CONTROL_MODE_JOYSTICK,
            MOTION_CONTROL_MODE_AXES,
        };

        // Lock Joystick Axis to one direction only
        INDI::PropertySwitch LockAxisSP {2};
        enum
        {
            LOCK_AXIS_1,
            LOCK_AXIS_2
        };
        // Pier Side
        INDI::PropertySwitch PierSideSP {2};

        // Pier Side Simulation
        INDI::PropertySwitch SimulatePierSideSP {2};
        enum
        {
            SIMULATE_YES,
            SIMULATE_NO
        };
        bool getSimulatePierSide() const;
        void setSimulatePierSide(bool value);

        // Pier Side
        TelescopePierSide lastPierSide, currentPierSide;

        const char * getPierSideStr(TelescopePierSide ps);

        // Satellite tracking
        /**
         * \brief Text Vector property defining the orbital elements of an artificial satellite (TLE).
         * \ref drivers/telescope/lx200_10micron.cpp "Example implementation"
         */
        INDI::PropertyText TLEtoTrackTP{1};
        /**
         * \brief Text Vector property defining the start and end of a satellite pass (window contains pass).
         * \ref drivers/telescope/lx200_10micron.cpp "Example implementation"
         */
        INDI::PropertyText SatPassWindowTP {2};

        /**
         * \struct SatelliteWindow
         * \brief Satellite pass: window start and end.
         */
        enum
        {
            SAT_PASS_WINDOW_START, ///< Index for start of the window
            SAT_PASS_WINDOW_END, ///< Index for end of the window
        } SatelliteWindow;

        /**
         * \brief Switch Vector property defining the state of the satellite tracking of the mount.
         * \ref drivers/telescope/lx200_10micron.cpp "Example implementation"
         */
        INDI::PropertySwitch TrackSatSP {2};
        /**
         * \struct SatelliteTracking
         * \brief Possible states for the satellite tracking.
         */
        enum
        {
            SAT_TRACK, ///< Track signal
            SAT_HALT, ///< Halt signal (abort)
        } SatelliteTracking;

        // PEC State
        INDI::PropertySwitch PECStateSP {2};

        // Mount Type
        INDI::PropertySwitch MountTypeSP {3};
        enum
        {
            MOUNT_ALTAZ,
            MOUNT_EQ_FORK,
            MOUNT_EQ_GEM
        };

        // Track Mode
        INDI::PropertySwitch TrackModeSP {0};

        // Track State
        INDI::PropertySwitch TrackStateSP {2};
        // ISwitch TrackStateS[2];

        // Track Rate
        INDI::PropertyNumber TrackRateNP {2};

        // Home Position
        INDI::PropertySwitch HomeSP {0};

        /**@}*/

        // PEC State
        TelescopePECState lastPECState {PEC_UNKNOWN}, currentPECState {PEC_UNKNOWN};

        uint32_t capability {0};
        int last_we_motion {-1}, last_ns_motion {-1};

        //Park
        const char *LoadParkData();
        bool WriteParkData();
        bool PurgeParkData();

        int PortFD                           = -1;
        Connection::Serial *serialConnection = nullptr;
        Connection::TCP *tcpConnection       = nullptr;

        bool IsParked {false};
        TelescopeParkData parkDataType {PARK_NONE};

    private:
        bool processTimeInfo(const char *utc, const char *offset);
        bool processLocationInfo(double latitude, double longitude, double elevation);
        void triggerSnoop(const char *driverName, const char *propertyName);
        void generateCoordSet();

        /**
         * @brief LoadParkXML Read and process park XML data.
         * @return error string if there is problem opening the file
         */
        const char *LoadParkXML();

        bool IsLocked {true};
        const char *ParkDeviceName {nullptr};
        const std::string ParkDataFileName;
        XMLEle *ParkdataXmlRoot {nullptr}, *ParkdeviceXml {nullptr}, *ParkstatusXml {nullptr}, *ParkpositionXml {nullptr},
               *ParkpositionAxis1Xml {nullptr}, *ParkpositionAxis2Xml {nullptr};

        double Axis1ParkPosition {0};
        double Axis1DefaultParkPosition {0};
        double Axis2ParkPosition {0};
        double Axis2DefaultParkPosition {0};

        uint8_t nSlewRate {0};
        IPState lastEqState { IPS_IDLE };

        uint8_t telescopeConnection = (CONNECTION_SERIAL | CONNECTION_TCP);

        Controller *controller {nullptr};

        float motionDirNSValue {0};
        float motionDirWEValue {0};

        bool m_simulatePierSide;    // use setSimulatePierSide and getSimulatePierSide for public access

        // 100 millisecond of arc or time.
        static constexpr double EQ_NOTIFY_THRESHOLD {1.0 / (60 * 60 * 10)};
};

}
