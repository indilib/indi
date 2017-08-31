/*! 
* 
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/.
* 
* Copyright(c) 2012 Apogee Imaging Systems, Inc. 
* 
* \class TestCamAltaF 
* \brief test cam for INI mode in maxim and maybe apex 
* 
*/ 

#include "TestCamAltaF.h" 
#include "CameraIo.h" 
#include "ApnCamData.h"
#include "CamGen2CcdAcqParams.h" 
#include "CamGen2ModeFsm.h" 
#include "TestCamHelpers.h" 
#include "AscentBasedIo.h" 
#include "apgHelper.h" 

//////////////////////////// 
// CTOR 
TestCamAltaF::TestCamAltaF(const std::string & ioType,
                         const std::string & DeviceAddr) :
                         AltaF(),
                          m_fileName( __FILE__ ),
                          m_GetImgTime( 0.0 )
{
    CreateCamIo( ioType, DeviceAddr );
}

//////////////////////////// 
// DTOR 
TestCamAltaF::~TestCamAltaF() 
{ 

} 


//////////////////////////// 
// CFG      CAM       FROM  ID
void TestCamAltaF::CfgCamFromId( uint16_t CameraId )
{
     //create and set the camera's cfg data
    DefaultCfgCamFromId( CameraId );

    UpdateCam();

    Init();
}

//////////////////////////// 
// CFG      CAM       FROM  FILE
void TestCamAltaF::CfgCamFromFile( const std::string & path,
           const std::string & cfgFileName )
{
    m_CamCfgData = TestCamHelpers::CreateCApnCamDataFromFile(
        path, cfgFileName);

    UpdateCam();

    Init();
}

//////////////////////////// 
// CFG      CAM       FROM     INI 
void TestCamAltaF::CfgCamFromIni( const std::string & input )
{
    CApnCamData cfgData;

    try
    {
        cfgData = TestCamHelpers::MkMetaDataFromIni( input );
    }
    catch( std::runtime_error & err )
    {
        // log what failed for easier debugging with tech guys
        apgHelper::LogErrorMsg( m_fileName, err.what(), __LINE__ );
        //re-throw exception
        throw;
    }

    m_CamCfgData  = std::tr1::shared_ptr<CApnCamData>( 
        new CApnCamData( cfgData ) );

    
    UpdateCam();

    //write the id to the camera
    m_Id = m_CamCfgData->m_MetaData.CameraId;
    WriteId2CamReg();
}

//////////////////////////// 
// UPDATE      CAM
void TestCamAltaF::UpdateCam()
{
    //read and set the firmware rev
    //doing this here for when we create the ModeFsm
    //objects during CCD adc initalization
    m_FirmwareVersion = m_CamIo->GetFirmwareRev();
    
    m_CcdAcqSettings = std::tr1::shared_ptr<CcdAcqParams>( 
        new CamGen2CcdAcqParams(m_CamCfgData, m_CamIo,m_CameraConsts) );

     //create the ModeFSM object so it can be used in the camera's 
    //Init function
    m_CamMode = std::tr1::shared_ptr<ModeFsm>( new CamGen2ModeFsm(m_CamIo,
        m_CamCfgData, m_FirmwareVersion) );

}

//////////////////////////// 
// GET     FIRMWARE    HDR
std::string TestCamAltaF::GetFirmwareHdr()
{
    return m_CamIo->GetFirmwareHdr();
}

//////////////////////////// 
//      SET     SERIAL       NUMBER
void TestCamAltaF::SetSerialNumber(const std::string & num)
{
    m_CamIo->SetSerialNumber( num );
}

//////////////////////////// 
// RUN  FIFO    TEST
std::vector<uint16_t>  TestCamAltaF::RunFifoTest(const uint16_t rows,
        const uint16_t cols, const uint16_t speed)
{
    return TestCamHelpers::RunFifoTest( m_CamIo,rows, cols, speed, m_GetImgTime);
}

//////////////////////////// 
// RUN ADS TEST
std::vector<uint16_t>  TestCamAltaF::RunAdsTest(const uint16_t rows,
        const uint16_t cols)
{
    return TestCamHelpers::RunAdsTest( this, m_CcdAcqSettings, rows, cols, m_GetImgTime);
}

//////////////////////////// 
// READ       BUFCON          REG 
uint8_t TestCamAltaF::ReadBufConReg( uint16_t reg )
{
    return m_CamIo->ReadBufConReg( reg ) ;
}

//////////////////////////// 
// WRITE       BUFCON          REG 
void TestCamAltaF::WriteBufConReg( uint16_t reg, uint8_t val )
{
    m_CamIo->WriteBufConReg( reg, val );
}


//////////////////////////// 
// READ       FX2          REG 
uint8_t TestCamAltaF::ReadFx2Reg( uint16_t reg )
{
    return m_CamIo->ReadFx2Reg( reg ) ;
}

//////////////////////////// 
// WRITE       FX2          REG 
void TestCamAltaF::WriteFx2Reg( uint16_t reg, uint8_t val )
{
    m_CamIo->WriteFx2Reg( reg, val );
}


//////////////////////////// 
//  GET        CAM        INFO 
CamInfo::StrDb TestCamAltaF::GetCamInfo()
{
    return std::tr1::dynamic_pointer_cast<AscentBasedIo>(m_CamIo)->ReadStrDatabase();
}
       
//////////////////////////// 
//  SET        CAM        INFO 
void TestCamAltaF::SetCamInfo( CamInfo::StrDb & info )
{
        return std::tr1::dynamic_pointer_cast<AscentBasedIo>(m_CamIo)->WriteStrDatabase( info );
}

//////////////////////////// 
//      PROGRAM     ALTAF
void TestCamAltaF::ProgramAltaF(const std::string & FilenameFpga,
    const std::string & FilenameFx2, const std::string & FilenameDescriptor,
     bool Print2StdOut)
{
    std::tr1::dynamic_pointer_cast<AscentBasedIo>(m_CamIo)->Program(FilenameFpga,
        FilenameFx2, FilenameDescriptor, Print2StdOut);
}
