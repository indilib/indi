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

#include <indilogger.h>

#include "alignment_scope.h"

double round(double value, int decimal_places)
{
    const double multiplier = std::pow(10.0, decimal_places);
    return std::round(value * multiplier) / multiplier;
}

TEST(ALIGNMENT_TEST, Test_TDVRoundTrip)
{
    Scope s;

    s.Handshake();
    s.SetAlignmentSubsystemActive(true);

    // Vega
    double ra = 18.6156, dec = 38.78361;

    struct ln_equ_posn RaDec;
    RaDec.ra = range360((ra * 360.0) / 24.0); // Convert decimal hours to decimal degrees
    RaDec.dec = rangeDec(dec);

    TelescopeDirectionVector TDV = s.TelescopeDirectionVectorFromEquatorialCoordinates(RaDec);

    struct ln_equ_posn RaDecResult;
    s.EquatorialCoordinatesFromTelescopeDirectionVector(TDV, RaDecResult);

    RaDecResult.ra = range360(RaDecResult.ra);
    RaDecResult.dec = rangeDec(RaDecResult.dec);

    ASSERT_DOUBLE_EQ(RaDec.ra, RaDecResult.ra);
    ASSERT_DOUBLE_EQ(RaDec.dec, RaDecResult.dec);
}

TEST(ALIGNMENT_TEST, Test_ThreeSyncPoints)
{
    Scope s;

    s.Handshake();
    s.updateLocation(34.70, -80.54, 161);
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

int main(int argc, char **argv)
{
    INDI::Logger::getInstance().configure("", INDI::Logger::file_off,
                                          INDI::Logger::DBG_ERROR, INDI::Logger::DBG_ERROR);

    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
