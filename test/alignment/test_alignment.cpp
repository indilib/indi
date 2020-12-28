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
#include <gmock/gmock.h>

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

    struct ln_equ_posn VegaJ2000
    {
        range360(decimalHoursToDecimalDegrees(18.6156972)),
            rangeDec(38.7856944),
    };

    struct ln_equ_posn ArcturusJ2000
    {
        range360(decimalHoursToDecimalDegrees(14.2612083)),
            rangeDec(19.1872694),
    };

    struct ln_equ_posn MizarJ2000
    {
        range360(decimalHoursToDecimalDegrees(13.3988500)),
            rangeDec(54.9254167),
    };

    s.AddAlignmentEntry(decimalDegreesToDecimalHours(VegaJ2000.ra), VegaJ2000.dec);
    s.AddAlignmentEntry(decimalDegreesToDecimalHours(ArcturusJ2000.ra), ArcturusJ2000.dec);
    s.AddAlignmentEntry(decimalDegreesToDecimalHours(MizarJ2000.ra), MizarJ2000.dec);

    ln_equ_posn pos1 = s.TelescopeEquatorialToSky(decimalDegreesToDecimalHours(VegaJ2000.ra), VegaJ2000.dec);

    // I would expect these to be much closer than one decimal place off
    ASSERT_DOUBLE_EQ(round(pos1.ra, 1), round(VegaJ2000.ra, 1));
    ASSERT_DOUBLE_EQ(round(pos1.dec, 6), round(VegaJ2000.dec, 6));

    sleep(10);
    ln_equ_posn pos2 = s.TelescopeEquatorialToSky(decimalDegreesToDecimalHours(VegaJ2000.ra), VegaJ2000.dec);

    // But after 10 seconds, we are at least stable...
    ASSERT_DOUBLE_EQ(round(pos1.ra, 5), round(pos2.ra, 5));
    ASSERT_DOUBLE_EQ(round(pos1.dec, 5), round(pos2.dec, 5));
}

int main(int argc, char **argv)
{
    INDI::Logger::getInstance().configure("", INDI::Logger::file_off,
                                          INDI::Logger::DBG_ERROR, INDI::Logger::DBG_ERROR);

    ::testing::InitGoogleTest(&argc, argv);
    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
