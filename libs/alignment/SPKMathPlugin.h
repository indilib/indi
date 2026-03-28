/*******************************************************************************
 Copyright(c) 2026 Christian Kemper. All rights reserved.

 SPK Math Plugin for INDI Alignment Subsystem.

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

#pragma once

#include "MathPlugin.h"
#include "spk/spk.h"
#include "TelescopeDirectionVectorSupportFunctions.h"
#include "libastro.h"

namespace INDI
{
namespace AlignmentSubsystem
{

class SPKMathPlugin : public MathPlugin, public TelescopeDirectionVectorSupportFunctions
{
    public:
        SPKMathPlugin();
        virtual ~SPKMathPlugin();

        virtual bool Initialise(InMemoryDatabase *pInMemoryDatabase) override;

        virtual bool TransformCelestialToTelescope(const double RightAscension, const double Declination,
                double JulianOffset,
                TelescopeDirectionVector &ApparentTelescopeDirectionVector) override;

        virtual bool TransformTelescopeToCelestial(const TelescopeDirectionVector &ApparentTelescopeDirectionVector,
                double &RightAscension, double &Declination, double JulianOffset = 0) override;

    private:
        spkOBS m_Obs;
        spkPM m_PM;
        spkAST m_Ast;
        spkOPT m_Opt;
        spkPO m_PO;

        void UpdateObsConfig();
        void UpdateAstrometry(double JulianOffset);

        std::vector<double> BuildObservationData(const InMemoryDatabase::AlignmentDatabaseType &syncPoints, int &outTermCount);
        void ParsePmfitCoefficients(const double pmv[6], int terms);
        TelescopeDirectionVector RollPitchToDirectionVector(double roll, double pitch);
        void DirectionVectorToRollPitch(const TelescopeDirectionVector &v, double &roll, double &pitch);
};

} // namespace AlignmentSubsystem
} // namespace INDI
