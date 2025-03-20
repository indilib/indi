/*******************************************************************************
 INDI Dome Base Class
 Copyright(c) 2014 Jasem Mutlaq. All rights reserved.

 The code used calculate dome target AZ and ZD is written by Ferran Casarramona, and adapted from code from Markus Wildi.
 The transformations are based on the paper Matrix Method for Coordinates Transformation written by Toshimi Taki (http://www.asahi-net.or.jp/~zs3t-tk).

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
#include "inditimer.h"

#include <string>

// Defines a point in a 3 dimension space
typedef struct
{
    double x, y, z;
} point3D;

namespace Connection
{
class Serial;
class TCP;
}

/**
 * \class INDI::Dome
   \brief Class to provide general functionality of a Dome device.

   Both relative and absolute position domes are supported. Furthermore, if no position feedback is available from the dome, an open-loop control is possible with simple direction commands (Clockwise and counter clockwise).

   Before using any of the dome functions, you must define the capabilities of the dome by calling SetDomeCapability() function. All positions are represented as degrees of azimuth.

   Relative motion is specified in degrees as either positive (clock wise direction), or negative (counter clock-wise direction).

   Slaving is used to synchronizes the dome's azimuth position with that of the mount. The mount's coordinates are snooped from the active mount that has its name specified in ACTIVE_TELESCOPE property in the ACTIVE_DEVICES vector.
   Dome motion begins when it receives TARGET_EOD_COORD property from the mount driver when the mount starts slewing to the desired target coordinates /em OR
   when the mount's current tracking position exceeds the AutoSync threshold. Therefore, slaving is performed while slewing and tracking. The user is required to fill in all required parameters before slaving can be used.
   The AutoSync threshold is the difference in degrees between the dome's azimuth angle and the mount's azimuth angle that should trigger a dome motion.
   By default, it is set to 0.5 degrees which would trigger dome motion due to any difference between the dome and mount azimuth angles that exceeds 0.5 degrees.
   For example, if the threshold is set to 5 degrees, the dome will only start moving to sync with the mount's azimuth angle once the difference in azimuth angles is equal or exceeds 5 degrees.

   Custom parking position is available for absolute/relative position domes.

   For roll-off observatories, parking state reflects whether the roof is closed or open.

   Developers need to subclass INDI::Dome to implement any driver for Domes within INDI.

  \note The code used calculate dome target AZ and ZD is written by Ferran Casarramona, and adapted from code from Markus Wildi. The transformations are based on the paper Matrix Method for Coordinates
 Transformation written by Toshimi Taki (http://www.asahi-net.or.jp/~zs3t-tk).

\author Jasem Mutlaq
*/
namespace INDI
{

class Dome : public DefaultDevice
{
    public:
        /** \typedef DomeMeasurements
                \brief Measurements necessary for dome-slit synchronization. All values are in meters. The displacements are measured from the true dome centre, and the dome is assumed spherical.
                \note: The mount centre is the point where RA and Dec. axis crosses, no matter the kind of mount. For example, for a fork mount this displacement is typically 0 if it's perfectly centred with RA axis.
            */
        typedef enum
        {
            DM_DOME_RADIUS,        /*!< Dome RADIUS */
            DM_SHUTTER_WIDTH,      /*!< Shutter width */
            DM_NORTH_DISPLACEMENT, /*!< Displacement to north of the mount center */
            DM_EAST_DISPLACEMENT,  /*!< Displacement to east of the mount center */
            DM_UP_DISPLACEMENT,    /*!< Up Displacement of the mount center */
            DM_OTA_OFFSET          /*!< Distance from the optical axis to the mount center*/
        } DomeMeasurements;

        enum DomeDirection
        {
            DOME_CW,
            DOME_CCW
        };
        enum DomeMotionCommand
        {
            MOTION_START,
            MOTION_STOP
        };

