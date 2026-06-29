/*!
 * \file MapPropertiesToInMemoryDatabase.cpp
 *
 * \author Roger James
 * \date 13th November 2013
 *
 */

#include "MapPropertiesToInMemoryDatabase.h"

#include <cfloat>

namespace INDI
{
namespace AlignmentSubsystem
{

// BLOB format identifier for the model JSON
#define ALIGNMENT_MODEL_FORMAT ".json"

void MapPropertiesToInMemoryDatabase::initProperties(Telescope *pTelescope)
{
    // ==================== NEW properties (simplified, cursor-free) ====================

    // ALIGNMENT_MODEL_BLOB — JSON BLOB containing the complete alignment model
    AlignmentModelBlob[0].fill("ALIGNMENT_MODEL_BLOB", "Alignment Model",
                               ALIGNMENT_MODEL_FORMAT);
    AlignmentModelBlob.fill(pTelescope->getDeviceName(), "ALIGNMENT_MODEL_BLOB",
                            "Alignment Model", ALIGNMENT_TAB, IP_RO, 0, IPS_IDLE);

    // ALIGNMENT_MODEL_COUNT — sync point count (read-only)
    AlignmentModelCountNP[0].fill("ALIGNMENT_MODEL_COUNT", "Count",
                                  "%g", 0, 100000, 0, 0);
    AlignmentModelCountNP.fill(pTelescope->getDeviceName(), "ALIGNMENT_MODEL_COUNT",
                               "Sync Points", ALIGNMENT_TAB, IP_RO, 0, IPS_IDLE);

    // ALIGN_POINT_NEW — append a sync point (write-only)
    AlignPointNewNP[NEW_JD].fill("ALIGN_POINT_JD", "Julian Date", "%g", 0, 60000, 0, 0);
    AlignPointNewNP[NEW_RA].fill("ALIGN_POINT_RA", "RA (hours)", "%010.6m", 0, 24, 0, 0);
    AlignPointNewNP[NEW_DEC].fill("ALIGN_POINT_DEC", "DEC (degrees)", "%010.6m", -90, 90, 0, 0);
    AlignPointNewNP[NEW_VX].fill("ALIGN_POINT_VECTOR_X", "TDV X", "%g", -FLT_MAX, FLT_MAX, 0, 0);
    AlignPointNewNP[NEW_VY].fill("ALIGN_POINT_VECTOR_Y", "TDV Y", "%g", -FLT_MAX, FLT_MAX, 0, 0);
    AlignPointNewNP[NEW_VZ].fill("ALIGN_POINT_VECTOR_Z", "TDV Z", "%g", -FLT_MAX, FLT_MAX, 0, 0);
    AlignPointNewNP.fill(pTelescope->getDeviceName(), "ALIGN_POINT_NEW",
                         "Add Sync Point", ALIGNMENT_TAB, IP_WO, 0, IPS_IDLE);

    // ALIGN_POINT_DELETE_INDEX — delete a sync point at 0-based index
    AlignPointDeleteIndexNP[0].fill("ALIGN_POINT_INDEX", "Index", "%g", 0, 100000, 0, -1);
    AlignPointDeleteIndexNP.fill(pTelescope->getDeviceName(), "ALIGN_POINT_DELETE_INDEX",
                                 "Delete Sync Point", ALIGNMENT_TAB, IP_WO, 0, IPS_IDLE);

    // ALIGNMENT_CLEAR — clear all sync points
    AlignmentClearSP[CLEAR_S].fill("CLEAR", "Clear All Points", ISS_OFF);
    AlignmentClearSP.fill(pTelescope->getDeviceName(), "ALIGNMENT_CLEAR",
                          "Clear Sync Points", ALIGNMENT_TAB, IP_WO, ISR_ATMOST1, 0, IPS_IDLE);

    // ALIGNMENT_LOAD — load database from persistent storage
    AlignmentLoadSP[LOAD_S].fill("LOAD", "Load Sync Points", ISS_OFF);
    AlignmentLoadSP.fill(pTelescope->getDeviceName(), "ALIGNMENT_LOAD",
                         "Load Sync Points", ALIGNMENT_TAB, IP_WO, ISR_ATMOST1, 0, IPS_IDLE);

    // ALIGNMENT_SAVE — save database to persistent storage
    AlignmentSaveSP[SAVE_S].fill("SAVE", "Save Sync Points", ISS_OFF);
    AlignmentSaveSP.fill(pTelescope->getDeviceName(), "ALIGNMENT_SAVE",
                         "Save Sync Points", ALIGNMENT_TAB, IP_WO, ISR_ATMOST1, 0, IPS_IDLE);

    // ==================== DEPRECATED properties (kept for backward compatibility) ====================

    // ALIGNMENT_POINT_MANDATORY_NUMBERS
    AlignmentPointSetEntryV[ENTRY_OBSERVATION_JULIAN_DATE].fill(
        "ALIGNMENT_POINT_ENTRY_OBSERVATION_JULIAN_DATE", "Observation Julian date",
        "%g", 0, 60000, 0, 0);
    AlignmentPointSetEntryV[ENTRY_RA].fill("ALIGNMENT_POINT_ENTRY_RA",
                                           "Right Ascension (hh:mm:ss)", "%010.6m", 0, 24, 0, 0);
    AlignmentPointSetEntryV[ENTRY_DEC].fill("ALIGNMENT_POINT_ENTRY_DEC",
                                            "Declination (dd:mm:ss)", "%010.6m", -90, 90, 0, 0);
    AlignmentPointSetEntryV[ENTRY_VECTOR_X].fill("ALIGNMENT_POINT_ENTRY_VECTOR_X",
            "Telescope direction vector x", "%g", -FLT_MAX, FLT_MAX, 0, 0);
    AlignmentPointSetEntryV[ENTRY_VECTOR_Y].fill("ALIGNMENT_POINT_ENTRY_VECTOR_Y",
            "Telescope direction vector y", "%g", -FLT_MAX, FLT_MAX, 0, 0);
    AlignmentPointSetEntryV[ENTRY_VECTOR_Z].fill("ALIGNMENT_POINT_ENTRY_VECTOR_Z",
            "Telescope direction vector z", "%g", -FLT_MAX, FLT_MAX, 0, 0);
    AlignmentPointSetEntryV.fill(pTelescope->getDeviceName(),
                                 "ALIGNMENT_POINT_MANDATORY_NUMBERS",
                                 "Mandatory sync point numeric fields (deprecated)",
                                 ALIGNMENT_TAB, IP_RW, 60, IPS_IDLE);

    // ALIGNMENT_POINT_OPTIONAL_BINARY_BLOB (deprecated, unused)
    AlignmentPointSetPrivateBinaryDataV[0].fill("ALIGNMENT_POINT_ENTRY_PRIVATE",
            "Private binary data", "alignmentPrivateData");
    AlignmentPointSetPrivateBinaryDataV.fill(pTelescope->getDeviceName(),
                                       "ALIGNMENT_POINT_OPTIONAL_BINARY_BLOB",
                                       "Optional sync point binary data (deprecated)",
                                       ALIGNMENT_TAB, IP_RW, 60, IPS_IDLE);

    // ALIGNMENT_POINTSET_SIZE (deprecated)
    AlignmentPointSetSizeV[0].fill("ALIGNMENT_POINTSET_SIZE", "Size",
                                   "%g", 0, 100000, 0, 0);
    AlignmentPointSetSizeV.fill(pTelescope->getDeviceName(),
                                "ALIGNMENT_POINTSET_SIZE",
                                "Current Set (deprecated)",
                                ALIGNMENT_TAB, IP_RO, 60, IPS_IDLE);

    // ALIGNMENT_POINTSET_CURRENT_ENTRY (deprecated)
    AlignmentPointSetPointerV[0].fill("ALIGNMENT_POINTSET_CURRENT_ENTRY", "Pointer",
                                      "%g", 0, 100000, 0, 0);
    AlignmentPointSetPointerV.fill(pTelescope->getDeviceName(),
                                   "ALIGNMENT_POINTSET_CURRENT_ENTRY",
                                   "Current Set (deprecated)",
                                   ALIGNMENT_TAB, IP_RW, 60, IPS_IDLE);

    // ALIGNMENT_POINTSET_ACTION (deprecated)
    AlignmentPointSetActionV[APPEND].fill("APPEND",
                                          "Add entries at end of set (deprecated)", ISS_ON);
    AlignmentPointSetActionV[INSERT].fill("INSERT",
                                          "Insert entries at current index (deprecated)", ISS_OFF);
    AlignmentPointSetActionV[EDIT].fill("EDIT",
                                        "Overwrite entry at current index (deprecated)", ISS_OFF);
    AlignmentPointSetActionV[DELETE].fill("DELETE",
                                          "Delete entry at current index (deprecated)", ISS_OFF);
    AlignmentPointSetActionV[CLEAR].fill("CLEAR",
                                         "Delete all the entries in the set (deprecated)", ISS_OFF);
    AlignmentPointSetActionV[READ].fill("READ",
                                        "Read the entry at the current pointer (deprecated)", ISS_OFF);
    AlignmentPointSetActionV[READ_INCREMENT].fill("READ INCREMENT",
            "Increment the pointer before reading the entry (deprecated)", ISS_OFF);
    AlignmentPointSetActionV[LOAD_DATABASE].fill("LOAD DATABASE",
            "Load the alignment database from local storage (deprecated)", ISS_OFF);
    AlignmentPointSetActionV[SAVE_DATABASE].fill("SAVE DATABASE",
            "Save the alignment database to local storage (deprecated)", ISS_OFF);
    AlignmentPointSetActionV.fill(pTelescope->getDeviceName(),
                                  "ALIGNMENT_POINTSET_ACTION",
                                  "Action to take (deprecated)",
                                  ALIGNMENT_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // ALIGNMENT_POINTSET_COMMIT (deprecated)
    AlignmentPointSetCommitV[0].fill("ALIGNMENT_POINTSET_COMMIT", "OK (deprecated)", ISS_OFF);
    AlignmentPointSetCommitV.fill(pTelescope->getDeviceName(),
                                  "ALIGNMENT_POINTSET_COMMIT",
                                  "Execute the action (deprecated)",
                                  ALIGNMENT_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
}

void MapPropertiesToInMemoryDatabase::ISGetProperties(Telescope *pTelescope)
{
    // ==================== NEW properties (simplified, cursor-free) ====================

    pTelescope->defineProperty(AlignmentModelBlob);
    pTelescope->defineProperty(AlignmentModelCountNP);
    pTelescope->defineProperty(AlignPointNewNP);
    pTelescope->defineProperty(AlignPointDeleteIndexNP);
    pTelescope->defineProperty(AlignmentClearSP);
    pTelescope->defineProperty(AlignmentLoadSP);
    pTelescope->defineProperty(AlignmentSaveSP);

    // ==================== DEPRECATED properties (kept for backward compatibility) ====================

    pTelescope->defineProperty(AlignmentPointSetEntryV);
    pTelescope->defineProperty(AlignmentPointSetPrivateBinaryDataV);
    pTelescope->defineProperty(AlignmentPointSetSizeV);
    pTelescope->defineProperty(AlignmentPointSetPointerV);
    pTelescope->defineProperty(AlignmentPointSetActionV);
    pTelescope->defineProperty(AlignmentPointSetCommitV);
}

void MapPropertiesToInMemoryDatabase::ProcessBlobProperties(Telescope *pTelescope, const char *name,
        int sizes[], int blobsizes[], char *blobs[],
        char *formats[], char *names[], int n)
{
    INDI_UNUSED(sizes);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);

    DEBUGFDEVICE(pTelescope->getDeviceName(), INDI::Logger::DBG_DEBUG, "ProcessBlobProperties - name(%s)", name);

    // --- Handle the new ALIGNMENT_MODEL_BLOB (client pushes a modified model back) ---
    if (AlignmentModelBlob.isNameMatch(name))
    {
        // Extract the JSON string from the incoming blob
        if (blobsizes[0] > 0 && blobs[0] != nullptr)
        {
            std::string JSONData(blobs[0], blobsizes[0]);

            bool success = DeserializeModelFromJSON(JSONData);
            if (success)
            {
                AlignmentModelBlob.setState(IPS_OK);
                AlignmentModelBlob.apply();

                // Re-initialize the math plugin with the new data
                // Note: The callback set in AlignmentSubsystemForDrivers constructor
                // will call Initialise(this). We trigger that here.
                UpdateModelCount();
            }
            else
            {
                AlignmentModelBlob.setState(IPS_ALERT);
                AlignmentModelBlob.apply();
            }
        }
        else
        {
            // Echo back a zero-length blob to acknowledge receipt
            AlignmentModelBlob[0].setBlob(nullptr);
            AlignmentModelBlob[0].setBlobLen(0);
            AlignmentModelBlob[0].setFormat(ALIGNMENT_MODEL_FORMAT);
            AlignmentModelBlob.setState(IPS_OK);
            AlignmentModelBlob.apply();
        }
    }

    // --- Handle the deprecated ALIGNMENT_POINT_OPTIONAL_BINARY_BLOB ---
    if (AlignmentPointSetPrivateBinaryDataV.isNameMatch(name))
    {
        // Echo back a zero-length blob to acknowledge receipt
        AlignmentPointSetPrivateBinaryDataV[0].setBlob(nullptr);
        AlignmentPointSetPrivateBinaryDataV[0].setBlobLen(0);
        AlignmentPointSetPrivateBinaryDataV[0].setFormat("alignmentPrivateData");
        AlignmentPointSetPrivateBinaryDataV.setState(IPS_OK);
        AlignmentPointSetPrivateBinaryDataV.apply();
    }
}

void MapPropertiesToInMemoryDatabase::ProcessNumberProperties(Telescope *, const char *name,
        double values[], char *names[], int n)
{
    // --- Handle new ALIGN_POINT_NEW (append a sync point) ---
    if (AlignPointNewNP.isNameMatch(name))
    {
        AlignmentDatabaseEntry entry;
        // Read the values. The fill defaults provide sensible starting values.
        for (int i = 0; i < n; i++)
        {
            if (strcmp(names[i], "ALIGN_POINT_JD") == 0)
                entry.ObservationJulianDate = values[i];
            else if (strcmp(names[i], "ALIGN_POINT_RA") == 0)
                entry.RightAscension = values[i];
            else if (strcmp(names[i], "ALIGN_POINT_DEC") == 0)
                entry.Declination = values[i];
            else if (strcmp(names[i], "ALIGN_POINT_VECTOR_X") == 0)
                entry.TelescopeDirection.x = values[i];
            else if (strcmp(names[i], "ALIGN_POINT_VECTOR_Y") == 0)
                entry.TelescopeDirection.y = values[i];
            else if (strcmp(names[i], "ALIGN_POINT_VECTOR_Z") == 0)
                entry.TelescopeDirection.z = values[i];
        }
        entry.PrivateDataSize = 0;

        GetAlignmentDatabase().push_back(entry);
        AlignPointNewNP.setState(IPS_OK);
        AlignPointNewNP.apply();

        // Reset the write-only values so the next write is detected
        AlignPointNewNP[NEW_JD].setValue(0);
        AlignPointNewNP[NEW_RA].setValue(0);
        AlignPointNewNP[NEW_DEC].setValue(0);
        AlignPointNewNP[NEW_VX].setValue(0);
        AlignPointNewNP[NEW_VY].setValue(0);
        AlignPointNewNP[NEW_VZ].setValue(0);

        UpdateModelCount();
        return;
    }

    // --- Handle new ALIGN_POINT_DELETE_INDEX ---
    if (AlignPointDeleteIndexNP.isNameMatch(name))
    {
        unsigned int index = static_cast<unsigned int>(AlignPointDeleteIndexNP[0].getValue());
        auto &db = GetAlignmentDatabase();

        if (index < db.size())
        {
            db.erase(db.begin() + index);
            AlignPointDeleteIndexNP.setState(IPS_OK);
        }
        else
        {
            AlignPointDeleteIndexNP.setState(IPS_ALERT);
        }

        // Reset to sentinel value
        AlignPointDeleteIndexNP[0].setValue(-1);
        AlignPointDeleteIndexNP.apply();

        UpdateModelCount();
        return;
    }

    // --- Handle deprecated ALIGNMENT_POINT_MANDATORY_NUMBERS ---
    if (AlignmentPointSetEntryV.isNameMatch(name))
    {
        AlignmentPointSetEntryV.setState(IPS_OK);
        AlignmentPointSetEntryV.apply();
    }
    // --- Handle deprecated ALIGNMENT_POINTSET_CURRENT_ENTRY ---
    else if (AlignmentPointSetPointerV.isNameMatch(name))
    {
        AlignmentPointSetPointerV.setState(IPS_OK);
        AlignmentPointSetPointerV.apply();
    }
}

void MapPropertiesToInMemoryDatabase::ProcessSwitchProperties(Telescope *pTelescope, const char *name,
        ISState *states, char *names[], int n)
{
    INDI_UNUSED(states);
    INDI_UNUSED(names);
    INDI_UNUSED(n);

    AlignmentDatabaseType &AlignmentDatabase = GetAlignmentDatabase();

    // --- Handle new ALIGNMENT_CLEAR ---
    if (AlignmentClearSP.isNameMatch(name))
    {
        AlignmentDatabase.clear();
        AlignmentClearSP[CLEAR_S].setState(ISS_OFF);
        AlignmentClearSP.setState(IPS_OK);
        AlignmentClearSP.apply();

        UpdateModelCount();

        return;
    }

    // --- Handle new ALIGNMENT_LOAD ---
    if (AlignmentLoadSP.isNameMatch(name))
    {
        if (LoadDatabase(pTelescope->getDeviceName()))
        {
            DEBUGDEVICE(pTelescope->getDeviceName(), INDI::Logger::DBG_SESSION,
                        "Alignment database loaded successfully");
            AlignmentLoadSP.setState(IPS_OK);
            UpdateModelCount();
        }
        else
        {
            DEBUGDEVICE(pTelescope->getDeviceName(), INDI::Logger::DBG_ERROR,
                        "Failed to load alignment database");
            AlignmentLoadSP.setState(IPS_ALERT);
        }
        AlignmentLoadSP[LOAD_S].setState(ISS_OFF);
        AlignmentLoadSP.apply();
        return;
    }

    // --- Handle new ALIGNMENT_SAVE ---
    if (AlignmentSaveSP.isNameMatch(name))
    {
        if (SaveDatabase(pTelescope->getDeviceName()))
        {
            DEBUGDEVICE(pTelescope->getDeviceName(), INDI::Logger::DBG_SESSION,
                        "Alignment database saved successfully");
            AlignmentSaveSP.setState(IPS_OK);
        }
        else
        {
            DEBUGDEVICE(pTelescope->getDeviceName(), INDI::Logger::DBG_ERROR,
                        "Failed to save alignment database");
            AlignmentSaveSP.setState(IPS_ALERT);
        }
        AlignmentSaveSP[SAVE_S].setState(ISS_OFF);
        AlignmentSaveSP.apply();
        return;
    }

    // --- Handle deprecated ALIGNMENT_POINTSET_ACTION ---
    if (AlignmentPointSetActionV.isNameMatch(name))
    {
        AlignmentPointSetActionV.setState(IPS_OK);
        AlignmentPointSetActionV.apply();
        return;
    }

    // --- Handle deprecated ALIGNMENT_POINTSET_COMMIT ---
    if (AlignmentPointSetCommitV.isNameMatch(name))
    {
        const unsigned int Offset = static_cast<unsigned int>(AlignmentPointSetPointerV[0].getValue());
        AlignmentPointSetCommitV.setState(IPS_OK);

        // Build an AlignmentDatabaseEntry from the deprecated MANDATORY_NUMBERS fields
        AlignmentDatabaseEntry CurrentValues;
        CurrentValues.ObservationJulianDate = AlignmentPointSetEntryV[ENTRY_OBSERVATION_JULIAN_DATE].getValue();
        CurrentValues.RightAscension        = AlignmentPointSetEntryV[ENTRY_RA].getValue();
        CurrentValues.Declination           = AlignmentPointSetEntryV[ENTRY_DEC].getValue();
        CurrentValues.TelescopeDirection.x  = AlignmentPointSetEntryV[ENTRY_VECTOR_X].getValue();
        CurrentValues.TelescopeDirection.y  = AlignmentPointSetEntryV[ENTRY_VECTOR_Y].getValue();
        CurrentValues.TelescopeDirection.z  = AlignmentPointSetEntryV[ENTRY_VECTOR_Z].getValue();
        CurrentValues.PrivateDataSize       = 0;

        // Dispatch based on the action
        if (AlignmentPointSetActionV[APPEND].getState() == ISS_ON)
        {
            AlignmentDatabase.push_back(CurrentValues);
            AlignmentPointSetSizeV[0].setValue(AlignmentDatabase.size());
            AlignmentPointSetSizeV.apply();
        }
        else if (AlignmentPointSetActionV[INSERT].getState() == ISS_ON)
        {
            if (Offset > AlignmentDatabase.size())
                AlignmentPointSetCommitV.setState(IPS_ALERT);
            else
            {
                AlignmentDatabase.insert(AlignmentDatabase.begin() + Offset, CurrentValues);
                AlignmentPointSetSizeV[0].setValue(AlignmentDatabase.size());
                AlignmentPointSetSizeV.apply();
            }
        }
        else if (AlignmentPointSetActionV[EDIT].getState() == ISS_ON)
        {
            if (Offset >= AlignmentDatabase.size())
                AlignmentPointSetCommitV.setState(IPS_ALERT);
            else
                AlignmentDatabase[Offset] = CurrentValues;
        }
        else if (AlignmentPointSetActionV[DELETE].getState() == ISS_ON)
        {
            if (Offset >= AlignmentDatabase.size())
                AlignmentPointSetCommitV.setState(IPS_ALERT);
            else
            {
                AlignmentDatabase.erase(AlignmentDatabase.begin() + Offset);
                AlignmentPointSetSizeV[0].setValue(AlignmentDatabase.size());
                AlignmentPointSetSizeV.apply();
            }
        }
        else if (AlignmentPointSetActionV[CLEAR].getState() == ISS_ON)
        {
            AlignmentDatabase.clear();
            AlignmentPointSetSizeV[0].setValue(0);
            AlignmentPointSetSizeV.apply();
        }
        else if ((AlignmentPointSetActionV[READ].getState() == ISS_ON) ||
                 (AlignmentPointSetActionV[READ_INCREMENT].getState() == ISS_ON))
        {
            unsigned int readOffset = Offset;
            if (AlignmentPointSetActionV[READ_INCREMENT].getState() == ISS_ON)
            {
                readOffset = Offset + 1;
                AlignmentPointSetPointerV[0].setValue(readOffset);
                AlignmentPointSetPointerV.apply();
            }

            if (readOffset >= AlignmentDatabase.size())
                AlignmentPointSetCommitV.setState(IPS_ALERT);
            else
            {
                AlignmentPointSetEntryV[ENTRY_OBSERVATION_JULIAN_DATE].setValue(
                    AlignmentDatabase[readOffset].ObservationJulianDate);
                AlignmentPointSetEntryV[ENTRY_RA].setValue(AlignmentDatabase[readOffset].RightAscension);
                AlignmentPointSetEntryV[ENTRY_DEC].setValue(AlignmentDatabase[readOffset].Declination);
                AlignmentPointSetEntryV[ENTRY_VECTOR_X].setValue(
                    AlignmentDatabase[readOffset].TelescopeDirection.x);
                AlignmentPointSetEntryV[ENTRY_VECTOR_Y].setValue(
                    AlignmentDatabase[readOffset].TelescopeDirection.y);
                AlignmentPointSetEntryV[ENTRY_VECTOR_Z].setValue(
                    AlignmentDatabase[readOffset].TelescopeDirection.z);
                AlignmentPointSetEntryV.apply();
            }
        }
        else if (AlignmentPointSetActionV[LOAD_DATABASE].getState() == ISS_ON)
        {
            LoadDatabase(pTelescope->getDeviceName());
            AlignmentPointSetSizeV[0].setValue(AlignmentDatabase.size());
            AlignmentPointSetSizeV.apply();
        }
        else if (AlignmentPointSetActionV[SAVE_DATABASE].getState() == ISS_ON)
        {
            SaveDatabase(pTelescope->getDeviceName());
        }

        // Reset the commit switch
        AlignmentPointSetCommitV[0].setState(ISS_OFF);
        AlignmentPointSetCommitV.apply();

        // Also update the new model blob/count
        UpdateModelCount();
    }
}

void MapPropertiesToInMemoryDatabase::UpdateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);
    IGeographicCoordinates Position { 0, 0, 0 };

