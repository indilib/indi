/*******************************************************************************
 Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include "telescope_simulator.h"

#include "indicom.h"

#include <cmath>
#include <cstring>
#include <memory>

// We declare an auto pointer to ScopeSim.
static std::unique_ptr<ScopeSim> telescope_sim(new ScopeSim());

#define GOTO_RATE      6.5      /* slew rate, degrees/s */
#define SLEW_RATE      2.5      /* slew rate, degrees/s */
#define FINE_SLEW_RATE 0.5      /* slew rate, degrees/s */

#define GOTO_LIMIT      5       /* Move at GOTO_RATE until distance from target is GOTO_LIMIT degrees */
#define SLEW_LIMIT      1       /* Move at SLEW_LIMIT until distance from target is SLEW_LIMIT degrees */

#define RA_AXIS     0
#define DEC_AXIS    1
#define GUIDE_NORTH 0
#define GUIDE_SOUTH 1
#define GUIDE_WEST  0
#define GUIDE_EAST  1

#define MIN_AZ_FLIP 180
#define MAX_AZ_FLIP 200

void ISPoll(void *p);

void ISGetProperties(const char *dev)
{
    telescope_sim->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    telescope_sim->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    telescope_sim->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    telescope_sim->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
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
void ISSnoopDevice(XMLEle *root)
{
    telescope_sim->ISSnoopDevice(root);
}

ScopeSim::ScopeSim()
{
    DBG_SCOPE = static_cast<uint32_t>(INDI::Logger::getInstance().addDebugLevel("Scope Verbose", "SCOPE"));

    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT | /*TELESCOPE_HAS_PIER_SIDE_SIMULATION |*/
                           TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION | TELESCOPE_HAS_TRACK_MODE | TELESCOPE_CAN_CONTROL_TRACK | TELESCOPE_HAS_TRACK_RATE,
                           4);

    /* initialize random seed: */
    srand(static_cast<uint32_t>(time(nullptr)));

    // assume no pier side property
    currentPierSide = lastPierSide = PIER_UNKNOWN;

    // initalise axis positions, for GEM pointing at pole, counterweight down
    axisPrimary.setDegrees(90.0);
    axisSecondary.setDegrees(90.0);
}

const char *ScopeSim::getDefaultName()
{
    return "Telescope Simulator";
}

