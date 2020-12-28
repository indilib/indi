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

double decimalHoursToDecimalDegrees(double decimalHours)
{
    return (decimalHours * 360.0) / 24.0;
}

double decimalDegreesToDecimalHours(double decimalDegrees)
{
    return (decimalDegrees * 24.0) / 360.0;
}

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

// adds a "perfect" alignment entry, where the telescope RA/Dec matches the actual RA/Dec.
void addAlignmentEntry(AlignmentSubsystemForDrivers *pTelescope, ln_equ_posn RaDec)
{
    AlignmentDatabaseEntry NewEntry;
    TelescopeDirectionVector TDV = pTelescope->TelescopeDirectionVectorFromEquatorialCoordinates(RaDec);

    NewEntry.ObservationJulianDate = ln_get_julian_from_sys();
    NewEntry.RightAscension = decimalDegreesToDecimalHours(RaDec.ra);
    NewEntry.Declination = RaDec.dec;
    NewEntry.TelescopeDirection = TDV;
    NewEntry.PrivateDataSize = 0;

    if (!pTelescope->CheckForDuplicateSyncPoint(NewEntry))
    {
        pTelescope->GetAlignmentDatabase().push_back(NewEntry);
        pTelescope->UpdateSize();
        pTelescope->Initialise(pTelescope);
    }
}

void checkAlignment(AlignmentSubsystemForDrivers *pTelescope, ln_equ_posn RaDec, int decimal_places = 3)
{
    TelescopeDirectionVector TDV = pTelescope->TelescopeDirectionVectorFromEquatorialCoordinates(RaDec);
    double RightAscension, Declination;
    bool result = pTelescope->TransformTelescopeToCelestial(TDV, RightAscension, Declination);
    ASSERT_TRUE(result);
    ASSERT_DOUBLE_EQ(round(Declination, decimal_places), round(RaDec.dec, decimal_places));
    ASSERT_DOUBLE_EQ(round(RightAscension, decimal_places), round(decimalDegreesToDecimalHours(RaDec.ra), decimal_places));
}

TEST(ALIGNMENT_TEST, Test_ThreeSyncPoints)
{
    Scope s;

    s.Handshake();
    s.UpdateLocation(34.70, -80.54, 161);
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

    addAlignmentEntry(&s, VegaJ2000);
    addAlignmentEntry(&s, ArcturusJ2000);
    addAlignmentEntry(&s, MizarJ2000);

    checkAlignment(&s, VegaJ2000, 2);
    checkAlignment(&s, ArcturusJ2000, 2);
    checkAlignment(&s, MizarJ2000, 2);
}

int main(int argc, char **argv)
{
    INDI::Logger::getInstance().configure("", INDI::Logger::file_off,
                                          INDI::Logger::DBG_ERROR, INDI::Logger::DBG_ERROR);

    ::testing::InitGoogleTest(&argc, argv);
    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