        /*! Dome Parking data type enum */
        enum DomeParkData
        {
            PARK_NONE,       /*!< 2-state parking (Open or Closed only)  */
            PARK_AZ,         /*!< Parking via azimuth angle control */
            PARK_AZ_ENCODER, /*!< Parking via azimuth encoder control */
        };

        /** \typedef ShutterOperation
                \brief Shutter operation command.
            */
        typedef enum
        {
            SHUTTER_OPEN, /*!< Open Shutter */
            SHUTTER_CLOSE /*!< Close Shutter */
        } ShutterOperation;

        /*! Mount Locking Policy */
        enum MountLockingPolicy
        {
            MOUNT_IGNORED,      /*!< Mount is ignored. Dome can park or unpark irrespective of mount parking status */
            MOUNT_LOCKS,        /*!< Mount Locks. Dome can park if mount is completely parked first. */
        };

        /** \typedef DomeState
                \brief Dome status
            */
        typedef enum
        {
            DOME_IDLE,      /*!< Dome is idle */
            DOME_MOVING,    /*!< Dome is in motion */
            DOME_SYNCED,    /*!< Dome is synced */
            DOME_PARKING,   /*!< Dome is parking */
            DOME_UNPARKING, /*!< Dome is unparking */
            DOME_PARKED,    /*!< Dome is parked */
            DOME_UNPARKED,  /*!< Dome is unparked */
            DOME_UNKNOWN,   /*!< Dome state is known */
            DOME_ERROR,     /*!< Dome has errors */
        } DomeState;

        /** \typedef ShutterStatus
                \brief Shutter Status
            */
        typedef enum
        {
            SHUTTER_OPENED,     /*!< Shutter is open */
            SHUTTER_CLOSED,     /*!< Shutter is closed */
            SHUTTER_MOVING,     /*!< Shutter in motion (opening or closing) */
            SHUTTER_UNKNOWN,    /*!< Shutter status is unknown */
            SHUTTER_ERROR       /*!< Shutter status is unknown */
        } ShutterState;

        enum
        {
            DOME_CAN_ABORT          = 1 << 0, /*!< Can the dome motion be aborted? */
            DOME_CAN_ABS_MOVE       = 1 << 1, /*!< Can the dome move to an absolute azimuth position? */
            DOME_CAN_REL_MOVE       = 1 << 2, /*!< Can the dome move to a relative position a number of degrees away from current position? Positive degrees is Clockwise direction. Negative Degrees is counter clock wise direction */
            DOME_CAN_PARK           = 1 << 3, /*!< Can the dome park and unpark itself? */
            DOME_CAN_SYNC           = 1 << 4, /*!< Can the dome sync to arbitrary position? */
            DOME_HAS_SHUTTER        = 1 << 5, /*!< Does the dome has a shutter than can be opened and closed electronically? */
            DOME_HAS_VARIABLE_SPEED = 1 << 6, /*!< Can the dome move in different configurable speeds? */
            DOME_HAS_BACKLASH       = 1 << 7  /*!< Can the dome compensate for backlash? */
        };

        /** \struct DomeConnection
                \brief Holds the connection mode of the Dome.
            */
        enum
        {
            CONNECTION_NONE   = 1 << 0, /** Do not use any connection plugin */
            CONNECTION_SERIAL = 1 << 1, /** For regular serial and bluetooth connections */
            CONNECTION_TCP    = 1 << 2  /** For Wired and WiFI connections */
        } DomeConnection;

        Dome();
        virtual ~Dome();