bool ScopeSim::initProperties()
{
    /* Make sure to init parent properties first */
    INDI::Telescope::initProperties();

#ifdef USE_EQUATORIAL_PE
    /* Simulated periodic error in RA, DEC */
    IUFillNumber(&EqPEN[RA_AXIS], "RA_PE", "RA (hh:mm:ss)", "%010.6m", 0, 24, 0, 15.);
    IUFillNumber(&EqPEN[DEC_AXIS], "DEC_PE", "DEC (dd:mm:ss)", "%010.6m", -90, 90, 0, 15.);
    IUFillNumberVector(&EqPENV, EqPEN, 2, getDeviceName(), "EQUATORIAL_PE", "Periodic Error", MOTION_TAB, IP_RO, 60,
                       IPS_IDLE);

    /* Enable client to manually add periodic error northward or southward for simulation purposes */
    IUFillSwitch(&PEErrNSS[DIRECTION_NORTH], "PE_N", "North", ISS_OFF);
    IUFillSwitch(&PEErrNSS[DIRECTION_SOUTH], "PE_S", "South", ISS_OFF);
    IUFillSwitchVector(&PEErrNSSP, PEErrNSS, 2, getDeviceName(), "PE_NS", "PE N/S", MOTION_TAB, IP_RW, ISR_ATMOST1, 60,
                       IPS_IDLE);

    /* Enable client to manually add periodic error westward or easthward for simulation purposes */
    IUFillSwitch(&PEErrWES[DIRECTION_WEST], "PE_W", "West", ISS_OFF);
    IUFillSwitch(&PEErrWES[DIRECTION_EAST], "PE_E", "East", ISS_OFF);
    IUFillSwitchVector(&PEErrWESP, PEErrWES, 2, getDeviceName(), "PE_WE", "PE W/E", MOTION_TAB, IP_RW, ISR_ATMOST1, 60,
                       IPS_IDLE);
#endif

    // mount type and alignment properties, these are in the Simulation tab
    IUFillSwitch(&mountTypeS[Alignment::ALTAZ], "ALTAZ", "AltAz", ISS_OFF);
    IUFillSwitch(&mountTypeS[Alignment::EQ_FORK], "EQ_FORK", "Fork (Eq)", ISS_ON);
    IUFillSwitch(&mountTypeS[Alignment::EQ_GEM], "EQ_GEM", "GEM", ISS_OFF);
    IUFillSwitchVector(&mountTypeSP, mountTypeS, 3, getDeviceName(), "MOUNT_TYPE", "Mount Type", "Simulation",
                       IP_RW, ISR_1OFMANY, 60, IPS_IDLE );

    /* How fast do we guide compared to sidereal rate */
    IUFillNumber(&GuideRateN[RA_AXIS], "GUIDE_RATE_WE", "W/E Rate", "%g", 0, 1, 0.1, 0.5);
    IUFillNumber(&GuideRateN[DEC_AXIS], "GUIDE_RATE_NS", "N/S Rate", "%g", 0, 1, 0.1, 0.5);
    IUFillNumberVector(&GuideRateNP, GuideRateN, 2, getDeviceName(), "GUIDE_RATE", "Guiding Rate", MOTION_TAB, IP_RW, 0,
                       IPS_IDLE);

    IUFillSwitch(&SlewRateS[SLEW_GUIDE], "SLEW_GUIDE", "Guide", ISS_OFF);
    IUFillSwitch(&SlewRateS[SLEW_CENTERING], "SLEW_CENTERING", "Centering", ISS_OFF);
    IUFillSwitch(&SlewRateS[SLEW_FIND], "SLEW_FIND", "Find", ISS_OFF);
    IUFillSwitch(&SlewRateS[SLEW_MAX], "SLEW_MAX", "Max", ISS_ON);
    IUFillSwitchVector(&SlewRateSP, SlewRateS, 4, getDeviceName(), "TELESCOPE_SLEW_RATE", "Slew Rate", MOTION_TAB,
                       IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Add Tracking Modes
    AddTrackMode("TRACK_SIDEREAL", "Sidereal", true);
    AddTrackMode("TRACK_CUSTOM", "Custom");

    // Let's simulate it to be an F/7.5 120mm telescope
    ScopeParametersN[0].value = 120;
    ScopeParametersN[1].value = 900;
    ScopeParametersN[2].value = 120;
    ScopeParametersN[3].value = 900;

    SetParkDataType(PARK_RA_DEC);

    initGuiderProperties(getDeviceName(), MOTION_TAB);

    /* Add debug controls so we may debug driver if necessary */
    addDebugControl();

    setDriverInterface(getDriverInterface() | GUIDER_INTERFACE);

//    double longitude = 0, latitude = 90;
//    // Get value from config file if it exists.
//    IUGetConfigNumber(getDeviceName(), "GEOGRAPHIC_COORD", "LONG", &longitude);
//    currentRA  = get_local_sidereal_time(longitude);
//    IUGetConfigNumber(getDeviceName(), "GEOGRAPHIC_COORD", "LAT", &latitude);
//    currentDEC = latitude > 0 ? 90 : -90;

//    alignment.latitude = Angle(latitude);
//    alignment.longitude = Angle(longitude);

    setDefaultPollingPeriod(250);

    return true;
}

void ScopeSim::ISGetProperties(const char *dev)
{
    /* First we let our parent populate */
    INDI::Telescope::ISGetProperties(dev);

    if (!isConnected())
    {
        defineSwitch(&mountTypeSP);
        loadConfig(true, mountTypeSP.name);
    }

    /*
    if (isConnected())
    {
        defineNumber(&GuideNSNP);
        defineNumber(&GuideWENP);
        defineNumber(&GuideRateNP);
        defineNumber(&EqPENV);
        defineSwitch(&PEErrNSSP);
        defineSwitch(&PEErrWESP);
    }
    */
}

bool ScopeSim::updateProperties()
{
    // update the pier side capability depending on the mount type
    uint32_t cap = GetTelescopeCapability();

    int type = IUFindOnSwitchIndex(&mountTypeSP);
    LOGF_INFO("updatePropertiers: Mount Type set to %i", type);
    if (type == Alignment::EQ_GEM)
    {
        cap |= TELESCOPE_HAS_PIER_SIDE;
    }
    else
    {
        cap &= ~static_cast<uint32_t>(TELESCOPE_HAS_PIER_SIDE);
    }

    SetTelescopeCapability(cap, 4);

    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        defineNumber(&GuideNSNP);
        defineNumber(&GuideWENP);
        defineNumber(&GuideRateNP);
        loadConfig(true, GuideRateNP.name);

        // the simulation properties are only available when not connected
        deleteProperty("MOUNT_TYPE");

#ifdef USE_EQUATORIAL_PE
        defineNumber(&EqPENV);
        defineSwitch(&PEErrNSSP);
        defineSwitch(&PEErrWESP);
#endif

        if (InitPark())
        {
            // If loading parking data is successful, we just set the default parking values.
            SetAxis1ParkDefault(currentRA);
            SetAxis2ParkDefault(currentDEC);

            if (isParked())
            {
                currentRA = ParkPositionN[AXIS_RA].value;
                currentDEC = ParkPositionN[AXIS_DE].value;
            }
        }
        else
        {
            // Otherwise, we set all parking data to default in case no parking data is found.
            SetAxis1Park(currentRA);
            SetAxis2Park(currentDEC);
            SetAxis1ParkDefault(currentRA);
            SetAxis2ParkDefault(currentDEC);
        }
        
        sendTimeFromSystem();
    }
    else
    {
        // the simulation properties are only defined when not connected
        defineSwitch(&mountTypeSP);

        deleteProperty(GuideNSNP.name);
        deleteProperty(GuideWENP.name);

#ifdef USE_EQUATORIAL_PE
        deleteProperty(EqPENV.name);
        deleteProperty(PEErrNSSP.name);
        deleteProperty(PEErrWESP.name);
#endif
        deleteProperty(GuideRateNP.name);
    }

    return true;
}

bool ScopeSim::Connect()
{
    LOG_INFO("Telescope simulator is online.");
    SetTimer(POLLMS);

    return true;
}

