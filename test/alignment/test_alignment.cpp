/*******************************************************************************
 Copyright(c) 2020 Rick Bassham. All rights reserved.
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

#include <gtest/gtest.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstdlib>
#include <cstring>
#include <stdio.h>

#include <indilogger.h>

#include "alignment_scope.h"

double round(double value, int decimal_places)
{
    const double multiplier = std::pow(10.0, decimal_places);
    return std::round(value * multiplier) / multiplier;
}

TEST(ALIGNMENT_TEST, Test_TDVRoundTripEquatorial)
{
    Scope s(INDI::AlignmentSubsystem::MathPluginManagement::EQUATORIAL);

    s.Handshake();
    s.SetAlignmentSubsystemActive(true);

    // Vega
    double ra = 18.6156, dec = 38.78361;

    INDI::IEquatorialCoordinates RaDec {ra, rangeDec(dec)};
    TelescopeDirectionVector TDV = s.TelescopeDirectionVectorFromEquatorialCoordinates(RaDec);
    INDI::IEquatorialCoordinates RaDecResult;
    s.EquatorialCoordinatesFromTelescopeDirectionVector(TDV, RaDecResult);

    RaDecResult.rightascension = range24(RaDecResult.rightascension);
    RaDecResult.declination = rangeDec(RaDecResult.declination);

    ASSERT_DOUBLE_EQ(RaDec.rightascension, RaDecResult.rightascension);
    ASSERT_DOUBLE_EQ(RaDec.declination, RaDecResult.declination);
}

TEST(ALIGNMENT_TEST, Test_TDVRoundTripAltAz)
{
    Scope s(INDI::AlignmentSubsystem::MathPluginManagement::ALTAZ);

    s.Handshake();
    s.SetAlignmentSubsystemActive(true);

    double alt = 35.7, az = 80.0;
    INDI::IHorizontalCoordinates AltAz;
    AltAz.altitude = range360(alt);
    AltAz.azimuth = range360(az);
    TelescopeDirectionVector TDV = s.TelescopeDirectionVectorFromAltitudeAzimuth(AltAz);
    INDI::IHorizontalCoordinates AltAzResult;
    s.AltitudeAzimuthFromTelescopeDirectionVector(TDV, AltAzResult);

    AltAzResult.altitude = range360(AltAzResult.altitude);
    AltAzResult.azimuth = range360(AltAzResult.azimuth);

    ASSERT_DOUBLE_EQ(AltAz.altitude, AltAzResult.altitude);
    ASSERT_DOUBLE_EQ(AltAz.azimuth, AltAzResult.azimuth);
}

TEST(ALIGNMENT_TEST, Test_ThreeSyncPointsEquatorial)
{
    Scope s(INDI::AlignmentSubsystem::MathPluginManagement::EQUATORIAL);

    s.Handshake();
    ASSERT_TRUE(s.updateLocation(34.70, 279.46, 161));
    s.SetAlignmentSubsystemActive(true);

    double VegaJ2000RA = 18.6156972;
    double VegaJ2000Dec = 38.7856944;

    double ArcturusJ2000RA = 14.2612083;
    double ArcturusJ2000Dec = 19.1872694;

    double MizarJ2000RA = 13.3988500;
    double MizarJ2000Dec = 54.9254167;

    // The test scope will do a "perfect" sync with whatever we send it.
    s.Sync(VegaJ2000RA, VegaJ2000Dec);
    s.Sync(ArcturusJ2000RA, ArcturusJ2000Dec);
    s.Sync(MizarJ2000RA, MizarJ2000Dec);

    double VegaSkyRA, VegaSkyDec;
    s.TelescopeEquatorialToSky(VegaJ2000RA, VegaJ2000Dec, VegaSkyRA, VegaSkyDec);

    // I would expect these to be closer than 1 decimal apart, but it seems to work
    ASSERT_DOUBLE_EQ(round(VegaJ2000RA, 1), round(VegaSkyRA, 1));
    ASSERT_DOUBLE_EQ(round(VegaJ2000Dec, 6), round(VegaSkyDec, 6));

    double VegaMountRA, VegaMountDec;
    s.SkyToTelescopeEquatorial(VegaSkyRA, VegaSkyDec, VegaMountRA, VegaMountDec);
    ASSERT_DOUBLE_EQ(round(VegaJ2000RA, 1), round(VegaMountRA, 1));
    ASSERT_DOUBLE_EQ(round(VegaJ2000Dec, 6), round(VegaMountDec, 6));
}

TEST(ALIGNMENT_TEST, Test_ThreeSyncPointsAltAz)
{
    Scope s(INDI::AlignmentSubsystem::MathPluginManagement::ALTAZ);

    s.Handshake();
    ASSERT_TRUE(s.updateLocation(34.70, 279.46, 161));
    s.SetAlignmentSubsystemActive(true);

    double VegaJ2000RA = 18.6156972;
    double VegaJ2000Dec = 38.7856944;

    double ArcturusJ2000RA = 14.2612083;
    double ArcturusJ2000Dec = 19.1872694;

    double MizarJ2000RA = 13.3988500;
    double MizarJ2000Dec = 54.9254167;

    // The test scope will do a "perfect" sync with whatever we send it.
    ASSERT_TRUE(s.Sync(VegaJ2000RA, VegaJ2000Dec));
    ASSERT_TRUE(s.Sync(ArcturusJ2000RA, ArcturusJ2000Dec));
    ASSERT_TRUE(s.Sync(MizarJ2000RA, MizarJ2000Dec));

    double testPointAlt = 35.123456, testPointAz = 80.123456;
    double skyRA, skyDec;
    ASSERT_TRUE(s.TelescopeAltAzToSky(testPointAlt, testPointAz, skyRA, skyDec));

    // convert the ra/dec we received to alt/az to compare
    INDI::IGeographicCoordinates location;
    s.GetDatabaseReferencePosition(location);
    INDI::IEquatorialCoordinates raDec {skyRA, skyDec};
    INDI::IHorizontalCoordinates altAz;
    INDI::EquatorialToHorizontal(&raDec, &location, ln_get_julian_from_sys(), &altAz);
    // I would expect these to be closer than 1 decimal apart, but it seems to work
    ASSERT_DOUBLE_EQ(round(testPointAlt, 1), round(altAz.altitude, 1));
    ASSERT_DOUBLE_EQ(round(testPointAz, 1), round(altAz.azimuth, 1));

    double roundTripAlt, roundTripAz;
    s.SkyToTelescopeAltAz(skyRA, skyDec, roundTripAlt, roundTripAz);

    ASSERT_DOUBLE_EQ(round(testPointAlt, 1), round(roundTripAlt, 1));
    ASSERT_DOUBLE_EQ(round(testPointAz, 1), round(roundTripAz, 1));
}

int main(int argc, char **argv)
{
    INDI::Logger::getInstance().configure("", INDI::Logger::file_off,
                                          INDI::Logger::DBG_ERROR, INDI::Logger::DBG_ERROR);

    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
