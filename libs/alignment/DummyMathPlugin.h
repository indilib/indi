
#pragma once

#include "AlignmentSubsystemForMathPlugins.h"
#include "ConvexHull.h"

namespace INDI
{
namespace AlignmentSubsystem
{
class DummyMathPlugin : public AlignmentSubsystemForMathPlugins
{
    public:
        DummyMathPlugin();
        virtual ~DummyMathPlugin();

        virtual bool Initialise(InMemoryDatabase *pInMemoryDatabase) override;

        virtual bool TransformCelestialToTelescopeJD(double RightAscension, double Declination,
                double JulianDate,
                TelescopeDirectionVector &ApparentTelescopeDirectionVector) override;

        virtual bool TransformTelescopeToCelestialJD(const TelescopeDirectionVector &ApparentTelescopeDirectionVector,
                double &RightAscension, double &Declination, double JulianDate) override;

        virtual bool TransformCelestialToTelescope(const double RightAscension, const double Declination,
                double JulianOffset,
                TelescopeDirectionVector &ApparentTelescopeDirectionVector) override;

        virtual bool TransformTelescopeToCelestial(const TelescopeDirectionVector &ApparentTelescopeDirectionVector,
                double &RightAscension, double &Declination, double JulianOffset = 0) override;
};

} // namespace AlignmentSubsystem
} // namespace INDI