bool ScopeSim::Disconnect()
{
    LOG_INFO("Telescope simulator is offline.");
    return true;
}

bool ScopeSim::ReadScopeStatus()
{
    // new axis control
    axisPrimary.update();
    axisSecondary.update();

    Angle ra, dec;
    alignment.MountToRaDec(axisPrimary.position, axisSecondary.position, &ra, &dec);

    // move both axes
    currentRA = ra.Hours();
    currentDEC = dec.Degrees();

    // update properties from the axis
    if (alignment.mountType == Alignment::MOUNT_TYPE::EQ_GEM)
    {
        setPierSide(fabs(axisSecondary.position.Degrees()) > 90 ? PIER_WEST : PIER_EAST);
    }

    bool slewing = axisPrimary.isSlewing || axisSecondary.isSlewing;
    switch (TrackState)
    {
        case SCOPE_PARKING:
            if (!slewing)
            {
                SetParked(true);
                EqNP.s = IPS_IDLE;
                LOG_INFO("Telescope slew is complete. Parking...");
            }
            break;
        case SCOPE_SLEWING:
            if (!slewing)
            {
                TrackState = axisPrimary.isTracking() ? SCOPE_TRACKING : SCOPE_IDLE;
                EqNP.s = IPS_IDLE;
                LOG_INFO("Telescope slew is complete. Tracking...");
            }
            break;
        default:
            break;
    }

    if (guidingEW && !axisPrimary.IsGuiding())
    {
        GuideWENP.np[0].value = 0;
        GuideWENP.np[1].value = 0;
        GuideComplete(INDI_EQ_AXIS::AXIS_RA);
        guidingEW = false;
    }

    if (guidingNS && !axisSecondary.IsGuiding())
    {
        GuideNSNP.np[0].value = 0;
        GuideNSNP.np[1].value = 0;
        GuideComplete(INDI_EQ_AXIS::AXIS_DE);
        guidingNS = false;
    }

    LOGF_DEBUG("%s: %f, ra %f", axisPrimary.axisName, axisPrimary.position.Degrees(), ra.Hours());
    LOGF_DEBUG("%s: %f, dec %f", axisSecondary.axisName, axisSecondary.position.Degrees(), dec.Degrees());

#ifdef OLD
    static struct timeval lastTime
    {
        0, 0
    };
    struct timeval currentTime
    {
        0, 0
    };
    double deltaTimeSecs = 0, da_ra = 0, da_dec = 0, deltaRa = 0, deltaDec = 0, ra_guide_dt = 0, dec_guide_dt = 0;
    int nlocked, ns_guide_dir = -1, we_guide_dir = -1;

#ifdef USE_EQUATORIAL_PE
    static double last_dx = 0, last_dy = 0;
    char RA_DISP[64], DEC_DISP[64], RA_GUIDE[64], DEC_GUIDE[64], RA_PE[64], DEC_PE[64], RA_TARGET[64], DEC_TARGET[64];
#endif

    /* update elapsed time since last poll, don't presume exactly POLLMS */
    gettimeofday(&currentTime, nullptr);

    if (lastTime.tv_sec == 0 && lastTime.tv_usec == 0)
        lastTime = currentTime;

    // Time diff in seconds
    deltaTimeSecs  = currentTime.tv_sec - lastTime.tv_sec + (currentTime.tv_usec - lastTime.tv_usec) / 1e6;
    lastTime = currentTime;

    if (fabs(targetRA - currentRA) * 15. >= GOTO_LIMIT)
        da_ra = GOTO_RATE * deltaTimeSecs;
    else if (fabs(targetRA - currentRA) * 15. >= SLEW_LIMIT)
        da_ra = SLEW_RATE * deltaTimeSecs;
    else
        da_ra = FINE_SLEW_RATE * deltaTimeSecs;

    if (fabs(targetDEC - currentDEC) >= GOTO_LIMIT)
        da_dec = GOTO_RATE * deltaTimeSecs;
    else if (fabs(targetDEC - currentDEC) >= SLEW_LIMIT)
        da_dec = SLEW_RATE * deltaTimeSecs;
    else
        da_dec = FINE_SLEW_RATE * deltaTimeSecs;

    if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY)
    {
        int rate = IUFindOnSwitchIndex(&SlewRateSP);

        switch (rate)
        {
            case SLEW_GUIDE:
                da_ra  = TrackRateN[AXIS_RA].value / (3600.0 * 15) * GuideRateN[RA_AXIS].value * deltaTimeSecs;
                da_dec = TrackRateN[AXIS_RA].value / 3600.0 * GuideRateN[DEC_AXIS].value * deltaTimeSecs;;
                break;

            case SLEW_CENTERING:
                da_ra  = FINE_SLEW_RATE * deltaTimeSecs * .1;
                da_dec = FINE_SLEW_RATE * deltaTimeSecs * .1;
                break;

            case SLEW_FIND:
                da_ra  = SLEW_RATE * deltaTimeSecs;
                da_dec = SLEW_RATE * deltaTimeSecs;
                break;

            default:
                da_ra  = GOTO_RATE * deltaTimeSecs;
                da_dec = GOTO_RATE * deltaTimeSecs;
                break;
        }

        switch (MovementNSSP.s)
        {
            case IPS_BUSY:
                if (MovementNSS[DIRECTION_NORTH].s == ISS_ON)
                    currentDEC += da_dec;
                else if (MovementNSS[DIRECTION_SOUTH].s == ISS_ON)
                    currentDEC -= da_dec;
                break;

            default:
                break;
        }

        switch (MovementWESP.s)
        {
            case IPS_BUSY:

                if (MovementWES[DIRECTION_WEST].s == ISS_ON)
                    currentRA -= da_ra / 15.;
                else if (MovementWES[DIRECTION_EAST].s == ISS_ON)
                    currentRA += da_ra / 15.;
                break;

            default:
                break;
        }

        NewRaDec(currentRA, currentDEC);

        return true;
    }

    /* Process per current state. We check the state of EQUATORIAL_EOD_COORDS_REQUEST and act acoordingly */
    switch (TrackState)
    {
        /*case SCOPE_IDLE:
            EqNP.s = IPS_IDLE;
            break;*/
        case SCOPE_SLEWING:
        case SCOPE_PARKING:
            /* slewing - nail it when both within one pulse @ SLEWRATE */
            nlocked = 0;        // seems to be some sort of state variable

            deltaRa = targetRA - currentRA;

            // Always take the shortcut, don't go all around the globe
            // If the difference between target and current is more than 12 hours, then we need to take the shortest path
            if (deltaRa > 12)
                deltaRa -= 24;
            else if (deltaRa < -12)
                deltaRa += 24;

            // In meridian flip, move to the position by doing a full rotation
            if (forceMeridianFlip)
            {
                // set deltaRa according to the target pier side so that the
                // slew is away from the meridian until the direction to go is towards
                // the target.
                switch (currentPierSide)
                {
                case PIER_EAST:
                    // force Ra move direction to be positive, i.e. to the West,
                    // until it is large and positive
                    if (deltaRa < 9)
                        deltaRa = 1;
                    else
                        forceMeridianFlip = false;
                    break;
                case PIER_WEST:
                    // force Ra move direction to be negative, i.e. East,
                    // until it is large and negative
                    if (deltaRa > -9)
                        deltaRa = -1;
                    else
                        forceMeridianFlip = false;
                    break;
                case PIER_UNKNOWN:
                    break;
                }
            }

            if (fabs(deltaRa) * 15. <= da_ra)
            {
                currentRA = targetRA;
                nlocked++;
            }
            else if (deltaRa > 0)
                currentRA += da_ra / 15.;
            else
                currentRA -= da_ra / 15.;

            currentRA = range24(currentRA);

            deltaDec = targetDEC - currentDEC;
            if (fabs(deltaDec) <= da_dec)
            {
                currentDEC = targetDEC;
                nlocked++;
            }
            else if (deltaDec > 0)
                currentDEC += da_dec;
            else
                currentDEC -= da_dec;

            EqNP.s = IPS_BUSY;

            if (nlocked == 2)
            {
                forceMeridianFlip = false;

                if (TrackState == SCOPE_SLEWING)
                {
                    // Initially no PE in both axis.
#ifdef USE_EQUATORIAL_PE
                    EqPEN[0].value = currentRA;
                    EqPEN[1].value = currentDEC;
                    IDSetNumber(&EqPENV, nullptr);
#endif

                    TrackState = SCOPE_TRACKING;

                    if (IUFindOnSwitchIndex(&SlewRateSP) != SLEW_CENTERING)
                    {
                        IUResetSwitch(&SlewRateSP);
                        SlewRateS[SLEW_CENTERING].s = ISS_ON;
                        IDSetSwitch(&SlewRateSP, nullptr);
                    }


                    EqNP.s = IPS_OK;
                    LOG_INFO("Telescope slew is complete. Tracking...");
                }
                else
                {
                    SetParked(true);
                    EqNP.s = IPS_IDLE;
                }
            }

            break;

        case SCOPE_IDLE:
            //currentRA += (TRACKRATE_SIDEREAL/3600.0 * dt) / 15.0;
            currentRA += (TrackRateN[AXIS_RA].value / 3600.0 * deltaTimeSecs) / 15.0;
            currentRA = range24(currentRA);
            break;

        case SCOPE_TRACKING:
            // In case of custom tracking rate
            if (TrackModeS[1].s == ISS_ON)
            {
                currentRA  += ( ((TRACKRATE_SIDEREAL / 3600.0) - (TrackRateN[AXIS_RA].value / 3600.0)) * deltaTimeSecs) / 15.0;
                currentDEC += ( (TrackRateN[AXIS_DE].value / 3600.0) * deltaTimeSecs);
            }

            deltaTimeSecs *= 1000;

            if (guiderNSTarget[GUIDE_NORTH] > 0)
            {
                LOGF_DEBUG("Commanded to GUIDE NORTH for %g ms", guiderNSTarget[GUIDE_NORTH]);
                ns_guide_dir = GUIDE_NORTH;
            }
            else if (guiderNSTarget[GUIDE_SOUTH] > 0)
            {
                LOGF_DEBUG("Commanded to GUIDE SOUTH for %g ms", guiderNSTarget[GUIDE_SOUTH]);
                ns_guide_dir = GUIDE_SOUTH;
            }

            // WE Guide Selection
            if (guiderEWTarget[GUIDE_WEST] > 0)
            {
                we_guide_dir = GUIDE_WEST;
                LOGF_DEBUG("Commanded to GUIDE WEST for %g ms", guiderEWTarget[GUIDE_WEST]);
            }
            else if (guiderEWTarget[GUIDE_EAST] > 0)
            {
                we_guide_dir = GUIDE_EAST;
                LOGF_DEBUG("Commanded to GUIDE EAST for %g ms", guiderEWTarget[GUIDE_EAST]);
            }

            if ( (ns_guide_dir != -1 || we_guide_dir != -1) && IUFindOnSwitchIndex(&SlewRateSP) != SLEW_GUIDE)
            {
                IUResetSwitch(&SlewRateSP);
                SlewRateS[SLEW_GUIDE].s = ISS_ON;
                IDSetSwitch(&SlewRateSP, nullptr);
            }

            if (ns_guide_dir != -1)
            {
                dec_guide_dt = (TrackRateN[AXIS_RA].value * GuideRateN[DEC_AXIS].value * guiderNSTarget[ns_guide_dir] / 1000.0 *
                                (ns_guide_dir == GUIDE_NORTH ? 1 : -1)) / 3600.0;

                guiderNSTarget[ns_guide_dir] = 0;
                GuideNSNP.s = IPS_IDLE;
                IDSetNumber(&GuideNSNP, nullptr);

#ifdef USE_EQUATORIAL_PE
                EqPEN[DEC_AXIS].value += dec_guide_dt;
#else
                currentDEC += dec_guide_dt;
#endif
            }

            if (we_guide_dir != -1)
            {
                ra_guide_dt = (TrackRateN[AXIS_RA].value * GuideRateN[RA_AXIS].value * guiderEWTarget[we_guide_dir] / 1000.0 *
                               (we_guide_dir == GUIDE_WEST ? -1 : 1)) / (3600.0 * 15.0);

                ra_guide_dt /= (cos(currentDEC * 0.0174532925));

                guiderEWTarget[we_guide_dir] = 0;
                GuideWENP.s = IPS_IDLE;
                IDSetNumber(&GuideWENP, nullptr);

#ifdef USE_EQUATORIAL_PE
                EqPEN[RA_AXIS].value += ra_guide_dt;
#else
                currentRA += ra_guide_dt;
#endif
            }

            //Mention the followng:
            // Current RA displacemet and direction
            // Current DEC displacement and direction
            // Amount of RA GUIDING correction and direction
            // Amount of DEC GUIDING correction and direction

#ifdef USE_EQUATORIAL_PE

            dx = EqPEN[RA_AXIS].value - targetRA;
            dy = EqPEN[DEC_AXIS].value - targetDEC;
            fs_sexa(RA_DISP, fabs(dx), 2, 3600);
            fs_sexa(DEC_DISP, fabs(dy), 2, 3600);

            fs_sexa(RA_GUIDE, fabs(ra_guide_dt), 2, 3600);
            fs_sexa(DEC_GUIDE, fabs(dec_guide_dt), 2, 3600);

            fs_sexa(RA_PE, EqPEN[RA_AXIS].value, 2, 3600);
            fs_sexa(DEC_PE, EqPEN[DEC_AXIS].value, 2, 3600);

            fs_sexa(RA_TARGET, targetRA, 2, 3600);
            fs_sexa(DEC_TARGET, targetDEC, 2, 3600);

            if (dx != last_dx || dy != last_dy || ra_guide_dt != 0.0 || dec_guide_dt != 0.0)
            {
                last_dx = dx;
                last_dy = dy;
                //LOGF_DEBUG("dt is %g\n", dt);
                LOGF_DEBUG("RA Displacement (%c%s) %s -- %s of target RA %s", dx >= 0 ? '+' : '-',
                           RA_DISP, RA_PE, (EqPEN[RA_AXIS].value - targetRA) > 0 ? "East" : "West", RA_TARGET);
                LOGF_DEBUG("DEC Displacement (%c%s) %s -- %s of target RA %s", dy >= 0 ? '+' : '-',
                           DEC_DISP, DEC_PE, (EqPEN[DEC_AXIS].value - targetDEC) > 0 ? "North" : "South", DEC_TARGET);
                LOGF_DEBUG("RA Guide Correction (%g) %s -- Direction %s", ra_guide_dt, RA_GUIDE,
                           ra_guide_dt > 0 ? "East" : "West");
                LOGF_DEBUG("DEC Guide Correction (%g) %s -- Direction %s", dec_guide_dt, DEC_GUIDE,
                           dec_guide_dt > 0 ? "North" : "South");
            }

            if (ns_guide_dir != -1 || we_guide_dir != -1)
                IDSetNumber(&EqPENV, nullptr);
#endif

            break;

        default:
            break;
    }
