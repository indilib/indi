/*!
 * \file ClientAPIForAlignmentDatabase.cpp
 *
 * \author Roger James
 * \date 13th November 2013
 *
 */

#include "ClientAPIForAlignmentDatabase.h"

#include "indicom.h"

namespace INDI
{
namespace AlignmentSubsystem
{
ClientAPIForAlignmentDatabase::ClientAPIForAlignmentDatabase()
{
    pthread_cond_init(&DriverActionCompleteCondition, nullptr);
    pthread_mutex_init(&DriverActionCompleteMutex, nullptr);
}

ClientAPIForAlignmentDatabase::~ClientAPIForAlignmentDatabase()
{
    pthread_cond_destroy(&DriverActionCompleteCondition);
    pthread_mutex_destroy(&DriverActionCompleteMutex);
}

bool ClientAPIForAlignmentDatabase::AppendSyncPoint(const AlignmentDatabaseEntry &CurrentValues)
{
    // Wait for driver to initialise if neccessary
    WaitForDriverCompletion();

    auto pAction = Action->getSwitch();
    auto pCommit = Commit->getSwitch();

    if (APPEND != pAction->findOnSwitchIndex())
    {
        // Request Append mode
        pAction->reset();
        pAction->at(APPEND)->setState(ISS_ON);
        SetDriverBusy();
        BaseClient->sendNewSwitch(pAction);
        WaitForDriverCompletion();
        if (IPS_OK != pAction->getState())
        {
            IDLog("AppendSyncPoint - Bad Action switch state %s\n", pAction->getStateAsString());
            return false;
        }
    }

    if (!SendEntryData(CurrentValues))
        return false;

    // Commit the entry to the database
    pCommit->reset();
    pCommit->at(0)->setState(ISS_ON);
    SetDriverBusy();
    BaseClient->sendNewSwitch(pCommit);
    WaitForDriverCompletion();
    if (IPS_OK != pCommit->getState())
    {
        IDLog("AppendSyncPoint - Bad Commit switch state %s\n", pCommit->getStateAsString());
        return false;
    }

    return true;
}

bool ClientAPIForAlignmentDatabase::ClearSyncPoints()
{
    // Wait for driver to initialise if neccessary
    WaitForDriverCompletion();

    auto pAction = Action->getSwitch();
    auto pCommit = Commit->getSwitch();

    // Select the required action
    if (CLEAR != pAction->findOnSwitchIndex())
    {
        // Request Clear mode
        pAction->reset();
        pAction->at(CLEAR)->setState(ISS_ON);
        SetDriverBusy();
        BaseClient->sendNewSwitch(pAction);
        WaitForDriverCompletion();
        if (IPS_OK != pAction->getState())
        {
            IDLog("ClearSyncPoints - Bad Action switch state %s\n", pAction->getStateAsString());
            return false;
        }
    }

    pCommit->reset();
    pCommit->at(0)->setState(ISS_ON);
    SetDriverBusy();
    BaseClient->sendNewSwitch(pCommit);
    WaitForDriverCompletion();
    if (IPS_OK != pCommit->getState())
    {
        IDLog("ClearSyncPoints - Bad Commit switch state %s\n", pCommit->getStateAsString());
        return false;
    }

    return true;
}

bool ClientAPIForAlignmentDatabase::DeleteSyncPoint(unsigned int Offset)
{
    // Wait for driver to initialise if neccessary
    WaitForDriverCompletion();

    auto pAction       = Action->getSwitch();
    auto pCurrentEntry = CurrentEntry->getNumber();
    auto pCommit       = Commit->getSwitch();

    // Select the required action
    if (DELETE != pAction->findOnSwitchIndex())
    {
        // Request Delete mode
        pAction->reset();
        pAction->at(DELETE)->setState(ISS_ON);
        SetDriverBusy();
        BaseClient->sendNewSwitch(pAction);
        WaitForDriverCompletion();
        if (IPS_OK != pAction->getState())
        {
            IDLog("DeleteSyncPoint - Bad Action switch state %s\n", pAction->getStateAsString());
            return false;
        }
    }

    // Send the offset
    pCurrentEntry->at(0)->setValue(Offset);
    SetDriverBusy();
    BaseClient->sendNewNumber(pCurrentEntry);
    WaitForDriverCompletion();
    if (IPS_OK != pCurrentEntry->getState())
    {
        IDLog("DeleteSyncPoint - Bad Current Entry state %s\n", pCurrentEntry->getStateAsString());
        return false;
    }

    // Commit the entry to the database
    pCommit->reset();
    pCommit->at(0)->setState(ISS_ON);
    SetDriverBusy();
    BaseClient->sendNewSwitch(pCommit);
    WaitForDriverCompletion();
    if (IPS_OK != pCommit->getState())
    {
        IDLog("DeleteSyncPoint - Bad Commit switch state %s\n", pCommit->getStateAsString());
        return false;
    }

    return true;
}

bool ClientAPIForAlignmentDatabase::EditSyncPoint(unsigned int Offset, const AlignmentDatabaseEntry &CurrentValues)
{
    // Wait for driver to initialise if neccessary
    WaitForDriverCompletion();

    auto pAction           = Action->getSwitch();
    auto pCurrentEntry     = CurrentEntry->getNumber();
    auto pCommit           = Commit->getSwitch();

    // Select the required action
    if (EDIT != pAction->findOnSwitchIndex())
    {
        // Request Edit mode
        pAction->reset();
        pAction->at(EDIT)->setState(ISS_ON);
        SetDriverBusy();
        BaseClient->sendNewSwitch(pAction);
        WaitForDriverCompletion();
        if (IPS_OK != pAction->getState())
        {
            IDLog("EditSyncPoint - Bad Action switch state %s\n", pAction->getStateAsString());
            return false;
        }
    }

    // Send the offset
    pCurrentEntry->at(0)->setValue(Offset);
    SetDriverBusy();
    BaseClient->sendNewNumber(pCurrentEntry);
    WaitForDriverCompletion();
    if (IPS_OK != pCurrentEntry->getState())
    {
        IDLog("EditSyncPoint - Bad Current Entry state %s\n", pCurrentEntry->getStateAsString());
        return false;
    }

    if (!SendEntryData(CurrentValues))
        return false;

    // Commit the entry to the database
    pCommit->reset();
    pCommit->at(0)->setState(ISS_ON);
    SetDriverBusy();
    BaseClient->sendNewSwitch(pCommit);
    WaitForDriverCompletion();
    if (IPS_OK != pCommit->getState())
    {
        IDLog("EditSyncPoint - Bad Commit switch state %s\n", pCommit->getStateAsString());
        return false;
    }

    return true;
}

int ClientAPIForAlignmentDatabase::GetDatabaseSize()
{
    return 0;
}

void ClientAPIForAlignmentDatabase::Initialise(INDI::BaseClient *BaseClient)
{
    ClientAPIForAlignmentDatabase::BaseClient = BaseClient;
}

bool ClientAPIForAlignmentDatabase::InsertSyncPoint(unsigned int Offset, const AlignmentDatabaseEntry &CurrentValues)
{
    // Wait for driver to initialise if neccessary
    WaitForDriverCompletion();

    auto pAction           = Action->getSwitch();
    auto pCurrentEntry     = CurrentEntry->getNumber();
    auto pCommit           = Commit->getSwitch();

    // Select the required action
    if (INSERT != pAction->findOnSwitchIndex())
    {
        // Request Insert mode
        pAction->reset();
        pAction->at(INSERT)->setState(ISS_ON);
        SetDriverBusy();
        BaseClient->sendNewSwitch(pAction);
        WaitForDriverCompletion();
        if (IPS_OK != pAction->getState())
        {
            IDLog("InsertSyncPoint - Bad Action switch state %s\n", pAction->getStateAsString());
            return false;
        }
    }

    // Send the offset
    pCurrentEntry->at(0)->setValue(Offset);
    SetDriverBusy();
    BaseClient->sendNewNumber(pCurrentEntry);
    WaitForDriverCompletion();
    if (IPS_OK != pCurrentEntry->getState())
    {
        IDLog("InsertSyncPoint - Bad Current Entry state %s\n", pCurrentEntry->getStateAsString());
        return false;
    }

    if (!SendEntryData(CurrentValues))
        return false;

    // Commit the entry to the database
    pCommit->reset();
    pCommit->at(0)->setState(ISS_ON);
    SetDriverBusy();
    BaseClient->sendNewSwitch(pCommit);
    WaitForDriverCompletion();
    if (IPS_OK != pCommit->getState())
    {
        IDLog("InsertSyncPoint - Bad Commit switch state %s\n", pCommit->getStateAsString());
        return false;
    }

    return true;
}

bool ClientAPIForAlignmentDatabase::LoadDatabase()
{
    // Wait for driver to initialise if neccessary
    WaitForDriverCompletion();

    auto pAction = Action->getSwitch();
    auto pCommit = Commit->getSwitch();

    // Select the required action
    if (LOAD_DATABASE != pAction->findOnSwitchIndex())
    {
        // Request Load Database mode
        pAction->reset();
        pAction->at(LOAD_DATABASE)->setState(ISS_ON);
        SetDriverBusy();
        BaseClient->sendNewSwitch(pAction);
        WaitForDriverCompletion();
        if (IPS_OK != pAction->getState())
        {
            IDLog("LoadDatabase - Bad Action switch state %s\n", pAction->getStateAsString());
            return false;
        }
    }

    // Commit the Load Database
    pCommit->reset();
    pCommit->at(0)->setState(ISS_ON);
    SetDriverBusy();
    BaseClient->sendNewSwitch(pCommit);
    WaitForDriverCompletion();
    if (IPS_OK != pCommit->getState())
    {
        IDLog("LoadDatabase - Bad Commit state %s\n", pCommit->getStateAsString());
        return false;
    }

    return true;
}

void ClientAPIForAlignmentDatabase::ProcessNewBLOB(IBLOB *BLOBPointer)
{
    if (strcmp(BLOBPointer->bvp->name, "ALIGNMENT_POINT_OPTIONAL_BINARY_BLOB") == 0)
    {
        if (IPS_BUSY != BLOBPointer->bvp->s)
        {
            auto pAction = Action->getSwitch();
            int Index    = pAction->findOnSwitchIndex();
            if ((READ != Index) && (READ_INCREMENT != Index))
                SignalDriverCompletion();
        }
    }
}

void ClientAPIForAlignmentDatabase::ProcessNewDevice(INDI::BaseDevice *DevicePointer)
{
    Device = DevicePointer;
}

void ClientAPIForAlignmentDatabase::ProcessNewNumber(INumberVectorProperty *NumberVectorProperty)
{
    if (strcmp(NumberVectorProperty->name, "ALIGNMENT_POINT_MANDATORY_NUMBERS") == 0)
    {
        if (IPS_BUSY != NumberVectorProperty->s)
        {
            auto pAction = Action->getSwitch();
            int Index                      = pAction->findOnSwitchIndex();
            if ((READ != Index) && (READ_INCREMENT != Index))
                SignalDriverCompletion();
        }
    }
    else if (strcmp(NumberVectorProperty->name, "ALIGNMENT_POINTSET_CURRENT_ENTRY") == 0)
    {
        if (IPS_BUSY != NumberVectorProperty->s)
        {
            auto pAction = Action->getSwitch();
            int Index                      = pAction->findOnSwitchIndex();
            if (READ_INCREMENT != Index)
                SignalDriverCompletion();
        }
    }
}

void ClientAPIForAlignmentDatabase::ProcessNewProperty(INDI::Property *PropertyPointer)
{
    bool GotOneOfMine = true;

    if (strcmp(PropertyPointer->getName(), "ALIGNMENT_POINT_MANDATORY_NUMBERS") == 0)
        MandatoryNumbers = PropertyPointer;
    else if (strcmp(PropertyPointer->getName(), "ALIGNMENT_POINT_OPTIONAL_BINARY_BLOB") == 0)
    {
        OptionalBinaryBlob = PropertyPointer;
        // Make sure the format string is set up
        OptionalBinaryBlob->getBLOB()->at(0)->setFormat("alignmentPrivateData");
    }
    else if (strcmp(PropertyPointer->getName(), "ALIGNMENT_POINTSET_SIZE") == 0)
        PointsetSize = PropertyPointer;
    else if (strcmp(PropertyPointer->getName(), "ALIGNMENT_POINTSET_CURRENT_ENTRY") == 0)
        CurrentEntry = PropertyPointer;
    else if (strcmp(PropertyPointer->getName(), "ALIGNMENT_POINTSET_ACTION") == 0)
        Action = PropertyPointer;
    else if (strcmp(PropertyPointer->getName(), "ALIGNMENT_POINTSET_COMMIT") == 0)
        Commit = PropertyPointer;
    else
        GotOneOfMine = false;

    // Tell the client when all the database proeprties have been set up
    if (GotOneOfMine && (nullptr != MandatoryNumbers) && (nullptr != OptionalBinaryBlob) && (nullptr != PointsetSize) &&
            (nullptr != CurrentEntry) && (nullptr != Action) && (nullptr != Commit))
    {
        // The DriverActionComplete state variable is initialised to false
        // So I need to call this to set it to true and signal anyone
        // waiting for the driver to initialise etc.
        SignalDriverCompletion();
    }
}

void ClientAPIForAlignmentDatabase::ProcessNewSwitch(ISwitchVectorProperty *SwitchVectorProperty)
{
    if (strcmp(SwitchVectorProperty->name, "ALIGNMENT_POINTSET_ACTION") == 0)
    {
        if (IPS_BUSY != SwitchVectorProperty->s)
            SignalDriverCompletion();
    }
    else if (strcmp(SwitchVectorProperty->name, "ALIGNMENT_POINTSET_COMMIT") == 0)
    {
        if (IPS_BUSY != SwitchVectorProperty->s)
            SignalDriverCompletion();
    }
}

bool ClientAPIForAlignmentDatabase::ReadIncrementSyncPoint(AlignmentDatabaseEntry &CurrentValues)
{
    // Wait for driver to initialise if neccessary
    WaitForDriverCompletion();

    auto pAction           = Action->getSwitch();
    auto pMandatoryNumbers = MandatoryNumbers->getNumber();
    auto pBLOB             = OptionalBinaryBlob->getBLOB();
    auto pCurrentEntry     = CurrentEntry->getNumber();
    auto pCommit           = Commit->getSwitch();

    // Select the required action
    if (READ_INCREMENT != pAction->findOnSwitchIndex())
    {
        // Request Read Increment mode
        pAction->reset();
        pAction->at(READ_INCREMENT)->setState(ISS_ON);
        SetDriverBusy();
        BaseClient->sendNewSwitch(pAction);
        WaitForDriverCompletion();
        if (IPS_OK != pAction->getState())
        {
            IDLog("ReadIncrementSyncPoint - Bad Action switch state %s\n", pAction->getStateAsString());
            return false;
        }
    }

    // Commit the read increment
    pCommit->reset();
    pCommit->at(0)->setState(ISS_ON);
    SetDriverBusy();
    BaseClient->sendNewSwitch(pCommit);
    WaitForDriverCompletion();
    if ((IPS_OK != pCommit->getState()) || (IPS_OK != pMandatoryNumbers->getState()) || (IPS_OK != pBLOB->getState()) ||
            (IPS_OK != pCurrentEntry->getState()))
    {
        IDLog("ReadIncrementSyncPoint - Bad Commit/Mandatory numbers/Blob/Current entry state %s %s %s %s\n",
              pCommit->getStateAsString(), pMandatoryNumbers->getStateAsString(), pBLOB->getStateAsString(), pCurrentEntry->getStateAsString());
        return false;
    }

    // Read the entry data
    CurrentValues.ObservationJulianDate = pMandatoryNumbers->at(ENTRY_OBSERVATION_JULIAN_DATE)->getValue();
    CurrentValues.RightAscension        = pMandatoryNumbers->at(ENTRY_RA)->getValue();
    CurrentValues.Declination           = pMandatoryNumbers->at(ENTRY_DEC)->getValue();
    CurrentValues.TelescopeDirection.x  = pMandatoryNumbers->at(ENTRY_VECTOR_X)->getValue();
    CurrentValues.TelescopeDirection.y  = pMandatoryNumbers->at(ENTRY_VECTOR_Y)->getValue();
    CurrentValues.TelescopeDirection.z  = pMandatoryNumbers->at(ENTRY_VECTOR_Z)->getValue();

    return true;
}

bool ClientAPIForAlignmentDatabase::ReadSyncPoint(unsigned int Offset, AlignmentDatabaseEntry &CurrentValues)
{
    // Wait for driver to initialise if neccessary
    WaitForDriverCompletion();

    auto pAction           = Action->getSwitch();
    auto pMandatoryNumbers = MandatoryNumbers->getNumber();
    auto pBLOB             = OptionalBinaryBlob->getBLOB();
    auto pCurrentEntry     = CurrentEntry->getNumber();
    auto pCommit           = Commit->getSwitch();

    // Select the required action
    if (READ != pAction->findOnSwitchIndex())
    {
        // Request Read mode
        pAction->reset();
        pAction->at(READ)->setState(ISS_ON);
        SetDriverBusy();
        BaseClient->sendNewSwitch(pAction);
        WaitForDriverCompletion();
        if (IPS_OK != pAction->getState())
        {
            IDLog("ReadSyncPoint - Bad Action switch state %s\n", pAction->getStateAsString());
            return false;
        }
    }

    // Send the offset
    pCurrentEntry->at(0)->setValue(Offset);
    SetDriverBusy();
    BaseClient->sendNewNumber(pCurrentEntry);
    WaitForDriverCompletion();
    if (IPS_OK != pCurrentEntry->getState())
    {
        IDLog("ReadSyncPoint - Bad Current Entry state %s\n", pCurrentEntry->getStateAsString());
        return false;
    }

    // Commit the read
    pCommit->reset();
    pCommit->at(0)->setState(ISS_ON);
    SetDriverBusy();
    BaseClient->sendNewSwitch(pCommit);
    WaitForDriverCompletion();
    if ((IPS_OK != pCommit->getState()) || (IPS_OK != pMandatoryNumbers->getState()) || (IPS_OK != pBLOB->getState()))
    {
        IDLog("ReadSyncPoint - Bad Commit/Mandatory numbers/Blob state %s %s %s\n", pCommit->getStateAsString(),
              pMandatoryNumbers->getStateAsString(), pBLOB->getStateAsString());
        return false;
    }

    // Read the entry data
    CurrentValues.ObservationJulianDate = pMandatoryNumbers->at(ENTRY_OBSERVATION_JULIAN_DATE)->getValue();
    CurrentValues.RightAscension        = pMandatoryNumbers->at(ENTRY_RA)->getValue();
    CurrentValues.Declination           = pMandatoryNumbers->at(ENTRY_DEC)->getValue();
    CurrentValues.TelescopeDirection.x  = pMandatoryNumbers->at(ENTRY_VECTOR_X)->getValue();
    CurrentValues.TelescopeDirection.y  = pMandatoryNumbers->at(ENTRY_VECTOR_Y)->getValue();
    CurrentValues.TelescopeDirection.z  = pMandatoryNumbers->at(ENTRY_VECTOR_Z)->getValue();

    return true;
}

bool ClientAPIForAlignmentDatabase::SaveDatabase()
{
    // Wait for driver to initialise if neccessary
    WaitForDriverCompletion();

    auto pAction = Action->getSwitch();
    auto pCommit = Commit->getSwitch();

    // Select the required action
    if (SAVE_DATABASE != pAction->findOnSwitchIndex())
    {
        // Request Load Database mode
        pAction->reset();
        pAction->at(SAVE_DATABASE)->setState(ISS_ON);
        SetDriverBusy();
        BaseClient->sendNewSwitch(pAction);
        WaitForDriverCompletion();
        if (IPS_OK != pAction->getState())
        {
            IDLog("SaveDatabase - Bad Action switch state %s\n", pAction->getStateAsString());
            return false;
        }
    }

    // Commit the Save Database
    pCommit->reset();
    pCommit->at(0)->setState(ISS_ON);
    SetDriverBusy();
    BaseClient->sendNewSwitch(pCommit);
    WaitForDriverCompletion();
    if (IPS_OK != pCommit->getState())
    {
        IDLog("Save Database - Bad Commit state %s\n", pCommit->getStateAsString());
        return false;
    }

    return true;
}

// Private methods

bool ClientAPIForAlignmentDatabase::SendEntryData(const AlignmentDatabaseEntry &CurrentValues)
{
    auto pMandatoryNumbers = MandatoryNumbers->getNumber();
    auto pBLOB              = OptionalBinaryBlob->getBLOB();
    // Send the entry data
    pMandatoryNumbers->at(ENTRY_OBSERVATION_JULIAN_DATE)->setValue(CurrentValues.ObservationJulianDate);
    pMandatoryNumbers->at(ENTRY_RA)->setValue(CurrentValues.RightAscension);
    pMandatoryNumbers->at(ENTRY_DEC)->setValue(CurrentValues.Declination);
    pMandatoryNumbers->at(ENTRY_VECTOR_X)->setValue(CurrentValues.TelescopeDirection.x);
    pMandatoryNumbers->at(ENTRY_VECTOR_Y)->setValue(CurrentValues.TelescopeDirection.y);
    pMandatoryNumbers->at(ENTRY_VECTOR_Z)->setValue(CurrentValues.TelescopeDirection.z);
    SetDriverBusy();
    BaseClient->sendNewNumber(pMandatoryNumbers);
    WaitForDriverCompletion();
    if (IPS_OK != pMandatoryNumbers->getState())
    {
        IDLog("SendEntryData - Bad mandatory numbers state %s\n", pMandatoryNumbers->getStateAsString());
        return false;
    }

    if ((0 != CurrentValues.PrivateDataSize) && (nullptr != CurrentValues.PrivateData.get()))
    {
        // I have a BLOB to send
        SetDriverBusy();
        BaseClient->startBlob(Device->getDeviceName(), pBLOB->getName(), indi_timestamp());
        BaseClient->sendOneBlob(pBLOB->at(0)->getName(), CurrentValues.PrivateDataSize, pBLOB->at(0)->getFormat(),
                                CurrentValues.PrivateData.get());
        BaseClient->finishBlob();
        WaitForDriverCompletion();
        if (IPS_OK != pBLOB->getState())
        {
            IDLog("SendEntryData - Bad BLOB state %s\n", pBLOB->getStateAsString());
            return false;
        }
    }
    return true;
}

bool ClientAPIForAlignmentDatabase::SetDriverBusy()
{
    int ReturnCode = pthread_mutex_lock(&DriverActionCompleteMutex);

    if (ReturnCode != 0)
        return false;
    DriverActionComplete = false;
    IDLog("SetDriverBusy\n");
    ReturnCode = pthread_mutex_unlock(&DriverActionCompleteMutex);
    return ReturnCode == 0;
}

bool ClientAPIForAlignmentDatabase::SignalDriverCompletion()
{
    int ReturnCode = pthread_mutex_lock(&DriverActionCompleteMutex);

    if (ReturnCode != 0)
        return false;
    DriverActionComplete = true;
    ReturnCode           = pthread_cond_signal(&DriverActionCompleteCondition);
    if (ReturnCode != 0)
    {
        ReturnCode = pthread_mutex_unlock(&DriverActionCompleteMutex);
        return false;
    }
    IDLog("SignalDriverCompletion\n");
    ReturnCode = pthread_mutex_unlock(&DriverActionCompleteMutex);
    return ReturnCode == 0;
}

bool ClientAPIForAlignmentDatabase::WaitForDriverCompletion()
{
    int ReturnCode = pthread_mutex_lock(&DriverActionCompleteMutex);

    while (!DriverActionComplete)
    {
        IDLog("WaitForDriverCompletion - Waiting\n");
        ReturnCode = pthread_cond_wait(&DriverActionCompleteCondition, &DriverActionCompleteMutex);
        IDLog("WaitForDriverCompletion - Back from wait ReturnCode = %d\n", ReturnCode);
        if (ReturnCode != 0)
        {
            ReturnCode = pthread_mutex_unlock(&DriverActionCompleteMutex);
            return false;
        }
    }
    IDLog("WaitForDriverCompletion - Finished waiting\n");
    ReturnCode = pthread_mutex_unlock(&DriverActionCompleteMutex);
    return ReturnCode == 0;
}

} // namespace AlignmentSubsystem
} // namespace INDI
