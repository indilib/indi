/*!
 * \file MapPropertiesToInMemoryDatabase.h
 *
 * \author Roger James
 * \date 13th November 2013
 *
 */

#pragma once

#include "InMemoryDatabase.h"

#include "inditelescope.h"

namespace INDI
{
namespace AlignmentSubsystem
{
/*!
 * \class MapPropertiesToInMemoryDatabase
 * \brief Maps the in-memory alignment database to INDI properties.
 *
 * An entry in the sync point database is defined by the following INDI properties:
 * - ALIGNMENT_POINT_ENTRY_OBSERVATION_JULIAN_DATE\n
 *   The Julian date of the sync point observation (number)
 * - ALIGNMENT_POINT_ENTRY_RA\n
 *   The right ascension of the sync point (number)
 * - ALIGNMENT_POINT_ENTRY_DEC\n
 *   The declination of the sync point (number)
 * - ALIGNMENT_POINT_ENTRY_VECTOR_X\n
 *   The x component of the telescope direction vector of the sync point (number)
 * - ALIGNMENT_POINT_ENTRY_VECTOR_Y\n
 *   The y component of the telescope direction vector of the sync point (number)
 * - ALIGNMENT_POINT_ENTRY_VECTOR_Z\n
 *   The z component of the telescope direction vector of the sync point (number)
 * .
 *
 * The database is accessed using the following new properties (simplified, cursor-free):
 * - ALIGNMENT_MODEL_BLOB\n
 *   A JSON BLOB containing the complete alignment model (including reference position,
 *   mount alignment, and all sync points). Driver publishes on change; client can push
 *   a modified model back to replace the entire database in one message.
 * - ALIGNMENT_MODEL_COUNT\n
 *   The count of the number of sync points in the set (number, read-only).
 * - ALIGN_POINT_NEW\n
 *   Append a single sync point (number, write-only). Elements: JD, RA, DEC, VX, VY, VZ.
 * - ALIGN_POINT_DELETE_INDEX\n
 *   Delete the sync point at the given 0-based index (number, write-only). Element: INDEX.
 * - ALIGNMENT_CLEAR\n
 *   Delete all sync points from the database (switch, write-only, AtMostOne). Member: CLEAR.
 * - ALIGNMENT_LOAD\n
 *   Load the database from persistent storage (switch, write-only, AtMostOne). Member: LOAD.
 * - ALIGNMENT_SAVE\n
 *   Save the database to persistent storage (switch, write-only, AtMostOne). Member: SAVE.
 * .
 *
 * The following DEPRECATED properties are kept for backward compatibility:
 * - ALIGNMENT_POINTSET_SIZE\n
 *   The count of the number of sync points in the set (number). Replaced by ALIGNMENT_MODEL_COUNT.
 * - ALIGNMENT_POINTSET_CURRENT_ENTRY\n
 *   A zero based number that sets/shows the current entry (number).
 *   Only valid if ALIGNMENT_POINTSET_SIZE is greater than zero.
 *   Replaced by index-based and BLOB operations.
 * - ALIGNMENT_POINTSET_ACTION\n
 *   Determines the action to take when the COMMIT property is written.
 *   - APPEND\n
 *     Append a new entry to the set. Replaced by ALIGN_POINT_NEW.
 *   - INSERT\n
 *     Insert a new entry at the pointer. Replaced by BLOB replace + re-add.
 *   - EDIT\n
 *     Overwrites the entry at the pointer. Replaced by BLOB replace.
 *   - DELETE\n
 *     Delete the entry at the pointer. Replaced by ALIGN_POINT_DELETE_INDEX.
 *   - CLEAR\n
 *     Delete all entries. Replaced by ALIGNMENT_CLEAR.
 *   - READ\n
 *     Read the entry at the pointer. Replaced by ALIGNMENT_MODEL_BLOB.
 *   - READ INCREMENT\n
 *     Increment the pointer before reading the entry. Replaced by ALIGNMENT_MODEL_BLOB.
 *   - LOAD DATABASE\n
 *     Load the database from local storage. Replaced by ALIGNMENT_LOAD.
 *   - SAVE DATABASE\n
 *     Save the database to local storage. Replaced by ALIGNMENT_SAVE.
 * - ALIGNMENT_POINTSET_COMMIT\n
 *   When written take the action defined above. Replaced by direct property writes.
 *   - COMMIT
 * - ALIGNMENT_POINT_MANDATORY_NUMBERS\n
 *   The sync point numeric fields. Replaced by ALIGN_POINT_NEW and ALIGNMENT_MODEL_BLOB.
 * - ALIGNMENT_POINT_OPTIONAL_BINARY_BLOB\n
 *   Optional binary blob for communication between the client and the math plugin.
 *   Removed (unused by any driver or math plugin).
 * .
 *
 */
class MapPropertiesToInMemoryDatabase : public InMemoryDatabase
{
    public:
        /// \brief Virtual destructor
        virtual ~MapPropertiesToInMemoryDatabase() {}