#endif
    char RAStr[64], DecStr[64];

    fs_sexa(RAStr, currentRA, 2, 3600);
    fs_sexa(DecStr, currentDEC, 2, 3600);

    DEBUGF(DBG_SCOPE, "Current RA: %s Current DEC: %s", RAStr, DecStr);

    NewRaDec(currentRA, currentDEC);

    return true;
}

bool ScopeSim::Goto(double r, double d)
{
    StartSlew(r, d, SCOPE_SLEWING);
    return true;
}

bool ScopeSim::Sync(double ra, double dec)
{
    Angle a1, a2;
    alignment.RaDecToMount(Angle(ra * 15.0), Angle(dec), &a1, &a2);
    axisPrimary.setDegrees(a1.Degrees());
    axisSecondary.setDegrees(a2.Degrees());

    Angle r, d;
    alignment.MountToRaDec(a1, a2, &r, &d);
    LOGF_DEBUG("sync to %f, %f, reached %f, %f", ra, dec, r.Hours(), d.Degrees());
    currentRA = r.Hours();
    currentDEC = d.Degrees();

#ifdef USE_EQUATORIAL_PE
    EqPEN[RA_AXIS].value  = ra;
    EqPEN[DEC_AXIS].value = dec;
    IDSetNumber(&EqPENV, nullptr);
#endif

    LOG_INFO("Sync is successful.");

    EqNP.s = IPS_OK;

    NewRaDec(currentRA, currentDEC);

    return true;
}

