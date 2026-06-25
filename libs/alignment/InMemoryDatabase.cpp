/*!
 * \file InMemoryDatabase.cpp
 *
 * \author Roger James
 * \date 13th November 2013
 *
 */

#include "InMemoryDatabase.h"

#include "basedevice.h"
#include "indicom.h"
#include "../indijson.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

namespace INDI
{
namespace AlignmentSubsystem
{
InMemoryDatabase::InMemoryDatabase() : DatabaseReferencePositionIsValid(false),
    LoadDatabaseCallback(nullptr), LoadDatabaseCallbackThisPointer(nullptr)
{
}

bool InMemoryDatabase::CheckForDuplicateSyncPoint(const AlignmentDatabaseEntry &CandidateEntry,
        double Tolerance) const
{
    return std::any_of(MySyncPoints.begin(), MySyncPoints.end(), [&CandidateEntry, Tolerance](const auto & point)
    {
        return (((std::abs(point.RightAscension - CandidateEntry.RightAscension) < 24.0 * Tolerance / 100.0) &&
                 (std::abs(point.Declination - CandidateEntry.Declination) < 180.0 * Tolerance / 100.0)) ||
                ((std::abs(point.TelescopeDirection.x - CandidateEntry.TelescopeDirection.x) < Tolerance / 100.0) &&
                 (std::abs(point.TelescopeDirection.y - CandidateEntry.TelescopeDirection.y) < Tolerance / 100.0) &&
                 (std::abs(point.TelescopeDirection.z - CandidateEntry.TelescopeDirection.z) < Tolerance / 100.0)));
    });
}

void InMemoryDatabase::RemoveSyncPoint(const AlignmentDatabaseEntry &CandidateEntry,
                                       double Tolerance)
{
    MySyncPoints.erase(std::remove_if(MySyncPoints.begin(), MySyncPoints.end(),
                                      [&CandidateEntry, Tolerance](const auto & point)
    {
        return (((std::abs(point.RightAscension - CandidateEntry.RightAscension) < 24.0 * Tolerance / 100.0) &&
                 (std::abs(point.Declination - CandidateEntry.Declination) < 180.0 * Tolerance / 100.0)) ||
                ((std::abs(point.TelescopeDirection.x - CandidateEntry.TelescopeDirection.x) < Tolerance / 100.0) &&
                 (std::abs(point.TelescopeDirection.y - CandidateEntry.TelescopeDirection.y) < Tolerance / 100.0) &&
                 (std::abs(point.TelescopeDirection.z - CandidateEntry.TelescopeDirection.z) < Tolerance / 100.0)));
    }),
    MySyncPoints.end());
}


bool InMemoryDatabase::GetDatabaseReferencePosition(IGeographicCoordinates &Position)
{
    if (DatabaseReferencePositionIsValid)
    {
        Position = DatabaseReferencePosition;
        return true;
    }
    else
        return false;
}

bool InMemoryDatabase::LoadDatabase(const char *DeviceName)
{
    char DatabaseFileName[MAXRBUF];
    char Errmsg[MAXRBUF];
    XMLEle *FileRoot    = nullptr;
    XMLEle *EntriesRoot = nullptr;
    XMLEle *EntryRoot   = nullptr;
    XMLEle *Element     = nullptr;
    XMLAtt *Attribute   = nullptr;
    LilXML *Parser      = newLilXML();

    FILE *fp = nullptr;

    snprintf(DatabaseFileName, MAXRBUF, "%s/.indi/%s_alignment_database.xml", getenv("HOME"), DeviceName);

    fp = fopen(DatabaseFileName, "r");
    if (fp == nullptr)
    {
        snprintf(Errmsg, MAXRBUF, "Unable to read alignment database file. Error loading file %s: %s\n",
                 DatabaseFileName, strerror(errno));
        return false;
    }

    char whynot[MAXRBUF];
    if (nullptr == (FileRoot = readXMLFile(fp, Parser, whynot)))
    {
        snprintf(Errmsg, MAXRBUF, "Unable to parse database XML: %s", whynot);
        return false;
    }

    if (strcmp(tagXMLEle(FileRoot), "INDIAlignmentDatabase") != 0)
    {
        return false;
    }

    if (nullptr == (EntriesRoot = findXMLEle(FileRoot, "DatabaseEntries")))
    {
        snprintf(Errmsg, MAXRBUF, "Cannot find DatabaseEntries element");
        return false;
    }

    if (nullptr != (Element = findXMLEle(FileRoot, "DatabaseReferenceLocation")))
    {
        if (nullptr == (Attribute = findXMLAtt(Element, "latitude")))
        {
            snprintf(Errmsg, MAXRBUF, "Cannot find latitude attribute");
            return false;
        }
        sscanf(valuXMLAtt(Attribute), "%lf", &DatabaseReferencePosition.latitude);
        if (nullptr == (Attribute = findXMLAtt(Element, "longitude")))
        {
            snprintf(Errmsg, MAXRBUF, "Cannot find longitude attribute");
            return false;
        }
        sscanf(valuXMLAtt(Attribute), "%lf", &DatabaseReferencePosition.longitude);
        DatabaseReferencePositionIsValid = true;
    }

    MySyncPoints.clear();

    for (EntryRoot = nextXMLEle(EntriesRoot, 1); EntryRoot != nullptr; EntryRoot = nextXMLEle(EntriesRoot, 0))
    {
        AlignmentDatabaseEntry CurrentValues;
        if (strcmp(tagXMLEle(EntryRoot), "DatabaseEntry") != 0)
        {
            return false;
        }
        for (Element = nextXMLEle(EntryRoot, 1); Element != nullptr; Element = nextXMLEle(EntryRoot, 0))
        {
            if (strcmp(tagXMLEle(Element), "ObservationJulianDate") == 0)
            {
                sscanf(pcdataXMLEle(Element), "%lf", &CurrentValues.ObservationJulianDate);
            }
            else if (strcmp(tagXMLEle(Element), "RightAscension") == 0)
            {
                f_scansexa(pcdataXMLEle(Element), &CurrentValues.RightAscension);
            }
            else if (strcmp(tagXMLEle(Element), "Declination") == 0)
            {
                f_scansexa(pcdataXMLEle(Element), &CurrentValues.Declination);
            }
            else if (strcmp(tagXMLEle(Element), "TelescopeDirectionVectorX") == 0)
            {
                sscanf(pcdataXMLEle(Element), "%lf", &CurrentValues.TelescopeDirection.x);
            }
            else if (strcmp(tagXMLEle(Element), "TelescopeDirectionVectorY") == 0)
            {
                sscanf(pcdataXMLEle(Element), "%lf", &CurrentValues.TelescopeDirection.y);
            }
            else if (strcmp(tagXMLEle(Element), "TelescopeDirectionVectorZ") == 0)
            {
                sscanf(pcdataXMLEle(Element), "%lf", &CurrentValues.TelescopeDirection.z);
            }
            else
                return false;
        }
        MySyncPoints.push_back(CurrentValues);
    }

    fclose(fp);
    delXMLEle(FileRoot);
    delLilXML(Parser);

    if (nullptr != LoadDatabaseCallback)
        (*LoadDatabaseCallback)(LoadDatabaseCallbackThisPointer);

    return true;
}

bool InMemoryDatabase::SaveDatabase(const char *DeviceName)
{
    char ConfigDir[MAXRBUF];
    char DatabaseFileName[MAXRBUF];
    char Errmsg[MAXRBUF];
    struct stat Status;
    FILE *fp;

    snprintf(ConfigDir, MAXRBUF, "%s/.indi/", getenv("HOME"));
    snprintf(DatabaseFileName, MAXRBUF, "%s%s_alignment_database.xml", ConfigDir, DeviceName);

    if (stat(ConfigDir, &Status) != 0)
    {
        if (mkdir(ConfigDir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) < 0)
        {
            snprintf(Errmsg, MAXRBUF, "Unable to create config directory. Error %s: %s\n", ConfigDir, strerror(errno));
            return false;
        }
    }

    fp = fopen(DatabaseFileName, "w");
    if (fp == nullptr)
    {
        snprintf(Errmsg, MAXRBUF, "Unable to open database file. Error opening file %s: %s\n", DatabaseFileName,
                 strerror(errno));
        return false;
    }

    fprintf(fp, "<INDIAlignmentDatabase>\n");

    if (DatabaseReferencePositionIsValid)
        fprintf(fp, "   <DatabaseReferenceLocation latitude='%lf' longitude='%lf'/>\n", DatabaseReferencePosition.latitude,
                DatabaseReferencePosition.longitude);

    fprintf(fp, "   <DatabaseEntries>\n");
    for (AlignmentDatabaseType::const_iterator Itr = MySyncPoints.begin(); Itr != MySyncPoints.end(); Itr++)
    {
        char SexaString[12]; // Long enough to hold xx:xx:xx.xx
        fprintf(fp, "      <DatabaseEntry>\n");

        fprintf(fp, "         <ObservationJulianDate>%lf</ObservationJulianDate>\n", (*Itr).ObservationJulianDate);
        fs_sexa(SexaString, (*Itr).RightAscension, 2, 3600);
        fprintf(fp, "         <RightAscension>%s</RightAscension>\n", SexaString);
        fs_sexa(SexaString, (*Itr).Declination, 2, 3600);
        fprintf(fp, "         <Declination>%s</Declination>\n", SexaString);
        fprintf(fp, "         <TelescopeDirectionVectorX>%lf</TelescopeDirectionVectorX>\n",
                (*Itr).TelescopeDirection.x);
        fprintf(fp, "         <TelescopeDirectionVectorY>%lf</TelescopeDirectionVectorY>\n",
                (*Itr).TelescopeDirection.y);
        fprintf(fp, "         <TelescopeDirectionVectorZ>%lf</TelescopeDirectionVectorZ>\n",
                (*Itr).TelescopeDirection.z);

        fprintf(fp, "      </DatabaseEntry>\n");
    }
    fprintf(fp, "   </DatabaseEntries>\n");

    fprintf(fp, "</INDIAlignmentDatabase>\n");

    fclose(fp);

    return true;
}

void InMemoryDatabase::SetDatabaseReferencePosition(double Latitude, double Longitude)
{
    DatabaseReferencePosition.latitude    = Latitude;
    DatabaseReferencePosition.longitude    = Longitude;
    DatabaseReferencePosition.elevation    = 0;
    DatabaseReferencePositionIsValid = true;
}

void InMemoryDatabase::SetDatabaseReferencePosition(const IGeographicCoordinates &Position)
{
    DatabaseReferencePosition        = Position;
    DatabaseReferencePositionIsValid = true;
}

void InMemoryDatabase::SetLoadDatabaseCallback(LoadDatabaseCallbackPointer_t CallbackPointer, void *ThisPointer)
{
    LoadDatabaseCallback            = CallbackPointer;
    LoadDatabaseCallbackThisPointer = ThisPointer;
}

// Helper: map MountAlignment_t enum to JSON string
static const char *MountAlignmentToString(MountAlignment_t alignment)
{
    switch (alignment)
    {
        case ZENITH:
            return "ZENITH";
        case NORTH_CELESTIAL_POLE:
            return "NORTH_CELESTIAL_POLE";
        case SOUTH_CELESTIAL_POLE:
            return "SOUTH_CELESTIAL_POLE";
        default:
            return "ZENITH";
    }
}

std::string InMemoryDatabase::SerializeModelToJSON(MountAlignment_t MountAlignment) const
{
    using json = nlohmann::json;

    json model;
    model["version"] = 1;

    model["referencePosition"] =
    {
        {"latitude",  DatabaseReferencePosition.latitude},
        {"longitude", DatabaseReferencePosition.longitude}
    };

    model["mountAlignment"] = MountAlignmentToString(MountAlignment);

    json points = json::array();
    for (const auto &entry : MySyncPoints)
    {
        json point;
        point["julianDate"] = entry.ObservationJulianDate;
        point["ra"]         = entry.RightAscension;
        point["dec"]        = entry.Declination;
        point["vector"] =
        {
            {"x", entry.TelescopeDirection.x},
            {"y", entry.TelescopeDirection.y},
            {"z", entry.TelescopeDirection.z}
        };
        points.push_back(point);
    }
    model["points"] = points;

    return model.dump();
}

bool InMemoryDatabase::DeserializeModelFromJSON(const std::string &JSONData)
{
    using json = nlohmann::json;

    json model;
    try
    {
        model = json::parse(JSONData);
    }
    catch (const json::parse_error &)
    {
        return false;
    }

    // Version check
    if (!model.contains("version") || model["version"].get<int>() != 1)
        return false;

    // Restore reference position
    if (model.contains("referencePosition"))
    {
        auto &ref = model["referencePosition"];
        if (ref.contains("latitude") && ref.contains("longitude"))
        {
            DatabaseReferencePosition.latitude  = ref["latitude"].get<double>();
            DatabaseReferencePosition.longitude = ref["longitude"].get<double>();
            DatabaseReferencePosition.elevation = 0;
            DatabaseReferencePositionIsValid    = true;
        }
    }

    // Clear and repopulate the database
    MySyncPoints.clear();

    if (model.contains("points") && model["points"].is_array())
    {
        for (const auto &jp : model["points"])
        {
            AlignmentDatabaseEntry entry;
            entry.ObservationJulianDate = jp.value("julianDate", 0.0);
            entry.RightAscension        = jp.value("ra",  0.0);
            entry.Declination           = jp.value("dec", 0.0);

            if (jp.contains("vector"))
            {
                auto &v = jp["vector"];
                entry.TelescopeDirection.x = v.value("x", 0.0);
                entry.TelescopeDirection.y = v.value("y", 0.0);
                entry.TelescopeDirection.z = v.value("z", 0.0);
            }

            entry.PrivateDataSize = 0;
            MySyncPoints.push_back(entry);
        }
    }

    return true;
}

} // namespace AlignmentSubsystem
} // namespace INDI