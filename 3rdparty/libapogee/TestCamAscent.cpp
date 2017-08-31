/*! 
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/.
*
* Copyright(c) 2010 Apogee Instruments, Inc. 
* \class TestCamAscent 
* \brief Test object for the Ascent camera line to be used with the Apex application 
* 
*/ 

#include "TestCamAscent.h" 
#include "CameraIo.h" 
#include "ApnCamData.h"
#include "CamGen2CcdAcqParams.h" 
#include "CamGen2ModeFsm.h" 
#include "TestCamHelpers.h" 
#include "AscentBasedIo.h" 
#include "apgHelper.h" 

//////////////////////////// 
// CTOR 
TestCamAscent::TestCamAscent(const std::string & ioType,
                         const std::string & DeviceAddr) :
                         Ascent(),
                          m_fileName( __FILE__ ),
                          m_GetImgTime( 0.0 )
{
    CreateCamIo( ioType, DeviceAddr );
}

//////////////////////////// 
// DTOR 
TestCamAscent::~TestCamAscent() 
{ 

} 


//////////////////////////// 
// CFG      CAM       FROM  ID
void TestCamAscent::CfgCamFromId( uint16_t CameraId )
{
     //create and set the camera's cfg data
    DefaultCfgCamFromId( CameraId );

    UpdateCam();

    Init();
}

//////////////////////////// 
// CFG      CAM       FROM  FILE
void TestCamAscent::CfgCamFromFile( const std::string & path,
           const std::string & cfgFileName )
{
    m_CamCfgData = TestCamHelpers::CreateCApnCamDataFromFile(
        path, cfgFileName);

    UpdateCam();

    Init();
}

//////////////////////////// 
// CFG      CAM       FROM     INI 
void TestCamAscent::CfgCamFromIni( const std::string & input )
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
// UPDATE      CAM
void TestCamAscent::UpdateCam()
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
std::string TestCamAscent::GetFirmwareHdr()
{
    return m_CamIo->GetFirmwareHdr();
}

//////////////////////////// 
//      SET     SERIAL       NUMBER
void TestCamAscent::SetSerialNumber(const std::string & num)
{
    m_CamIo->SetSerialNumber( num );
}

//////////////////////////// 
// RUN  FIFO    TEST
std::vector<uint16_t>  TestCamAscent::RunFifoTest(const uint16_t rows,
        const uint16_t cols, const uint16_t speed)
{
    return TestCamHelpers::RunFifoTest( m_CamIo,rows, cols, speed, m_GetImgTime);
}

//////////////////////////// 
// RUN ADS TEST
std::vector<uint16_t>  TestCamAscent::RunAdsTest(const uint16_t rows,
        const uint16_t cols)
{
    return TestCamHelpers::RunAdsTest( this, m_CcdAcqSettings, rows, cols, m_GetImgTime);
}

//////////////////////////// 
//      PROGRAM     ASCENT
void TestCamAscent::ProgramAscent(const std::string & FilenameFpga,
    const std::string & FilenameFx2, const std::string & FilenameDescriptor,
     bool Print2StdOut)
{
    std::tr1::dynamic_pointer_cast<AscentBasedIo>(m_CamIo)->Program(FilenameFpga,
        FilenameFx2, FilenameDescriptor, Print2StdOut);
}


//////////////////////////// 
// READ       BUFCON          REG 
uint8_t TestCamAscent::ReadBufConReg( uint16_t reg )
{
    return m_CamIo->ReadBufConReg( reg ) ;
}

//////////////////////////// 
// WRITE       BUFCON          REG 
void TestCamAscent::WriteBufConReg( uint16_t reg, uint8_t val )
{
    m_CamIo->WriteBufConReg( reg, val );
}


//////////////////////////// 
// READ       FX2          REG 
uint8_t TestCamAscent::ReadFx2Reg( uint16_t reg )
{
    return m_CamIo->ReadFx2Reg( reg ) ;
}

//////////////////////////// 
// WRITE       FX2          REG 
void TestCamAscent::WriteFx2Reg( uint16_t reg, uint8_t val )
{
    m_CamIo->WriteFx2Reg( reg, val );
}


//////////////////////////// 
//  GET        CAM        INFO 
CamInfo::StrDb TestCamAscent::GetCamInfo()
{
    return std::tr1::dynamic_pointer_cast<AscentBasedIo>(m_CamIo)->ReadStrDatabase();
}
       
//////////////////////////// 
//  SET        CAM        INFO 
void TestCamAscent::SetCamInfo( CamInfo::StrDb & info )
{
        return std::tr1::dynamic_pointer_cast<AscentBasedIo>(m_CamIo)->WriteStrDatabase( info );
}
