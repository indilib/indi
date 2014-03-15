/*!
 * \file ClientAPIForAlignmentDatabase.h
 *
 * \author Roger James
 * \date 13th November 2013
 *
 */

#ifndef INDI_ALIGNMENTSUBSYSTEM_CLIENTAPIFORALIGNMENTDATABASE_H
#define INDI_ALIGNMENTSUBSYSTEM_CLIENTAPIFORALIGNMENTDATABASE_H

#include "Common.h"

#include "indibase/basedevice.h"
#include "indibase/baseclient.h"

#include <string>

namespace INDI {
namespace AlignmentSubsystem {

/*!
 * \class ClientAPIForAlignmentDatabase
 * \brief This class provides the client API to the driver side alignment database. It communicates
 * with the driver via the INDI properties interface.
 */
class ClientAPIForAlignmentDatabase
{
public:
    /// \brief Default constructor
    ClientAPIForAlignmentDatabase();

    /// \brief Virtual destructor
    virtual ~ClientAPIForAlignmentDatabase();

    /** \brief Append a sync point to the database.
        \param[in] CurrentValues The entry to append.
        \return True if successful
    */
    bool AppendSyncPoint(const AlignmentDatabaseEntry& CurrentValues);

    /** \brief Delete all sync points from the database.
        \return True if successful
    */
    bool ClearSyncPoints();

    /** \brief Delete a sync point from the database.
        \param[in] Offset Pointer to the entry to delete
        \return True if successful
    */
    bool DeleteSyncPoint(unsigned int Offset);

    /** \brief Edit a sync point in the database.
        \param[in] Offset Pointer to where to make the edit.
        \param[in] CurrentValues The entry to edit.
        \return True if successful
    */
    bool EditSyncPoint(unsigned int Offset, const AlignmentDatabaseEntry& CurrentValues);

    /** \brief Return the number of entries in the database.
        \return The number of entries in the database
    */
    const int GetDatabaseSize();

    /// \brief Initialise the API
    /// \param[in] BaseClient A pointer to the INDI::BaseClient class
    void Initialise(INDI::BaseClient *BaseClient);

    /** \brief Insert a sync point in the database.
        \param[in] Offset Pointer to where to make then insertion.
        \param[in] CurrentValues The entry to insert.
        \return True if successful
    */
    bool InsertSyncPoint(unsigned int Offset, const AlignmentDatabaseEntry& CurrentValues);

    /** \brief Load the database from persistent storage
        \return True if successful
    */
    bool LoadDatabase();

    /** \brief Process new BLOB message from driver. This routine should be called from within
     the newBLOB handler in the client. This routine is not normally called directly but is called by
     the ProcessNewBLOB function in INDI::Alignment::AlignmentSubsystemForClients which filters out calls
     from unwanted devices. TODO maybe hide this function.
        \param[in] BLOBPointer A pointer to the INDI::IBLOB.
    */
    void ProcessNewBLOB(IBLOB *BLOBPointer);

    /** \brief Process new device message from driver. This routine should be called from within
     the newDevice handler in the client. This routine is not normally called directly but is called by
     the ProcessNewDevice function in INDI::Alignment::AlignmentSubsystemForClients which filters out calls
     from unwanted devices. TODO maybe hide this function.
        \param[in] DevicePointer A pointer to the INDI::BaseDevice object.
    */
    void ProcessNewDevice(INDI::BaseDevice *DevicePointer);

    /** \brief Process new number message from driver. This routine should be called from within
     the newNumber handler in the client. This routine is not normally called directly but is called by
     the ProcessNewNumber function in INDI::Alignment::AlignmentSubsystemForClients which filters out calls
     from unwanted devices. TODO maybe hide this function.
        \param[in] NumberVectorProperty A pointer to the INDI::INumberVectorProperty.
    */
    void ProcessNewNumber(INumberVectorProperty *NumberVectorProperty);

    /** \brief Process new property message from driver. This routine should be called from within
     the newProperty handler in the client. This routine is not normally called directly but is called by
     the ProcessNewProperty function in INDI::Alignment::AlignmentSubsystemForClients which filters out calls
     from unwanted devices. TODO maybe hide this function.
        \param[in] PropertyPointer A pointer to the INDI::Property object.
    */
    void ProcessNewProperty(INDI::Property *PropertyPointer);

    /** \brief Process new switch message from driver. This routine should be called from within
     the newSwitch handler in the client. This routine is not normally called directly but is called by
     the ProcessNewSwitch function in INDI::Alignment::AlignmentSubsystemForClients which filters out calls
     from unwanted devices. TODO maybe hide this function.
        \param[in] SwitchVectorProperty A pointer to the INDI::ISwitchVectorProperty.
    */
    void ProcessNewSwitch(ISwitchVectorProperty *SwitchVectorProperty);

    /** \brief Increment the current offset then read a sync point from the database.
        \param[out] CurrentValues The entry read.
        \return True if successful
    */
    bool ReadIncrementSyncPoint(AlignmentDatabaseEntry& CurrentValues);

    /** \brief Read a sync point from the database.
        \param[in] Offset Pointer to where to read from.
        \param[out] CurrentValues The entry read.
        \return True if successful
    */
    bool ReadSyncPoint(unsigned int Offset, AlignmentDatabaseEntry& CurrentValues);

    /** \brief Save the database to persistent storage
        \return True if successful
    */
    bool SaveDatabase();

private:

    // Private methods

    bool SendEntryData(const AlignmentDatabaseEntry& CurrentValues);
    bool SetDriverBusy();
    bool SignalDriverCompletion();
    bool WaitForDriverCompletion();

    // Private properties

    INDI::BaseClient *BaseClient;
    pthread_cond_t DriverActionCompleteCondition;
    pthread_mutex_t DriverActionCompleteMutex;
    bool DriverActionComplete;
    INDI::BaseDevice *Device;
    INDI::Property *MandatoryNumbers;
    INDI::Property *OptionalBinaryBlob;
    INDI::Property *PointsetSize;
    INDI::Property *CurrentEntry;
    INDI::Property *Action;
    INDI::Property *Commit;
};

} // namespace AlignmentSubsystem
} // namespace INDI

#endif // INDI_ALIGNMENTSUBSYSTEM_CLIENTAPIFORALIGNMENTDATABASE_H