bool ScopeSim::Park()
{
    StartSlew(GetAxis1Park(), GetAxis2Park(), SCOPE_PARKING);
    return true;
}

// common code for GoTo and park
void ScopeSim::StartSlew(double ra, double dec, TelescopeStatus status)
{
    Angle a1, a2;
    alignment.RaDecToMount(Angle(ra * 15.0), Angle(dec), &a1, &a2);

    axisPrimary.StartSlew(a1);
    axisSecondary.StartSlew(a2);

    targetRA  = ra;
    targetDEC = dec;
    char RAStr[64], DecStr[64];

    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);

    const char * statusStr;
    switch (status)
    {
        case SCOPE_PARKING:
            statusStr = "Parking";
            break;
        case SCOPE_SLEWING:
            statusStr = "Slewing";
            break;
        default:
            statusStr = "unknown";
    }
    TrackState = status;

    LOGF_INFO("%s to RA: %s - DEC: %s", statusStr, RAStr, DecStr);
}

bool ScopeSim::UnPark()
{
    SetParked(false);
    return true;
}

bool ScopeSim::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device

    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, "GUIDE_RATE") == 0)
        {
            IUUpdateNumber(&GuideRateNP, values, names, n);
            GuideRateNP.s = IPS_OK;
            IDSetNumber(&GuideRateNP, nullptr);
            return true;
        }

        if (strcmp(name, GuideNSNP.name) == 0 || strcmp(name, GuideWENP.name) == 0)
        {
            processGuiderProperties(name, values, names, n);
            return true;
        }
    }

    //  if we didn't process it, continue up the chain, let somebody else
    //  give it a shot
    return INDI::Telescope::ISNewNumber(dev, name, values, names, n);
}