        // Public methods

        /** \brief Initialize alignment database properties. It is recommended to call this function within initProperties()
            * of your primary device.
            * This registers both the new simplified properties and the deprecated legacy properties.
            * \param[in] pTelescope Pointer to the child INDI::Telecope class
           */
        void InitProperties(Telescope *pTelescope);

        /** \brief Call this function from within the ISNewBLOB processing path. The function will
            * handle any alignment database related properties.
            * \param[in] pTelescope Pointer to the child INDI::Telecope class
            * \param[in] name vector property name
            * \param[in] sizes
            * \param[in] blobsizes
            * \param[in] blobs
            * \param[in] formats
            * \param[in] names
            * \param[in] n
           */
        void ProcessBlobProperties(Telescope *pTelescope, const char *name, int sizes[], int blobsizes[], char *blobs[],
                                   char *formats[], char *names[], int n);

        /** \brief Call this function from within the ISNewNumber processing path. The function will
            * handle any alignment database related properties.
            * \param[in] pTelescope Pointer to the child INDI::Telecope class
            * \param[in] name vector property name
            * \param[in] values value as passed by the client
            * \param[in] names names as passed by the client
            * \param[in] n number of values and names pair to process.
           */
        void ProcessNumberProperties(Telescope *, const char *name, double values[], char *names[], int n);

        /** \brief Call this function from within the ISNewSwitch processing path. The function will
            * handle any alignment database related properties.
            * \param[in] pTelescope Pointer to the child INDI::Telecope class
            * \param[in] name vector property name
            * \param[in] states states as passed by the client
            * \param[in] names names as passed by the client
            * \param[in] n number of values and names pair to process.
           */
        void ProcessSwitchProperties(Telescope *pTelescope, const char *name, ISState *states, char *names[], int n);

        /** \brief Call this function from within the updateLocation processing path.
            * \param[in] latitude Site latitude in degrees.
            * \param[in] longitude Site latitude in degrees increasing eastward from Greenwich (0 to 360).
            * \param[in] elevation Site elevation in meters.
           */
        void UpdateLocation(double latitude, double longitude, double elevation);

        /** \brief Call this function when the number of entries in the database changes.
            * Updates the ALIGNMENT_MODEL_COUNT property and publishes the full model
            * via the ALIGNMENT_MODEL_BLOB property.
            * \param[in] MountAlignment The current approximate mount alignment for serialization
            *               (ZENITH, NORTH_CELESTIAL_POLE, or SOUTH_CELESTIAL_POLE). Defaults to ZENITH.
           */
        void UpdateSize(MountAlignment_t MountAlignment = ZENITH);

    private:
        /** \brief Update the count properties and publish the model BLOB.
            * Called internally by UpdateSize() and from property handlers.
            * \param[in] MountAlignment The current approximate mount alignment for serialization (defaults to ZENITH).
           */
        void UpdateModelCount(MountAlignment_t MountAlignment = ZENITH);
        /** \brief Publish the full alignment model as a JSON BLOB to all clients.
            * This is called automatically by UpdateSize() whenever the database changes.
            * \param[in] MountAlignment The current approximate mount alignment for serialization.
           */
        void PublishModelBlob(MountAlignment_t MountAlignment);

        // --- NEW properties (simplified, cursor-free) ---

