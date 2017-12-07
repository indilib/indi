/*!
 * \file MathPlugin.cpp
 *
 * \author Roger James
 * \date 13th November 2013
 *
 */

#include "MathPlugin.h"
#include <cstdio>

namespace INDI
{
namespace AlignmentSubsystem
{
bool MathPlugin::Initialise(InMemoryDatabase *pInMemoryDatabase)
{
    MathPlugin::pInMemoryDatabase = pInMemoryDatabase;
    return true;
}

std::string MathPlugin::GetInternalDataRepresentation(std::string PluginDisplayName)
{
    return "<AlignmentSubsystemData>\n<MathPlugin>"+PluginDisplayName+"</MathPlugin>\n"+GetDatabaseRepresentation()+"\n</AlignmentSubsystemData>";
}

std::string MathPlugin::GetDatabaseRepresentation()
{
    std::string repr="<INDIAlignmentDatabase>\n";
    ln_lnlat_posn DatabaseReferencePosition;

    
    if (pInMemoryDatabase->GetDatabaseReferencePosition(DatabaseReferencePosition))
    {
        char lat[20], lng[20];

	std::snprintf(lat, 20, "%lf", DatabaseReferencePosition.lat);
	std::snprintf(lng, 20, "%lf", DatabaseReferencePosition.lng);
	std::string strlat=lat;
	std::string strlng=lng;
        repr += "   <DatabaseReferenceLocation latitude='" + strlat + "' longitude='" + strlng + "'/>\n";

	repr += "   <DatabaseEntries>\n";
	InMemoryDatabase::AlignmentDatabaseType &SyncPoints = pInMemoryDatabase->GetAlignmentDatabase();
        for (InMemoryDatabase::AlignmentDatabaseType::const_iterator Itr = SyncPoints.begin(); Itr != SyncPoints.end(); Itr++)
        {
	    char buf[32];
	    std::string strbuf;
            repr += "      <DatabaseEntry>\n";
	    std::snprintf(buf, 32, "%lf", (*Itr).ObservationJulianDate);
	    strbuf = buf;
            repr += "         <ObservationJulianDate>" + strbuf + "</ObservationJulianDate>\n";
	    std::snprintf(buf, 32, "%lf", (*Itr).RightAscension);
	    strbuf = buf;
            repr += "         <RightAscension>" + strbuf + "</RightAscension>\n";
	    std::snprintf(buf, 32, "%lf", (*Itr).Declination);
	    strbuf = buf;	    
	    repr += "         <Declination>"+ strbuf + "</Declination>\n";
	    std::snprintf(buf, 32, "%lf", (*Itr).TelescopeDirection.x);
	    strbuf = buf;		    
	    repr += "         <TelescopeDirectionVectorX>" + strbuf + "</TelescopeDirectionVectorX>\n";
	    std::snprintf(buf, 32, "%lf", (*Itr).TelescopeDirection.y);
	    strbuf = buf;		    
	    repr += "         <TelescopeDirectionVectorY>" + strbuf + "</TelescopeDirectionVectorY>\n";
	    std::snprintf(buf, 32, "%lf", (*Itr).TelescopeDirection.z);
	    strbuf = buf;		    
	    repr += "         <TelescopeDirectionVectorZ>" + strbuf + "</TelescopeDirectionVectorZ>\n";
	    repr += "      </DatabaseEntry>\n";
	}
	repr += "   </DatabaseEntries>\n";
    }
    repr += "</INDIAlignmentDatabase>";
    return repr;
}
  
} // namespace AlignmentSubsystem
} // namespace INDI