bool ScopeSim::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, mountTypeSP.name) == 0)
        {
            if (IUUpdateSwitch(&mountTypeSP, states, names, n) < 0)
                return false;

            mountTypeSP.s = IPS_OK;
            int type = IUFindOnSwitchIndex(&mountTypeSP);
            if (type < 0)
                return false;
            alignment.mountType = static_cast<Alignment::MOUNT_TYPE>(type);
            LOGF_DEBUG("ISNewSwitch: Mount Type set to %i", type);
            return true;
        }

        // Slew mode
        if (strcmp(name, SlewRateSP.name) == 0)
        {
            if (IUUpdateSwitch(&SlewRateSP, states, names, n) < 0)
                return false;

            SlewRateSP.s = IPS_OK;
            IDSetSwitch(&SlewRateSP, nullptr);
            return true;
        }

#ifdef USE_EQUATORIAL_PE
        if (strcmp(name, "PE_NS") == 0)
        {
            IUUpdateSwitch(&PEErrNSSP, states, names, n);

            PEErrNSSP.s = IPS_OK;

            if (PEErrNSS[DIRECTION_NORTH].s == ISS_ON)
            {
                EqPEN[DEC_AXIS].value += TRACKRATE_SIDEREAL / 3600.0 * GuideRateN[DEC_AXIS].value;
                LOGF_DEBUG("Simulating PE in NORTH direction for value of %g", TRACKRATE_SIDEREAL / 3600.0);
            }
            else
            {
                EqPEN[DEC_AXIS].value -= TRACKRATE_SIDEREAL / 3600.0 * GuideRateN[DEC_AXIS].value;
                LOGF_DEBUG("Simulating PE in SOUTH direction for value of %g", TRACKRATE_SIDEREAL / 3600.0);
            }

            IUResetSwitch(&PEErrNSSP);
            IDSetSwitch(&PEErrNSSP, nullptr);
            IDSetNumber(&EqPENV, nullptr);

            return true;
        }

        if (strcmp(name, "PE_WE") == 0)
        {
            IUUpdateSwitch(&PEErrWESP, states, names, n);

            PEErrWESP.s = IPS_OK;

            if (PEErrWES[DIRECTION_WEST].s == ISS_ON)
            {
                EqPEN[RA_AXIS].value -= TRACKRATE_SIDEREAL / 3600.0 / 15. * GuideRateN[RA_AXIS].value;
                LOGF_DEBUG("Simulator PE in WEST direction for value of %g", TRACKRATE_SIDEREAL / 3600.0);
            }
            else
            {
                EqPEN[RA_AXIS].value += TRACKRATE_SIDEREAL / 3600.0 / 15. * GuideRateN[RA_AXIS].value;
                LOGF_DEBUG("Simulator PE in EAST direction for value of %g", TRACKRATE_SIDEREAL / 3600.0);
            }

            IUResetSwitch(&PEErrWESP);
            IDSetSwitch(&PEErrWESP, nullptr);
            IDSetNumber(&EqPENV, nullptr);

            return true;
        }