        /** \brief ALIGNMENT_MODEL_BLOB — JSON BLOB containing the complete alignment model.
            * Driver publishes on change; client can push a modified model back to replace
            * the entire database in one message.
            * Format: "alignmentModel.json"
           */
        INDI::PropertyBlob AlignmentModelBlob {1};

        /** \brief ALIGNMENT_MODEL_COUNT — Number of sync points in the model (read-only number). */
        INDI::PropertyNumber AlignmentModelCountNP {1};

        /** \brief ALIGN_POINT_NEW — Append a single sync point (write-only number).
            * Elements: JD (ObservationJulianDate), RA (decimal hours), DEC (decimal degrees),
            *           VX, VY, VZ (TelescopeDirectionVector components).
           */
        INDI::PropertyNumber AlignPointNewNP {6};
        enum
        {
            /** \brief Julian Date of the observation */
            NEW_JD,
            /** \brief Right Ascension in decimal hours */
            NEW_RA,
            /** \brief Declination in decimal degrees */
            NEW_DEC,
            /** \brief Telescope direction vector X component */
            NEW_VX,
            /** \brief Telescope direction vector Y component */
            NEW_VY,
            /** \brief Telescope direction vector Z component */
            NEW_VZ
        };

        /** \brief ALIGN_POINT_DELETE_INDEX — Delete the sync point at the given 0-based index
            * (write-only number). Single element: INDEX.
           */
        INDI::PropertyNumber AlignPointDeleteIndexNP {1};

        /** \brief ALIGNMENT_CLEAR — Delete all sync points from the database (write-only switch, AtMostOne).
            * Member: CLEAR.
           */
        INDI::PropertySwitch AlignmentClearSP {1};
        /** \brief Clear switch index */
        enum { CLEAR_S };

        /** \brief ALIGNMENT_LOAD — Load the database from persistent storage (write-only switch, AtMostOne).
            * Member: LOAD.
           */
        INDI::PropertySwitch AlignmentLoadSP {1};
        /** \brief Load switch index */
        enum { LOAD_S };

        /** \brief ALIGNMENT_SAVE — Save the database to persistent storage (write-only switch, AtMostOne).
            * Member: SAVE.
           */
        INDI::PropertySwitch AlignmentSaveSP {1};
        /** \brief Save switch index */
        enum { SAVE_S };

        // --- DEPRECATED properties (kept for backward compatibility) ---

        /** \brief DEPRECATED: ALIGNMENT_POINT_MANDATORY_NUMBERS
            * \deprecated Replaced by ALIGN_POINT_NEW and ALIGNMENT_MODEL_BLOB.
            * Elements: JD, RA, DEC, VX, VY, VZ.
           */
        INDI::PropertyNumber AlignmentPointSetEntryV {6};

        /** \brief DEPRECATED: ALIGNMENT_POINT_OPTIONAL_BINARY_BLOB
            * \deprecated Removed — unused by any driver or math plugin.
           */
        INDI::PropertyBlob AlignmentPointSetPrivateBinaryDataV {1};

        /** \brief DEPRECATED: ALIGNMENT_POINTSET_SIZE
            * \deprecated Replaced by ALIGNMENT_MODEL_COUNT.
           */
        INDI::PropertyNumber AlignmentPointSetSizeV {1};

        /** \brief DEPRECATED: ALIGNMENT_POINTSET_CURRENT_ENTRY
            * \deprecated Replaced by index-based and BLOB operations.
           */
        INDI::PropertyNumber AlignmentPointSetPointerV {1};

        /** \brief DEPRECATED: ALIGNMENT_POINTSET_ACTION
            * \deprecated Replaced by dedicated properties (ALIGN_POINT_NEW, etc.).
            * Members: APPEND, INSERT, EDIT, DELETE, CLEAR, READ, READ_INCREMENT, LOAD_DATABASE, SAVE_DATABASE.
           */
        INDI::PropertySwitch AlignmentPointSetActionV {9};

        /** \brief DEPRECATED: ALIGNMENT_POINTSET_COMMIT
            * \deprecated No longer needed — new properties act on write.
            * Member: COMMIT.
           */
        INDI::PropertySwitch AlignmentPointSetCommitV {1};
};

} // namespace AlignmentSubsystem
} // namespace INDI