/*!
 * \file InMemoryDatabase.h
 *
 * \author Roger James
 * \date 13th November 2013
 *
 */

#pragma once

#include "Common.h"

#include "libastro.h"

#include <string>
#include <vector>

namespace INDI
{
namespace AlignmentSubsystem
{
/// \class InMemoryDatabase
/// \brief This class provides the driver side API to the in memory alignment database.
class InMemoryDatabase
{
    public:
        /// \brief Default constructor
        InMemoryDatabase();

        /// \brief Virtual destructor
        virtual ~InMemoryDatabase() {}

        typedef std::vector<AlignmentDatabaseEntry> AlignmentDatabaseType;

        // Public methods

        /// \brief Check if a entry already exists in the database
        /// \param[in] CandidateEntry The candidate entry to check
        /// \param[in] Tolerance The % tolerance used in the checking process (default 0.1%)
        /// \return True if an entry already exists within the required tolerance
        bool CheckForDuplicateSyncPoint(const AlignmentDatabaseEntry &CandidateEntry, double Tolerance = 0.1) const;

        /// \brief Remove a sync point that falls within the tolerance of a candidate point.
        /// \param[in] CandidateEntry The candidate entry to check
        /// \param[in] Tolerance The % tolerance used in the checking process (default 0.1%)
        void RemoveSyncPoint(const AlignmentDatabaseEntry &CandidateEntry, double Tolerance = 0.1);

        /// \brief Get a reference to the in memory database.
        /// \return A reference to the in memory database.
        AlignmentDatabaseType &GetAlignmentDatabase()
        {
            return MySyncPoints;
        }

        /// \brief Get the database reference position
        /// \param[in] Position A pointer to a IGeographicCoordinates object to return the current position in
        /// \return True if successful
        bool GetDatabaseReferencePosition(IGeographicCoordinates &Position);

        /// \brief Load the database from persistent storage
        /// \param[in] DeviceName The name of the current device.
        /// \return True if successful
        bool LoadDatabase(const char *DeviceName);

        /// \brief Save the database to persistent storage
        /// \param[in] DeviceName The name of the current device.
        /// \return True if successful
        bool SaveDatabase(const char *DeviceName);

        /// \brief Set the database reference position
        /// \param[in] Latitude
        /// \param[in] Longitude
        void SetDatabaseReferencePosition(double Latitude, double Longitude);
        /// \brief Set the database reference position
        /// \param[in] Position
        void SetDatabaseReferencePosition(const IGeographicCoordinates &Position);

        typedef void (*LoadDatabaseCallbackPointer_t)(void *);

        /// \brief Set the function to be called when the database is loaded or reloaded
        /// \param[in] CallbackPointer A pointer to the class function to call
        /// \param[in] ThisPointer A pointer to the class object of the callback function
        void SetLoadDatabaseCallback(LoadDatabaseCallbackPointer_t CallbackPointer, void *ThisPointer);

        /// \brief Serialize the entire model (including reference position and mount alignment) to a JSON string.
        /// \param[in] MountAlignment The approximate mount alignment (ZENITH, NORTH_CELESTIAL_POLE, SOUTH_CELESTIAL_POLE).
        /// \return A JSON string representing the complete alignment model.
        std::string SerializeModelToJSON(MountAlignment_t MountAlignment) const;

        /// \brief Deserialize a JSON string and replace the in-memory database.
        /// \param[in] JSONData The JSON string to deserialize.
        /// \return True if successful, false if the JSON is malformed or has an unsupported version.
        bool DeserializeModelFromJSON(const std::string &JSONData);

    private:
        AlignmentDatabaseType MySyncPoints;
        IGeographicCoordinates DatabaseReferencePosition;
        bool DatabaseReferencePositionIsValid;
        LoadDatabaseCallbackPointer_t LoadDatabaseCallback;
        void *LoadDatabaseCallbackThisPointer;
};

} // namespace AlignmentSubsystem
} // namespace INDI