#endif
    }

    //  Nobody has claimed this, so, ignore it
    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}

bool ScopeSim::Abort()
{
    axisPrimary.AbortSlew();
    axisSecondary.AbortSlew();
    return true;
}

bool ScopeSim::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    if (TrackState == SCOPE_PARKED)
    {
        LOG_ERROR("Please unpark the mount before issuing any motion commands.");
        return false;
    }
    mcRate = static_cast<int>(IUFindOnSwitchIndex(&SlewRateSP)) + 1;

    int rate = (dir == INDI_DIR_NS::DIRECTION_NORTH) ? mcRate : -mcRate;
    //LOGF_DEBUG("MoveNS dir %s, motion %s, rate %d", dir == DIRECTION_NORTH ? "N" : "S" , command == 0 ? "start" : "stop", rate);

    axisSecondary.mcRate = command == MOTION_START ? rate : 0;

    return true;
}

bool ScopeSim::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    if (TrackState == SCOPE_PARKED)
    {
        LOG_ERROR("Please unpark the mount before issuing any motion commands.");
        return false;
    }

    mcRate = static_cast<int>(IUFindOnSwitchIndex(&SlewRateSP)) + 1;
    int rate = (dir == INDI_DIR_WE::DIRECTION_EAST) ? mcRate : -mcRate;
    //LOGF_DEBUG("MoveWE dir %d, motion %s, rate %d", dir == DIRECTION_EAST ? "E" : "W", command == 0 ? "start" : "stop", rate);

    axisPrimary.mcRate = command == MOTION_START ? rate : 0;
    return true;
}

IPState ScopeSim::GuideNorth(uint32_t ms)
{
    double rate = GuideRateN[DEC_AXIS].value;
    axisSecondary.StartGuide(rate, ms);
    guidingNS = true;
    return IPS_BUSY;
}

IPState ScopeSim::GuideSouth(uint32_t ms)
{
    double rate = GuideRateN[DEC_AXIS].value;
    axisSecondary.StartGuide(-rate, ms);
    guidingNS = true;
    return IPS_BUSY;
}

IPState ScopeSim::GuideEast(uint32_t ms)
{
    double rate = GuideRateN[RA_AXIS].value;
    axisPrimary.StartGuide(-rate, ms);
    guidingEW = true;
    return IPS_BUSY;
}

IPState ScopeSim::GuideWest(uint32_t ms)
{
    double rate = GuideRateN[RA_AXIS].value;
    axisPrimary.StartGuide(rate, ms);
    guidingEW = true;
    return IPS_BUSY;
}

bool ScopeSim::SetCurrentPark()
{
    SetAxis1Park(currentRA);
    SetAxis2Park(currentDEC);

    return true;
}

bool ScopeSim::SetDefaultPark()
{
    // By default set RA to HA
    SetAxis1Park(get_local_sidereal_time(LocationN[LOCATION_LONGITUDE].value));

    // Set DEC to 90 or -90 depending on the hemisphere
    SetAxis2Park((LocationN[LOCATION_LATITUDE].value > 0) ? 90 : -90);

    return true;
}

bool ScopeSim::SetTrackMode(uint8_t mode)
{
    INDI_UNUSED(mode);
    axisPrimary.TrackRate(Axis::SIDEREAL);
    return true;
}

bool ScopeSim::SetTrackEnabled(bool enabled)
{
    axisPrimary.Tracking(enabled);
    return true;
}

bool ScopeSim::SetTrackRate(double raRate, double deRate)
{
    INDI_UNUSED(raRate);
    INDI_UNUSED(deRate);
    return true;
}

bool ScopeSim::saveConfigItems(FILE *fp)
{
    INDI::Telescope::saveConfigItems(fp);

    IUSaveConfigSwitch(fp, &mountTypeSP);
    IUSaveConfigNumber(fp, &GuideRateNP);

    return true;
}

bool ScopeSim::updateLocation(double latitude, double longitude, double elevation)
{
    LOGF_DEBUG("Update location %8.3f, %8.3f, %4.0f", latitude, longitude, elevation);

    alignment.latitude = Angle(latitude);
    alignment.longitude = Angle(longitude);
    INDI_UNUSED(elevation);
    return true;
 }


/////////////////////////////////////////////////////////////////////

// Angle implementation

Angle::Angle(double value, ANGLE_UNITS type)
{
    switch(type)
    {
        case DEGREES:
            angle = range(value);
            break;
        case HOURS:
            angle = range(value * 15.0);
            break;
        case RADIANS:
            angle = range(value * M_PI);
            break;
    }
}

// Axis Implementation


void Axis::setDegrees(double degrees)
{
    this->position = degrees;
}

