#ifndef INDI_ALIGNMENTSUBSYSTEM_DUMMYMATHPLUGIN_H
#define INDI_ALIGNMENTSUBSYSTEM_DUMMYMATHPLUGIN_H

#include "AlignmentSubsystemForMathPlugins.h"
#include "ConvexHull.h"

namespace INDI {
namespace AlignmentSubsystem {

class DummyMathPlugin : public AlignmentSubsystemForMathPlugins
{
    public:
        DummyMathPlugin();
        virtual ~DummyMathPlugin();

    virtual bool Initialise(InMemoryDatabase* pInMemoryDatabase);

    virtual bool TransformCelestialToTelescope(const double RightAscension, const double Declination, double JulianOffset,
                                                TelescopeDirectionVector& ApparentTelescopeDirectionVector);

    virtual bool TransformTelescopeToCelestial(const TelescopeDirectionVector& ApparentTelescopeDirectionVector, double& RightAscension, double& Declination);
};

} // namespace AlignmentSubsystem
} // namespace INDI

#endif // INDI_ALIGNMENTSUBSYSTEM_DUMMYMATHPLUGIN_H