    if (GetDatabaseReferencePosition(Position))
    {
        // Position is already valid
        if ((latitude != Position.latitude) || (longitude != Position.longitude))
        {
            // Warn the user somehow
        }
    }
    else
        SetDatabaseReferencePosition(latitude, longitude);
}

void MapPropertiesToInMemoryDatabase::UpdateSize(MountAlignment_t MountAlignment)
{
    UpdateModelCount(MountAlignment);
}

// Private: update count and publish blob
void MapPropertiesToInMemoryDatabase::UpdateModelCount(MountAlignment_t MountAlignment)
{
    size_t count = GetAlignmentDatabase().size();

    // Update new count property
    AlignmentModelCountNP[0].setValue(count);
    AlignmentModelCountNP.apply();

    // Update deprecated count property
    AlignmentPointSetSizeV[0].setValue(count);
    AlignmentPointSetSizeV.apply();

    // Publish the full model BLOB
    PublishModelBlob(MountAlignment);
}

void MapPropertiesToInMemoryDatabase::PublishModelBlob(MountAlignment_t MountAlignment)
{
    m_SerializedModelJSON = SerializeModelToJSON(MountAlignment);

    // Publish the JSON blob to clients
    AlignmentModelBlob[0].setBlob(const_cast<char *>(m_SerializedModelJSON.c_str()));
    AlignmentModelBlob[0].setBlobLen(m_SerializedModelJSON.size());
    AlignmentModelBlob[0].setSize(m_SerializedModelJSON.size());
    AlignmentModelBlob[0].setFormat(ALIGNMENT_MODEL_FORMAT);
    AlignmentModelBlob.setState(IPS_OK);
    AlignmentModelBlob.apply();
}

} // namespace AlignmentSubsystem
} // namespace INDI