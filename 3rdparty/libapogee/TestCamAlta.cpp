/*! 
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/.
*
* Copyright(c) 2010 Apogee Instruments, Inc. 
* \class TestCamAlta 
* \brief Test camera object for controlling alta cameras 
* 
*/ 

#include "TestCamAlta.h" 
#include "CameraIo.h" 
#include "ApnCamData.h"
#include "AltaCcdAcqParams.h" 
#include "AltaModeFsm.h" 
#include "TestCamHelpers.h" 
#include "AltaIo.h" 
#include "apgHelper.h" 

//////////////////////////// 
// CTOR 
TestCamAlta::TestCamAlta(const std::string & ioType,
                         const std::string & DeviceAddr) :
                         Alta(),
                          m_fileName( __FILE__ ),
                          m_GetImgTime( 0.0 )

{
    CreateCamIo( ioType, DeviceAddr );

    if( CamModel::ETHERNET == m_CamIo->GetInterfaceType() )
    {
        m_PlatformType = CamModel::ALTAE;
    }

}

//////////////////////////// 
// DTOR 
TestCamAlta::~TestCamAlta() 
{ 

} 

//////////////////////////// 
// CFG      CAM       FROM  ID
void TestCamAlta::CfgCamFromId( uint16_t CameraId )
{
     //create and set the camera's cfg data
    DefaultCfgCamFromId( CameraId );

    UpdateCam();

    Init();
}

//////////////////////////// 
// CFG      CAM       FROM  FILE
void TestCamAlta::CfgCamFromFile( const std::string & path,
           const std::string & cfgFileName )
{
    m_CamCfgData = TestCamHelpers::CreateCApnCamDataFromFile(
        path, cfgFileName);

    UpdateCam();

    Init();
}

//////////////////////////// 
// CFG      CAM       FROM     INI 
void TestCamAlta::CfgCamFromIni( const std::string & input )
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
}

//////////////////////////// 
// UPDATE     CAM
void TestCamAlta::UpdateCam()
{
    //read and set the firmware rev
    //doing this here for when we create the ModeFsm
    //objects during CCD adc initalization
    m_FirmwareVersion = m_CamIo->GetFirmwareRev();
    
    m_CcdAcqSettings = std::tr1::shared_ptr<CcdAcqParams>( 
        new AltaCcdAcqParams(m_CamCfgData, m_CamIo,m_CameraConsts) );

     //create the ModeFSM object so it can be used in the camera's 
    //Init function
    m_CamMode = std::tr1::shared_ptr<ModeFsm>( new AltaModeFsm(m_CamIo,
        m_CamCfgData, m_FirmwareVersion) );

    //if this is a ethernet set up bulk seq
     if( CamModel::ETHERNET == m_CamIo->GetInterfaceType() )
     {
         m_CamMode->SetBulkDownload( true );
     }
  
}

//////////////////////////// 
// GET     FIRMWARE    HDR
std::string TestCamAlta::GetFirmwareHdr()
{
    return m_CamIo->GetFirmwareHdr();
}

//////////////////////////// 
//      SET     SERIAL       NUMBER
void TestCamAlta::SetSerialNumber(const std::string & num)
{
   m_CamIo->SetSerialNumber( num );
}

//////////////////////////// 
//      PROGRAM     ALTA
void TestCamAlta::ProgramAlta(const std::string & FilenameCamCon,
            const std::string & FilenameBufCon, const std::string & FilenameFx2,
            const std::string & FilenameGpifCamCon,const std::string & FilenameGpifBufCon,
            const std::string & FilenameGpifFifo, bool Print2StdOut)
{
    std::tr1::dynamic_pointer_cast<AltaIo>(m_CamIo)->Program(FilenameCamCon,
        FilenameBufCon, FilenameFx2, FilenameGpifCamCon,
        FilenameGpifBufCon, FilenameGpifFifo,Print2StdOut);
}

//////////////////////////// 
// RUN  FIFO    TEST
std::vector<uint16_t>  TestCamAlta::RunFifoTest(const uint16_t rows,
        const uint16_t cols, const uint16_t speed)
{
    return TestCamHelpers::RunFifoTest( m_CamIo, rows, cols, speed, m_GetImgTime);
}

//////////////////////////// 
// RUN ADS TEST
std::vector<uint16_t>  TestCamAlta::RunAdsTest(const uint16_t rows,
        const uint16_t cols)
{
   
    return TestCamHelpers::RunAdsTest( this, m_CcdAcqSettings, rows, cols, m_GetImgTime);
}


//////////////////////////// 
// READ       BUFCON          REG 
uint8_t TestCamAlta::ReadBufConReg( uint16_t reg )
{
    return m_CamIo->ReadBufConReg( reg ) ;
}

//////////////////////////// 
// WRITE       BUFCON          REG 
void TestCamAlta::WriteBufConReg( uint16_t reg, uint8_t val )
{
    m_CamIo->WriteBufConReg( reg, val );
}

//////////////////////////// 
// READ       FX2          REG 
uint8_t TestCamAlta::ReadFx2Reg( uint16_t reg )
{
    return m_CamIo->ReadFx2Reg( reg ) ;
}

//////////////////////////// 
// WRITE       FX2          REG 
void TestCamAlta::WriteFx2Reg( uint16_t reg, uint8_t val )
{
    m_CamIo->WriteFx2Reg( reg, val );
}
