/*! 
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/.
*
* Copyright(c) 2011 Apogee Imaging Systems, Inc. 
* \class TestCamHiC
* \brief object created ICamera, it handle ini initalization 
* 
*/ 

#include "TestCamHiC.h" 
#include "TestCamHelpers.h" 
#include "CamGen2CcdAcqParams.h" 
#include "CamGen2ModeFsm.h" 
#include "CameraIo.h" 
#include "AscentBasedIo.h" 
#include "apgHelper.h" 

//////////////////////////// 
// CTOR 
TestCamHiC::TestCamHiC(const std::string & ioType,
                         const std::string & DeviceAddr) :
                         HiC(),
                          m_fileName( __FILE__ ),
                          m_GetImgTime( 0.0 )
{
    CreateCamIo( ioType, DeviceAddr );
}

//////////////////////////// 
// DTOR 
TestCamHiC::~TestCamHiC() 
{ 

} 

//////////////////////////// 
// CFG      CAM       FROM  FILE
void TestCamHiC::CfgCamFromFile( const std::string & path,
           const std::string & cfgFileName )
{
    m_CamCfgData = TestCamHelpers::CreateCApnCamDataFromFile(
        path, cfgFileName);

    UpdateCam();

    Init();
}

//////////////////////////// 
// CFG      CAM       FROM     INI 
void TestCamHiC::CfgCamFromIni( const std::string & input )
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
// CFG      CAM       FROM  ID
void TestCamHiC::CfgCamFromId( uint16_t CameraId )
{
     //create and set the camera's cfg data
    DefaultCfgCamFromId( CameraId );

    UpdateCam();

    Init();
}

//////////////////////////// 
// UPDATE      CAM
void TestCamHiC::UpdateCam()
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
std::string TestCamHiC::GetFirmwareHdr()
{
    return m_CamIo->GetFirmwareHdr();
}

//////////////////////////// 
//      PROGRAM     QUAD
void TestCamHiC::ProgramHiC(const std::string & FilenameFpga,
    const std::string & FilenameFx2, const std::string & FilenameDescriptor,
     bool Print2StdOut)
{
    std::tr1::dynamic_pointer_cast<AscentBasedIo>(m_CamIo)->Program(FilenameFpga,
        FilenameFx2, FilenameDescriptor, Print2StdOut);
}


//////////////////////////// 
// READ       BUFCON          REG 
uint8_t TestCamHiC::ReadBufConReg( uint16_t reg )
{
    return m_CamIo->ReadBufConReg( reg ) ;
}

//////////////////////////// 
// WRITE       BUFCON          REG 
void TestCamHiC::WriteBufConReg( uint16_t reg, uint8_t val )
{
    m_CamIo->WriteBufConReg( reg, val );
}


//////////////////////////// 
// READ       FX2          REG 
uint8_t TestCamHiC::ReadFx2Reg( uint16_t reg )
{
    return m_CamIo->ReadFx2Reg( reg ) ;
}

//////////////////////////// 
// WRITE       FX2          REG 
void TestCamHiC::WriteFx2Reg( uint16_t reg, uint8_t val )
{
    m_CamIo->WriteFx2Reg( reg, val );
}


//////////////////////////// 
//      SET     SERIAL       NUMBER
void TestCamHiC::SetSerialNumber(const std::string & num)
{
    m_CamIo->SetSerialNumber( num );
}


//////////////////////////// 
// RUN  FIFO    TEST
std::vector<uint16_t>  TestCamHiC::RunFifoTest(const uint16_t rows,
        const uint16_t cols, const uint16_t speed)
{
    return TestCamHelpers::RunFifoTest( m_CamIo,rows, cols, speed, m_GetImgTime);
}

//////////////////////////// 
// RUN ADS TEST
std::vector<uint16_t>  TestCamHiC::RunAdsTest(const uint16_t rows,
        const uint16_t cols)
{
    return TestCamHelpers::RunAdsTest( this, m_CcdAcqSettings, rows, cols, m_GetImgTime);
}