void Axis::setHours(double hours)
{
    this->position = hours * 15.0;
}

void Axis::update()         // called about once a second to update the position and mode
{
    struct timeval currentTime { 0, 0 };
    /* update elapsed time since last poll, don't presume exactly POLLMS */
    gettimeofday(&currentTime, nullptr);

    if (lastTime.tv_sec == 0 && lastTime.tv_usec == 0)
        lastTime = currentTime;


    // Time diff in seconds
    double interval  = currentTime.tv_sec - lastTime.tv_sec + (currentTime.tv_usec - lastTime.tv_usec) / 1e6;
    lastTime = currentTime;
    double change = 0;

    //LOGF_DEBUG("axis %s: position %f, target %f, interval %f", axisName, position.Degrees(), target.Degrees(), interval);

    // handle the slew
    if (isSlewing)
    {
        // get positions relative to the rotate centre
        Angle trc = target - rotateCentre;
        Angle prc = position - rotateCentre;
        // get the change, don't use Angle so the change goes through the rotate centre
        double delta = trc.Degrees() - prc.Degrees();
        double fastChange = mcRates[4].Degrees() * interval;
        double slowChange = fastChange / 5;
        //LOGF_DEBUG("slew: trc %f prc %f, delta %f", trc.Degrees(), prc.Degrees(), delta);
        // apply the change to the relative position
        const char * b;
        if (delta < -fastChange)
        {
            change = -fastChange;
            b = "--";
        }
        else if (delta < -slowChange)
        {
            change = -slowChange;
            b = "-";
        }
        else if (delta > fastChange)
        {
            change = fastChange;
            b = "++";
        }
        else if (delta > slowChange)
        {
            change = slowChange;
            b = "+";
        }
        else
        {
            position = target;
            isSlewing = false;
            //OnMoveFinished();
            b = "=";
        }
        position += change;
        //LOGF_DEBUG("move %s: change %f, position %f", b, change, position.Degrees());
    }

    // handle the motion control
    if (mcRate < 0)
    {
        change = -mcRates[-mcRate].Degrees() * interval;
        //LOGF_DEBUG("mcRate %d, rate %f, change %f", mcRate, mcRates[-mcRate].Degrees(), change);
        position += change;
    }
    else if (mcRate > 0)
    {
        change = mcRates[mcRate].Degrees() * interval;
        //LOGF_DEBUG("mcRate %d, rate %f, change %f", mcRate, mcRates[mcRate].Degrees(), change);
        position += change;
    }

    // handle guiding
    if (guideDuration > 0)
    {
        change = guideRateDegSec.Degrees() * (guideDuration > interval ? interval : guideDuration);
        guideDuration -= interval;
        //LOGF_DEBUG("guide rate %f, remaining duration %f, change %f", guideRateDegSec.Degrees(), guideDuration, change);
        position += change;
    }

    // tracking
    if (trackMode != AXIS_TRACK_MODE::OFF)
    {
        position += trackingRateDegSec * interval;
        target += trackingRateDegSec * interval;
        //LOGF_DEBUG("tracking, rate %f, position %f, target %f", trackingRateDegSec.Degrees(), position.Degrees(), target.Degrees());
    }
}

// Alignment methods

Angle Alignment::lst()
{
    return Angle(get_local_sidereal_time(longitude.Degrees360()) * 15.0);
}

void Alignment::MountToHaDec(Angle primary, Angle secondary, Angle * ha, Angle* dec)
{
    switch (mountType)
    {
    case MOUNT_TYPE::ALTAZ:
        break;
    case MOUNT_TYPE::EQ_FORK:
        if (latitude >= 0)
            *dec = Angle(secondary);
        else
            *dec = Angle(-secondary);
        *ha = Angle(primary);
        break;
    case MOUNT_TYPE::EQ_GEM:
        auto d = secondary.Degrees();
        auto h = primary.Degrees();
        if (d > 90 || d < -90)
        {
            // pointing state inverted
            d = 180.0 - d;
            h += 180.0;
        }
        *ha = Angle(h);
        *dec = Angle(d);
        break;
    }
}

void Alignment::MountToRaDec(Angle primary, Angle secondary, Angle * ra, Angle* dec)
{
    MountToHaDec(primary, secondary, ra, dec);
    *ra = lst() - *ra;
}

void Alignment::HaDecToMount(Angle ha, Angle dec, Angle* primary, Angle* secondary)
{
    switch (mountType)
    {
    case MOUNT_TYPE::ALTAZ:
        break;
    case MOUNT_TYPE::EQ_FORK:
        if (latitude >= 0)
            *secondary = Angle(dec);
        else
            *secondary = Angle(-dec);
        *primary = Angle(ha);
        break;
    case MOUNT_TYPE::EQ_GEM:
        *primary = Angle(ha);
        *secondary = Angle(dec);
        if (ha < 0)
        {
            // pointing state inverted
            *primary += Angle(180);
            *secondary = Angle(180) - dec;
        }
        if (latitude < 0)
            *secondary = -*secondary;
        break;
    }
}

void Alignment::RaDecToMount(Angle ra, Angle dec, Angle* primary, Angle* secondary)
{
    HaDecToMount(lst() - ra, dec, primary, secondary);
}