        virtual bool initProperties() override;
        virtual void ISGetProperties(const char * dev) override;
        virtual bool updateProperties() override;
        virtual bool ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n) override;
        virtual bool ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n) override;
        virtual bool ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n) override;
        virtual bool ISSnoopDevice(XMLEle * root) override;

        static void buttonHelper(const char * button_n, ISState state, void * context);

        /**
             * @brief setDomeConnection Set Dome connection mode. Child class should call this in the constructor before Dome registers
             * any connection interfaces
             * @param value ORed combination of DomeConnection values.
             */
        void setDomeConnection(const uint8_t &value);

        /**
             * @return Get current Dome connection mode
             */
        uint8_t getDomeConnection() const;

        /**
             * @brief GetDomeCapability returns the capability of the dome
             */
        uint32_t GetDomeCapability() const
        {
            return capability;
        }

        /**
             * @brief SetDomeCapability set the dome capabilities. All capabilities must be initialized.
             * @param cap pointer to dome capability
             */
        void SetDomeCapability(uint32_t cap);

        /**
             * @return True if dome support aborting motion
             */
        bool CanAbort()
        {
            return capability & DOME_CAN_ABORT;
        }

        /**
             * @return True if dome has absolute position encoders.
             */
        bool CanAbsMove()
        {
            return capability & DOME_CAN_ABS_MOVE;
        }

        /**
             * @return True if dome has relative position encoders.
             */
        bool CanRelMove()
        {
            return capability & DOME_CAN_REL_MOVE;
        }

        /**
             * @return True if dome can park.
             */
        bool CanPark()
        {
            return capability & DOME_CAN_PARK;
        }

        /**
             * @return True if dome can sync.
             */
        bool CanSync()
        {
            return capability & DOME_CAN_SYNC;
        }

        /**
             * @return True if dome has controllable shutter door
             */
        bool HasShutter()
        {
            return capability & DOME_HAS_SHUTTER;
        }

        /**
             * @return True if dome support multiple speeds
             */
        bool HasVariableSpeed()
        {
            return capability & DOME_HAS_VARIABLE_SPEED;
        }

        /**
         * @return True if the dome supports backlash
         */
        bool HasBacklash()
        {
            return capability & DOME_HAS_BACKLASH;
        }

        /**
             * @brief isLocked, is the dome currently locked?
             * @return True if lock status equals true, and TelescopeClosedLockTP is Telescope Locks.
             */
        bool isLocked();

        DomeState getDomeState() const
        {
            return m_DomeState;
        }
        void setDomeState(const DomeState &value);

        ShutterState getShutterState() const
        {
            return m_ShutterState;
        }
        void setShutterState(const ShutterState &value);

        IPState getMountState() const;

    protected:
        /**
             * @brief SetSpeed Set Dome speed. This does not initiate motion, it sets the speed for the next motion command. If motion is in progress, then change speed accordingly.
             * @param rpm Dome speed (RPM)
             * @return true if successful, false otherwise
             */
        virtual bool SetSpeed(double rpm);

        /** \brief Move the Dome in a particular direction.
                \param dir Direction of Dome, either DOME_CW or DOME_CCW.
                \return Return IPS_OK if dome operation is complete. IPS_BUSY if operation is in progress. IPS_ALERT on error.
            */
        virtual IPState Move(DomeDirection dir, DomeMotionCommand operation);

        /** \brief Move the Dome to an absolute azimuth.
                \param az The new position of the Dome.
                \return Return IPS_OK if motion is completed and Dome reached requested position. Return IPS_BUSY if Dome started motion to requested position and is in progress.
                        Return IPS_ALERT if there is an error.
            */
        virtual IPState MoveAbs(double az);

        /** \brief Move the Dome to an relative position.
                \param azDiff The relative azimuth angle to move. Positive degree is clock-wise direction. Negative degrees is counter clock-wise direction.
                \return Return IPS_OK if motion is completed and Dome reached requested position. Return IPS_BUSY if Dome started motion to requested position and is in progress.
                        Return IPS_ALERT if there is an error.
            */
        virtual IPState MoveRel(double azDiff);

        /**
             * \brief Sync sets the dome current azimuth as the supplied azimuth position
             * \return True if sync is successful, false otherwise.
             */
        virtual bool Sync(double az);

        /**
             * \brief Abort all dome motion
             * \return True if abort is successful, false otherwise.
             */
        virtual bool Abort();

        /**
             * \brief Goto Park Position. The park position is an absolute azimuth value.
             * \return Return IPS_OK if motion is completed and Dome reached park position. Return IPS_BUSY if Dome started motion to park requested position and is in progress.
                        Return -IPS_ALERT if there is an error.
             */
        virtual IPState Park();

        /**
             * \brief UnPark dome. The action of the Unpark command is dome specific, but it may include opening the shutter and moving to home position. When UnPark() is successful
             * The observatory should be in a ready state to utilize the mount to perform observations.
             * \return Return IPS_OK if motion is completed and Dome is unparked. Return IPS_BUSY if Dome unparking is in progress.
                        Return -IPS_ALERT if there is an error.
             */
        virtual IPState UnPark();

        /**
         * @brief SetBacklash Set the dome backlash compensation value
         * @param steps value in absolute steps to compensate
         * @return True if successful, false otherwise.
         */
        virtual bool SetBacklash(int32_t steps);

        /**
         * @brief SetBacklashEnabled Enables or disables the dome backlash compensation
         * @param enable flag to enable or disable backlash compensation
         * @return True if successful, false otherwise.
         */
        virtual bool SetBacklashEnabled(bool enabled);

        /**
             * \brief Open or Close shutter
             * \param operation Either open or close the shutter.
             * \return Return IPS_OK if shutter operation is complete. Return IPS_BUSY if shutter operation is in progress.
                        Return IPS_ALERT if there is an error.
             */
        virtual IPState ControlShutter(ShutterOperation operation);

        /**
             * @brief getShutterStatusString
             * @param status Status of shutter
             * @return Returns string representation of the shutter status
             */
        const char * GetShutterStatusString(ShutterState status);

        /**
             * \brief setParkDataType Sets the type of parking data stored in the park data file and presented to the user.
             * \param type parking data type. If PARK_NONE then no properties will be presented to the user for custom parking position.
             */
        void SetParkDataType(DomeParkData type);

        /**
             * @brief InitPark Loads parking data (stored in ~/.indi/ParkData.xml) that contains parking status
             * and parking position. InitPark() should be called after successful connection to the dome on startup.
             * @return True if loading is successful and data is read, false otherwise. On success, you must call
             * SetAzParkDefault() to set the default parking values. On failure, you must call
             * SetAzParkDefault() to set the default parking values in addition to SetAzPark()
             * to set the current parking position.
             */
        bool InitPark();

        /**
             * @brief isParked is dome currently parked?
             * @return True if parked, false otherwise.
             */
        bool isParked();

        /**
             * @brief SetParked Change the mount parking status. The data park file (stored in ~/.indi/ParkData.xml) is updated in the process.
             * @param isparked set to true if parked, false otherwise.
             */
        void SetParked(bool isparked);

        /**
             * @return Get current AZ parking position.
             */
        double GetAxis1Park();

        /**
             * @return Get default AZ parking position.
             */
        double GetAxis1ParkDefault();

        /**
             * @brief SetRAPark Set current AZ parking position. The data park file (stored in ~/.indi/ParkData.xml) is updated in the process.
             * @param value current Axis 1 value (AZ either in angles or encoder values as specified by the DomeParkData type).
             */
        void SetAxis1Park(double value);

        /**
             * @brief SetAxis1Park Set default AZ parking position.
             * @param value Default Axis 1 value (AZ either in angles or encoder values as specified by the DomeParkData type).
             */
        void SetAxis1ParkDefault(double steps);

        /**
             * @brief SetCurrentPark Set current coordinates/encoders value as the desired parking position
             * \note This function performs no action unless subclassed by the child class if required.
             */
        virtual bool SetCurrentPark();

        /**
             * @brief SetDefaultPark Set default coordinates/encoders value as the desired parking position
             * \note This function performs no action unless subclassed by the child class if required.
             */
        virtual bool SetDefaultPark();

        //Park
        const char * LoadParkData();
        bool WriteParkData();

        /**
             * @brief GetTargetAz
             * @param Az Returns Azimuth required to the dome in order to center the shutter aperture with telescope
             * @param Alt
             * @param minAz Returns Minimum azimuth in order to avoid any dome interference to the full aperture of the telescope
             * @param maxAz Returns Maximum azimuth in order to avoid any dome interference to the full aperture of the telescope
             * @return Returns false if it can't solve it due bad geometry of the observatory
             */
        bool GetTargetAz(double &Az, double &Alt, double &minAz, double &maxAz);

        /**
             * @brief Intersection Calculate the intersection of a ray and a sphere. The line segment is defined from p1 to p2.  The sphere is of radius r and centered at (0,0,0).
             * From http://local.wasp.uwa.edu.au/~pbourke/geometry/sphereline/
             * There are potentially two points of intersection given by
             * p := p1 + mu1 (p2 - p1)
             * p := p1 + mu2 (p2 - p1)
             * @param p1 First point
             * @param p2 Direction of the ray
             * @param r RADIUS of sphere
             * @param mu1 First point of potentional intersection.
             * @param mu2 Second point of potentional intersection.
             * @return Returns FALSE if the ray doesn't intersect the sphere.
             */
        bool Intersection(point3D p1, point3D p2, double r, double &mu1, double &mu2);

        /**
             * @brief OpticalCenter This function calculates the distance from the optical axis to the Dome center
             * @param MountCenter Distance from the Dome center to the point where mount axis crosses
             * @param dOpticalAxis Distance from the mount center to the optical axis.
             * @param Lat Latitude
             * @param Ah Hour Angle (in hours)
             * @param OP a 3D point from the optical center to the Dome center.
             * @return false in case of error.
             */
        bool OpticalCenter(point3D MountCenter, double dOpticalAxis, double Lat, double Ah, point3D &OP);

        /**
             * @brief OpticalVector This function calculates a second point for determining the optical axis
             * @param Az Azimuth
             * @param Alt Altitude
             * @param OV a 3D point that determines the optical line.
             * @return false in case of error.
             */
        bool OpticalVector(double Az, double Alt, point3D &OV);

        /**
             * @brief CheckHorizon Returns true if telescope points above horizon.
             * @param HA Hour angle
             * @param dec Declination
             * @param lat observer's latitude
             * @return True if telescope points above horizon, false otherwise.
             */
        bool CheckHorizon(double HA, double dec, double lat);

        /**
             * @brief saveConfigItems Saves the Device Port and Dome Presets in the configuration file
             * @param fp pointer to configuration file
             * @return true if successful, false otherwise.
             */
        virtual bool saveConfigItems(FILE * fp) override;

        /**
             * @brief updateCoords updates the horizontal coordinates (Az & Alt) of the mount from the snooped RA, DEC and observer's location.
             */
        void UpdateMountCoords();

        /**
             * @brief UpdateAutoSync This function calculates the target dome azimuth from the mount's target coordinates given the dome parameters.
             *  If the difference between the dome's and mount's azimuth angles exceeds the AutoSync threshold, the dome will be commanded to sync to the mount azimuth position.
             */
        virtual void UpdateAutoSync();

        /** \brief perform handshake with device to check communication */
        virtual bool Handshake();

        /**
         * Signal to concrete driver when Active Devices are updated.
         */
        virtual void ActiveDevicesUpdated() {};

        double Csc(double x);
        double Sec(double x);

        INDI::PropertyNumber DomeSpeedNP {1};

        INDI::PropertySwitch DomeMotionSP {2};

        INDI::PropertyNumber DomeAbsPosNP {1};

        INDI::PropertyNumber DomeRelPosNP {1};

        INDI::PropertySwitch AbortSP {1};

        INDI::PropertyNumber DomeParamNP {1};

        INDI::PropertyNumber DomeSyncNP {1};

        INDI::PropertySwitch DomeShutterSP {2};

        INDI::PropertySwitch ParkSP {2};

        INDI::PropertyNumber ParkPositionNP {1};

        INDI::PropertySwitch ParkOptionSP {3};

        INDI::PropertyText ActiveDeviceTP {3};
        enum
        {
            ACTIVE_MOUNT,
            ACTIVE_INPUT,
            ACTIVE_OUTPUT
        };

        // Switch to lock id mount is unparked
        INDI::PropertySwitch MountPolicySP {2};

        // Shutter control on Park/Unpark
        INDI::PropertySwitch ShutterParkPolicySP {2};
        enum
        {
            SHUTTER_CLOSE_ON_PARK,
            SHUTTER_OPEN_ON_UNPARK,
        };

        INDI::PropertyNumber PresetNP {3};
        INDI::PropertySwitch PresetGotoSP {3};

        INDI::PropertyNumber DomeMeasurementsNP {6};

        INDI::PropertySwitch OTASideSP {5};
        // 0 is East, 1 is West, 2 is as reported by mout, 3 as deducted by Hour Angle
        // 4 ignore pier side and perform as in a fork mount
        enum
        {
            DM_OTA_SIDE_EAST,
            DM_OTA_SIDE_WEST,
            DM_OTA_SIDE_MOUNT,
            DM_OTA_SIDE_HA,
            DM_OTA_SIDE_IGNORE
        };

        int mountOTASide = 0; // Side of the telescope with respect of the mount, 1: west, -1: east, 0 not reported
        INDI::PropertySwitch DomeAutoSyncSP {2};

        // Backlash toggle
        INDI::PropertySwitch DomeBacklashSP {2};

        // Backlash steps
        INDI::PropertyNumber DomeBacklashNP {1};

        uint32_t capability;
        DomeParkData parkDataType;

        double prev_az, prev_alt, prev_ra, prev_dec;

        // For Serial and TCP connections
        int PortFD = -1;

        Connection::Serial * serialConnection = nullptr;
        Connection::TCP * tcpConnection       = nullptr;

        // States
        DomeState m_DomeState;
        ShutterState m_ShutterState;
        IPState m_MountState;

        // Observer geographic coords. Snooped from mount driver.
        IGeographicCoordinates observer;
        // Do we have valid geographic coords from mount driver?
        bool HaveLatLong = false;

        // Mount horizontal and equatorial coords. Snoops from mount driver.
        INDI::IHorizontalCoordinates mountHoriztonalCoords;
        INDI::IEquatorialCoordinates mountEquatorialCoords;
        // Do we have valid coords from mount driver?
        bool HaveRaDec = false;

    private:
        void processButton(const char * button_n, ISState state);
        void triggerSnoop(const char * driverName, const char * propertyName);
        /**
         * @brief SyncParkStatus Update the state and switches for parking
         * @param isparked True if parked, false otherwise.
         */
        void SyncParkStatus(bool isparked);
        /**
         * @brief LoadParkXML Read and process park XML data.
         * @return error string if there is problem opening the file
         */
        const char * LoadParkXML();

        /**
         * @brief Validate a file name
         * @param file_name File name
         * @return True if the file name is valid otherwise false.
         */
        std::string GetHomeDirectory() const;

        Controller * controller = nullptr;

        bool IsParked = false;
        bool IsMountParked = true;
        bool IsLocked = true;
        bool AutoSyncWarning = false;
        bool UseHourAngle = false;

        const char * ParkDeviceName;
        const std::string ParkDataFileName;
        INDI::Timer m_MountUpdateTimer;
        //int m_HorizontalUpdateTimerID { -1 };
        XMLEle * ParkdataXmlRoot, *ParkdeviceXml, *ParkstatusXml, *ParkpositionXml, *ParkpositionAxis1Xml;

        double Axis1ParkPosition;
        double Axis1DefaultParkPosition;

        bool callHandshake();
        uint8_t domeConnection = CONNECTION_SERIAL | CONNECTION_TCP;

        // How often we update horizontal coordinates (10 seconds).
        static constexpr uint32_t HORZ_UPDATE_TIMER { 10000 };
};

}
