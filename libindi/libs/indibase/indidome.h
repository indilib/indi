/*******************************************************************************
 INDI Dome Base Class
 Copyright(c) 2014 Jasem Mutlaq. All rights reserved.

 The code used calculate dome target AZ and ZD is written by Ferran Casarramona, and adapted from code from Markus Wildi.
 The transformations are based on the paper Matrix Method for Coodinates Transformation written by Toshimi Taki (http://www.asahi-net.or.jp/~zs3t-tk).

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

#ifndef INDIDOME_H
#define INDIDOME_H

#include <libnova.h>

#include "defaultdevice.h"
#include "indicontroller.h"
#include "indidomeinterface.h"

// Defines a point in a 3 dimension space
typedef struct
{
    double x,y,z;
} point3D;

/**
 * \class INDI::Dome
   \brief Class to provide general functionality of a Dome device.

   Both relative and absolute position domes supported. Furthermore, if no position feedback is available from the dome,
   an open-loop control is possible using timers, speed presets (RPM), and direction of motion (Clockwise and Counter Clockwise).
   Developers need to subclass INDI::Dome to implement any driver for Domes within INDI.

  \note The code used calculate dome target AZ and ZD is written by Ferran Casarramona, and adapted from code from Markus Wildi. The transformations are based on the paper Matrix Method for Coodinates
 Transformation written by Toshimi Taki (http://www.asahi-net.or.jp/~zs3t-tk).

\author Jasem Mutlaq
*/
class INDI::Dome : public INDI::DefaultDevice, public INDI::DomeInterface
{
    public:

    /** \typedef DomeMeasurements
        \brief Measurements necessary for dome-slit synchronization. All values are in meters. The displacements are measured from the true dome centre, and the dome is assumed spherical.
        \Note: The mount centre is the point where RA and Dec. axis crosses, no matter the kind of mount. For example, for a fork mount this displacement is typically 0 if it's perfectly centred with RA axis.
    */
    typedef enum
    {
        DM_DOME_RADIUS,           /*!< Dome RADIUS */
        DM_SHUTTER_WIDTH,         /*!< Shutter width */
        DM_NORTH_DISPLACEMENT,    /*!< Displacement to north of the mount center */
        DM_EAST_DISPLACEMENT,     /*!< Displacement to east of the mount center */
        DM_UP_DISPLACEMENT,       /*!< Up Displacement of the mount center */
        DM_OTA_OFFSET             /*!< Distance from the optical axis to the mount center*/
    } DomeMeasurements;

        Dome();
        virtual ~Dome();

        virtual bool initProperties();
        virtual void ISGetProperties (const char *dev);
        virtual bool updateProperties();
        virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
        virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
        virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
        virtual bool ISSnoopDevice (XMLEle *root);

        static void buttonHelper(const char * button_n, ISState state, void *context);

    protected:

        /**
         * @brief GetTargetAz
         * @param RA Right ascension in hours (0.0 - 24.0)
         * @param Az Returns Azimuth required to the dome in order to center the shutter aperture with telescope
         * @param Alt
         * @param minAz Returns Minimum azimuth in order to avoid any dome interference to the full aperture of the telescope
         * @param maxAz Returns Maximum azimuth in order to avoid any dome interference to the full aperture of the telescope
         * @return Returns false if it can't solve it due bad geometry of the observatory
         */
        bool GetTargetAz(double & Az, double & Alt, double & minAz, double & maxAz);

        /**
         * @brief Intersection Calculate the intersection of a ray and a sphere. The line segment is defined from p1 to p2.  The sphere is of radius r and centered at sc.
         * From http://local.wasp.uwa.edu.au/~pbourke/geometry/sphereline/
         * There are potentially two points of intersection given by
         * p := p1 + mu1 (p2 - p1)
         * p := p1 + mu2 (p2 - p1)
         * @param p1 First ray
         * @param p2 Second ray
         * @param sc Center of sphere
         * @param r RADIUS of sphere
         * @param mu1 First point of potentional intersection.
         * @param mu2 Second point of potentional intersection.
         * @return Returns FALSE if the ray doesn't intersect the sphere.
         */
        bool Intersection(point3D p1, point3D p2, point3D sc, double r, double & mu1, double & mu2);

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
         * @param OP Optical center
         * @param Az Azimuth
         * @param Alt Altitude
         * @param OV a 3D point that determines the optical line.
         * @return false in case of error.
         */
        bool OpticalVector(point3D OP, double Az, double Alt, point3D & OV);

        /**
         * @brief CheckHorizon Returns true if telescope points above horizon.
         * @param ha Hour angle
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
        virtual bool saveConfigItems(FILE *fp);

        /**
         * @brief updateCoords updates the horizontal coordinates (Az & Alt) of the mount from the snooped RA, DEC and observer's location.
         */
        void UpdateMountCoords();

        /**
         * @brief UpdateAutoSync This function calculates the target dome azimuth from the mount's target coordinates given the dome parameters.
         *  If the difference between the dome's and mount's azimuth angles exceeds the AutoSync threshold, the dome will be commanded to sync to the mount azimuth position.
         */
        virtual void UpdateAutoSync();


        double Csc(double x);
        double Sec(double x);

        ITextVectorProperty PortTP;
        IText PortT[1];

        ITextVectorProperty ActiveDeviceTP;
        IText ActiveDeviceT[1];

        INumber PresetN[3];
        INumberVectorProperty PresetNP;
        ISwitch PresetGotoS[3];
        ISwitchVectorProperty PresetGotoSP; 
        INumber DomeMeasurementsN[6];
        INumberVectorProperty DomeMeasurementsNP;
        ISwitchVectorProperty DomeAutoSyncSP;
        ISwitch DomeAutoSyncS[2];

        void processButton(const char * button_n, ISState state);

        INDI::Controller *controller;

        struct ln_lnlat_posn observer;
        struct ln_hrz_posn mountHoriztonalCoords;
        struct ln_equ_posn mountEquatorialCoords;
        IPState mountState;
        double prev_az, prev_alt, prev_ra, prev_dec;

};

#endif // INDIDOME_H
