/*! 
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/.
*
* Copyright(c) 2010 Apogee Instruments, Inc. 
* \class TestCamHelpers 
* \brief namespace that contains functions that are useful for the apex application 
* 
*/ 

#include "TestCamHelpers.h" 
#include "helpers.h" 
#include "parseCfgTabDelim.h"
#include "CamCfgMatrix.h" 
#include "CameraIo.h" 
#include "CamHelpers.h" 
#include "CameraStatusRegs.h" 
#include "apgHelper.h" 
#include "ApogeeCam.h" 
#include "CcdAcqParams.h" 
#include "CameraInfo.h" 
#include "ApgTimer.h" 
#include "versionNo.h" 

#include <sstream>
#include <algorithm>


#include <atlstr.h>


//////////////////////////// 
// GET   LIST  OF    IDS
std::vector<uint16_t> TestCamHelpers::GetListOfIds()
{
     std::string cfgName = help::FixPath( apgHelper::GetCamCfgDir() ) 
         + apgHelper::GetCfgFileName();
    std::vector<std::tr1::shared_ptr<CamCfg::APN_CAMERA_METADATA> > meta;
    parseCfgTabDelim::FetchMetaData( cfgName, meta );
    std::vector<uint16_t> result;

    std::vector<std::tr1::shared_ptr<CamCfg::APN_CAMERA_METADATA> >::iterator itr;
    for( itr = meta.begin(); itr != meta.end(); ++itr )
    {
        result.push_back( (*itr)->CameraId );
    }

    std::sort( result.begin(), result.end() );

    return result;
}

//////////////////////////// 
// GET   LIST  OF    IDS
std::string TestCamHelpers::GetSoftwareVersions()
{
    //TODO - figure out how to get other version
    //of the software versions
    //result += "Driver: " + + "\n";
    //result += "cURL: " + + "\n";
    
    std::stringstream ss;
    ss << "libapogee version: " << APOGEE_MAJOR_VERSION;
    ss << "." << APOGEE_MINOR_VERSION;
    ss << "." << APOGEE_PATCH_VERSION;

    return ss.str();
}

//////////////////////////// 
// MK       PATTERN      FILENAME
std::string TestCamHelpers::MkPatternFileName( const std::string & path, 
                                           const std::string & baseName)
{
    std::string result = help::FixPath( path ) + baseName + ".csv";
    return result;
}

//////////////////////////// 
// GET   LIST  OF    IDS
std::tr1::shared_ptr<CApnCamData> TestCamHelpers::CreateCApnCamDataFromFile( 
        const std::string & path, const std::string & cfgFileName )
{
    std::string fixedPath = help::FixPath(path);
    std::string fullFile = fixedPath + cfgFileName;
    std::vector<std::tr1::shared_ptr<CamCfg::APN_CAMERA_METADATA>> newMetaVect;

    parseCfgTabDelim::FetchMetaData( fullFile, newMetaVect );

    //fecth the first one, there should only be one...but...
    //cannot guarantee that.
    CamCfg::APN_CAMERA_METADATA theMeta = *newMetaVect.at(0);

    //create the new camera configuration data in the files in the path
    std::tr1::shared_ptr<CApnCamData> CamCfgData( 
        new CApnCamData( theMeta,
        parseCfgTabDelim::FetchVerticalPattern( MkPatternFileName( 
                fixedPath, theMeta.VerticalPattern) ),
        parseCfgTabDelim::FetchHorizontalPattern( MkPatternFileName( 
                fixedPath, theMeta.ClampPatternNormal ) ),
        parseCfgTabDelim::FetchHorizontalPattern( MkPatternFileName( 
                fixedPath, theMeta.SkipPatternNormal ) ),
        parseCfgTabDelim::FetchHorizontalPattern( MkPatternFileName( 
                fixedPath, theMeta.RoiPatternNormal ) ),
        parseCfgTabDelim::FetchHorizontalPattern( MkPatternFileName( 
                fixedPath, theMeta.ClampPatternFast ) ),
        parseCfgTabDelim::FetchHorizontalPattern( MkPatternFileName( 
                fixedPath, theMeta.SkipPatternFast ) ),
        parseCfgTabDelim::FetchHorizontalPattern( MkPatternFileName( 
                fixedPath, theMeta.RoiPatternFast ) ),
                parseCfgTabDelim::FetchVerticalPattern( MkPatternFileName( 
                fixedPath, theMeta.VerticalPatternVideo) ),
        parseCfgTabDelim::FetchHorizontalPattern( MkPatternFileName( 
                fixedPath, theMeta.ClampPatternVideo ) ),
        parseCfgTabDelim::FetchHorizontalPattern( MkPatternFileName( 
                fixedPath, theMeta.SkipPatternVideo ) ),
        parseCfgTabDelim::FetchHorizontalPattern( MkPatternFileName( 
                fixedPath, theMeta.RoiPatternVideo) ),
        parseCfgTabDelim::FetchHorizontalPattern( MkPatternFileName( 
                fixedPath, theMeta.ClampPatternNormalDual ) ),
        parseCfgTabDelim::FetchHorizontalPattern( MkPatternFileName( 
                fixedPath, theMeta.SkipPatternNormalDual ) ),
        parseCfgTabDelim::FetchHorizontalPattern( MkPatternFileName( 
                fixedPath, theMeta.RoiPatternNormalDual ) ),
        parseCfgTabDelim::FetchHorizontalPattern( MkPatternFileName( 
                fixedPath, theMeta.ClampPatternFastDual ) ),
        parseCfgTabDelim::FetchHorizontalPattern( MkPatternFileName( 
                fixedPath, theMeta.SkipPatternFastDual ) ),
        parseCfgTabDelim::FetchHorizontalPattern( MkPatternFileName( 
                fixedPath, theMeta.RoiPatternFastDual ) ) ) );

    return CamCfgData;
}


//////////////////////////// 
// RUN  FIFO    TEST
std::vector<uint16_t> TestCamHelpers::RunFifoTest( 
        std::tr1::shared_ptr<CameraIo> camIo,
        const uint16_t numRows, const uint16_t numCols, 
        const uint16_t speed, double & fifoGetImgTime)
{
     if( numRows == 0 || numCols == 0 || speed == 0)
    {
        std::string errStr("invalid input into fifo test function");
        apgHelper::throwRuntimeException(__FILE__, errStr, __LINE__,
            Apg::ErrorType_InvalidUsage);
    }

    const unsigned long numPixels = numRows*numCols;
    //prep camera 4 Fifo Test
    const uint16_t imgSzHigh = ( numPixels & 0xFFFF0000 ) >> 16;
    const uint16_t imgSzLow = ( numPixels & 0x0000FFFF );

    camIo->WriteReg(CameraRegs::TEST_COUNT_UPPER, imgSzHigh);
    camIo->WriteReg(CameraRegs::TEST_COUNT_LOWER, imgSzLow);
    //turn test bit on
    camIo->ReadOrWriteReg(CameraRegs::OP_A, CameraRegs::OP_A_TEST_MODE_BIT);
    camIo->WriteReg(CameraRegs::OP_B, speed);
    camIo->WriteReg(CameraRegs::IMAGE_COUNT,1); //always one image for this test


    CameraStatusRegs::AdvStatus statusAdv;
        memset(&statusAdv,0,sizeof(statusAdv));

    bool waiting = true;

    //tell the camera what to send us
    camIo->SetupImgXfer(numCols, numRows, 1, false);     

    //start the fifo test
    camIo->WriteReg(CameraRegs::CMD_A, CameraRegs::CMD_A_TEST_BIT);

    while( waiting )
    {
        camIo->GetStatus( statusAdv );

        if( statusAdv.DataAvailFlag )
        {
           break;
        }

        apgHelper::ApogeeSleep(50);
    }

    ApgTimer theTimer;
    theTimer.Start();

    std::vector<uint16_t> data(numPixels);
    camIo->GetImageData( data );

    theTimer.Stop();
	fifoGetImgTime	= theTimer.GetTimeInSec();
    
    if( data.size() != numPixels)
    {
        std::stringstream received;
        received << data.size() ;

        std::stringstream requested;
        requested << numPixels;

        std::string errMsg = "FIFO TEST ERROR - Requested " + requested.str() \
            + " pixels, but recieved " + received.str() + " pixels.";
        apgHelper::throwRuntimeException( __FILE__, errMsg, __LINE__,
            Apg::ErrorType_Serious );
    }

    //turn off test bit
    camIo->ReadAndWriteReg(CameraRegs::OP_A, 
        static_cast<uint16_t>(~CameraRegs::OP_A_TEST_MODE_BIT) );

    return data;
}


//////////////////////////// 
// RUN  ADS   TEST
std::vector<uint16_t> TestCamHelpers::RunAdsTest( 
        ApogeeCam * theCam,
        std::tr1::shared_ptr<CcdAcqParams> acq,
        const uint16_t numRows, 
        const uint16_t numCols, 
        double & fifoGetImgTime)
{
    //pre-condition checking
    if( numRows == 0 || numCols == 0 )
    {
        std::string errStr("invalid input into ads test function");
        apgHelper::throwRuntimeException( __FILE__, errStr, 
            __LINE__, Apg::ErrorType_InvalidUsage );
    }

    if( numCols > theCam->GetMaxImgCols() )
    {
        std::stringstream msg;
        msg << "Input number of columns, " << numCols;
        msg << ", greater than maximum number of columns, ";
        msg << theCam->GetMaxImgCols();

        apgHelper::throwRuntimeException( __FILE__, msg.str(), 
            __LINE__, Apg::ErrorType_InvalidUsage );
    }

    if( numRows > theCam->GetMaxImgRows() )
    {
        std::stringstream msg;
        msg << "Input number of rows, " << numRows;
        msg << ", greater than maximum number of rows, ";
        msg << theCam->GetMaxImgRows();

        apgHelper::throwRuntimeException( __FILE__, msg.str(), 
            __LINE__, Apg::ErrorType_InvalidUsage );
    }

    //setup imaging ROI
    //cols
    theCam->SetRoiStartCol( 0 );

    if( 1 != theCam->GetRoiBinCol() )
    {
        theCam->SetRoiBinCol( 1 );
    }
    
    theCam->SetRoiNumCols( numCols );

    //rows
    theCam->SetRoiStartRow( 0 );

    if( 1 != theCam->GetRoiBinRow() )
    {
        theCam->SetRoiBinRow( 1 );
    }

    theCam->SetRoiNumRows( numRows  );

    //only capture one image in ads sim mode
    theCam->SetImageCount(1);

    //turn on sim mode
    acq->SetAdsSimMode( true );

    //start the simulated exposure
    theCam->StartExposure(0.1, true);     

    //wait for the image
    bool waiting = true;

    while( waiting )
    {
        if( theCam->GetImagingStatus() == Apg::Status_ImageReady)
        {
           break;
        }

        apgHelper::ApogeeSleep(50);
    }

    ApgTimer theTimer;
    theTimer.Start();

    //fetch the data
    std::vector<uint16_t> data;
    theCam->GetImage( data );

	theTimer.Stop();
	fifoGetImgTime	= theTimer.GetTimeInSec();

    const unsigned long numPixels = numRows*numCols;
    if( data.size() != numPixels)
    {
        std::stringstream received;
        received << data.size() ;

        std::stringstream requested;
        requested << numPixels;

        std::string errMsg = "ADS TEST ERROR - Requested " + requested.str() \
            + " pixels, but recieved " + received.str() + " pixels.";
        apgHelper::throwRuntimeException( __FILE__, errMsg, __LINE__,
            Apg::ErrorType_Serious );
    }

    //turn off test bit
    acq->SetAdsSimMode( false );

    return data;
}



//--------------------------------------------------------------------------
//          APN_CONFIG_LOAD
// Load configuration data from a standard INI style data file.
CApnCamData TestCamHelpers::MkMetaDataFromIni( const std::string & iniName )
{

    USES_CONVERSION;
    CString iniFileName( iniName.c_str() );

    //fetch the data common to all camera lines
    CamCfg::APN_CAMERA_METADATA metaData;
    CamCfg::Clear( metaData );

    //some hardcode values
    metaData.CoolingSupported = true;
    metaData.RegulatedCoolingSupported = true;
    metaData.TempBackoffPoint = 2.0;
    metaData.TempSetPoint = -20.0;
    metaData.IRPreflashTime = 160;
    metaData.TempRampRateOne = 700;
    metaData.TempRampRateTwo = 4000;
    metaData.SensorTypeCCD = true;
    metaData.MinSuggestedExpTime = 10.0;
    metaData.RowOffsetBinning = 1;
    metaData.SupportsSingleDualReadoutSwitching = false;

    //values from ini file
    TCHAR iniStr[512];
    CString tempCStr;

    metaData.CameraId = static_cast<uint16_t>( GetPrivateProfileInt( _T( "system" ), 
        _T( "id" ), 0, iniFileName ) );

    GetPrivateProfileString( _T( "system" ), _T( "Sensor" ), _T( "test" ),
            iniStr, 511, iniFileName );
    metaData.Sensor.append( T2A(iniStr) );

    metaData.TotalColumns = static_cast<uint16_t>( GetPrivateProfileInt( _T( "system" ), 
        _T( "Total_Columns" ), 1024, iniFileName ) );

    metaData.TotalRows = static_cast<uint16_t>( GetPrivateProfileInt( _T( "system" ), 
        _T( "Total_Rows" ), 1024, iniFileName ) );

    metaData.ImagingColumns = static_cast<uint16_t>( GetPrivateProfileInt( _T( "system" ), 
        _T( "Imaging_Columns" ), 1024, iniFileName ) );

    metaData.ImagingRows = static_cast<uint16_t>( GetPrivateProfileInt( _T( "system" ), 
        _T( "Imaging_Rows" ), 1024, iniFileName ) );

    metaData.ClampColumns = static_cast<uint16_t>( GetPrivateProfileInt( _T( "system" ), 
        _T( "Clamp_Columns" ), 0, iniFileName ) );

    metaData.OverscanColumns = static_cast<uint16_t>( GetPrivateProfileInt( _T( "system" ), 
        _T( "Overscan_Columns" ), 0, iniFileName ) );

    metaData.PreRoiSkipColumns = static_cast<uint16_t>( GetPrivateProfileInt( _T( "system" ), 
        _T( "PreClampSkip_Columns" ), 0, iniFileName ) );

    metaData.PostRoiSkipColumns = static_cast<uint16_t>( GetPrivateProfileInt( _T( "system" ), 
        _T( "PostRoiSkip_Columns" ), 0, iniFileName ) );

    // TODO is this a no op?
    //GetPrivateProfileInt( _T( "system" ), _T( "PostOverscanSkip_Columns" ), 0, iniFileName );

    metaData.UnderscanRows = static_cast<uint16_t>( GetPrivateProfileInt( _T( "system" ), 
        _T( "Underscan_Rows" ), 0, iniFileName ) );

    metaData.OverscanRows = static_cast<uint16_t>( GetPrivateProfileInt( _T( "system" ), 
        _T( "Overscan_Rows" ), 0, iniFileName ) );

    metaData.NumAdOutputs = static_cast<uint16_t>( GetPrivateProfileInt( _T( "system" ), 
        _T( "num_ad_outputs" ), 1, iniFileName ) );

    if( 2 == metaData.NumAdOutputs )
    {
        metaData.SupportsSingleDualReadoutSwitching = true;
    }

    metaData.AmpCutoffDisable = static_cast<uint16_t>( GetPrivateProfileInt( _T( "system" ), 
        _T( "amp_cutoff_disable" ), 0, iniFileName ) );

    if( 1 == GetPrivateProfileInt( _T( "system" ), _T( "interline" ), 0, iniFileName ) )
    {
        metaData.InterlineCCD = true;
    }
    // TODO is this a no op?
    //GetPrivateProfileInt( _T( "system" ), _T( "HBin_Max" ), 0, iniFileName );

    // TODO is this a no op?
    //GetPrivateProfileInt( _T( "system" ), _T( "VBin_Max" ), 0, iniFileName );

    GetPrivateProfileString( _T( "system" ), _T( "Hflush_Disable" ), _T( "false" ),
            iniStr, 511, iniFileName );

    tempCStr = iniStr;

    if( 0 == tempCStr.CompareNoCase( _T("true") ) )
    {
        metaData.HFlushDisable = true;
    }
    else
    {
        metaData.HFlushDisable = false;
    }

    GetPrivateProfileString( _T( "system" ), _T( "vertical_pattern" ), _T( "vertical_pattern_noop" ),
            iniStr, 511, iniFileName );
    metaData.VerticalPattern.append( T2A(iniStr) );

    CamCfg::APN_VPATTERN_FILE vertPattern =
        parseCfgTabDelim::FetchVerticalPattern(  metaData.VerticalPattern );

    GetPrivateProfileString( _T( "system" ), _T( "vertical_video_pattern" ), _T( "vertical_video_pattern_noop" ),
            iniStr, 511, iniFileName );
    metaData.VerticalPatternVideo.append( T2A(iniStr) );

    CamCfg::APN_VPATTERN_FILE vertVideoPattern =
        parseCfgTabDelim::FetchVerticalPattern( metaData.VerticalPatternVideo );

    CamCfg::APN_HPATTERN_FILE skipNorm;
    GetHPattern( iniName, "Skip16_Pattern", "Skip16_Pattern_noop", 
        metaData.SkipPatternNormal, skipNorm );

    CamCfg::APN_HPATTERN_FILE clampNorm;
    GetHPattern( iniName, "Clamp16_Pattern", "Clamp16_Pattern_noop", 
        metaData.ClampPatternNormal, clampNorm );

    CamCfg::APN_HPATTERN_FILE roiNorm;
    GetHPattern( iniName, "Roi16_Pattern", "Roi16_Pattern_noop", 
        metaData.RoiPatternNormal, roiNorm );

    CamCfg::APN_HPATTERN_FILE skipFast;
    GetHPattern( iniName, "Skip12_Pattern", "Skip12_Pattern_noop", 
        metaData.SkipPatternFast, skipFast );

    CamCfg::APN_HPATTERN_FILE clampFast;
    GetHPattern( iniName, "Clamp12_Pattern", "Clamp12_Pattern_noop", 
        metaData.ClampPatternFast, clampFast );
     
     CamCfg::APN_HPATTERN_FILE roiFast;
     GetHPattern( iniName, "Roi12_Pattern", "Roi12_Pattern_noop", 
        metaData.RoiPatternFast, roiFast );   
 
    CamCfg::APN_HPATTERN_FILE skipVideo;
    GetHPattern( iniName, "Skip_video_Pattern", "Skip_video_Pattern_noop", 
        metaData.SkipPatternVideo, skipVideo ); 

    CamCfg::APN_HPATTERN_FILE clampVideo;
    GetHPattern( iniName, "Clamp_video_Pattern", "Clamp_video_Pattern_noop", 
        metaData.ClampPatternVideo, clampVideo );

    CamCfg::APN_HPATTERN_FILE roiVideo;
    GetHPattern( iniName, "Roi_video_Pattern", "Roi_video_Pattern_noop", 
        metaData.RoiPatternVideo, roiVideo );

    CamCfg::APN_HPATTERN_FILE skipNormDual;
    GetHPattern( iniName, "Skip16_Pattern", "Skip16_Pattern_Dual_noop", 
        metaData.SkipPatternNormalDual, skipNormDual );

    CamCfg::APN_HPATTERN_FILE clampNormDual;
    GetHPattern( iniName, "Clamp16_Pattern", "Clamp16_Pattern_Dual_noop", 
        metaData.ClampPatternNormalDual, clampNormDual );

    CamCfg::APN_HPATTERN_FILE roiNormDual;
    GetHPattern( iniName, "Roi16_Pattern", "Roi16_Pattern_Dual_noop", 
        metaData.RoiPatternNormalDual, roiNormDual );

    CamCfg::APN_HPATTERN_FILE skipFastDual;
    GetHPattern( iniName, "Skip12_Pattern", "Skip12_Pattern_Dual_noop", 
        metaData.SkipPatternFastDual, skipFastDual );

    CamCfg::APN_HPATTERN_FILE clampFastDual;
    GetHPattern( iniName, "Clamp12_Pattern", "Clamp12_Pattern_Dual_noop", 
        metaData.ClampPatternFastDual, clampFastDual );
     
     CamCfg::APN_HPATTERN_FILE roiFastDual;
     GetHPattern( iniName, "Roi12_Pattern", "Roi12_Pattern_Dual_noop", 
        metaData.RoiPatternFastDual, roiFastDual );
  
    metaData.VFlushBinning = static_cast<uint16_t>( GetPrivateProfileInt( _T( "system" ), 
            _T( "Vflush_Default" ), 1, iniFileName ) );

    metaData.VideoSubSample = static_cast<uint16_t>( GetPrivateProfileInt( _T( "system" ), 
            _T( "ascent_video_subsample" ), 1, iniFileName ) );

    metaData.PrimaryADLatency = static_cast<uint16_t>( GetPrivateProfileInt( _T( "system" ), 
                _T( "ad_latency_correct_count" ), 1, iniFileName ) );

    GetPrivateProfileString( _T( "system" ), _T( "ad_latency_correct_enable" ), _T( "false" ),
            iniStr, 511, iniFileName );
    tempCStr = iniStr;

    if( 0 == tempCStr.CompareNoCase( _T("true") ) )
    {
        metaData.DefaultDataReduction = true;
    }
    else
    {
        metaData.DefaultDataReduction = false;
    }

    GetPrivateProfileString( _T( "system" ), _T( "Color" ), _T( "false" ),
            iniStr, 511, iniFileName );
    tempCStr = iniStr;

    if( 0 == tempCStr.CompareNoCase( _T("true") ) )
    {
        metaData.Color = true;
    }
    else
    {
        metaData.Color = false;
    }

    GetPrivateProfileString( _T( "system" ), _T( "PixelSize_X" ), _T( "1.0" ),
            iniStr, 511, iniFileName );
    std::stringstream x( T2A(iniStr) );
    x >> metaData.PixelSizeX;

    GetPrivateProfileString( _T( "system" ), _T( "PixelSize_Y" ), _T( "1.0" ),
            iniStr, 511, iniFileName );
    std::stringstream y( T2A(iniStr) );
    y >> metaData.PixelSizeY;

    int delay = GetPrivateProfileInt( _T( "system" ), _T( "ShutterCloseDelay" ), 100, iniFileName );
    metaData.ShutterCloseDelay = delay;

    // get the information for creating the camera object
    GetPrivateProfileString( _T( "system" ), _T( "Interface" ), _T( "usb" ),
            iniStr, 511, iniFileName );
    std::string interfaceType( T2A(iniStr) );
    //convert to lower case
    std::transform(interfaceType.begin(), interfaceType.end(), interfaceType.begin(), ::tolower);

    GetPrivateProfileString( _T( "system" ), _T( "Usb_Device" ), _T( "0" ),
            iniStr, 511, iniFileName );
    std::string device( T2A(iniStr) );

    GetPrivateProfileString( _T( "system" ), _T( "ip_address" ), _T( "0.0.0.0" ),
            iniStr, 511, iniFileName );
    std::string ipaddr( T2A(iniStr) );
       
     //alta=1, ascent=2, Quad=3, hic=4, altaf=5, aspen=6
     const int camType = GetPrivateProfileInt( _T( "system" ), _T( "camera_type" ), 1, iniFileName );

    //create the specific camera type and fetch the camera
    //specific data
    if( 2 == camType || 3 == camType  || 4 == camType || 5 == camType || 6 == camType )
    {
      
         metaData.AdCfg = static_cast<uint16_t>( GetPrivateProfileInt( _T( "system" ),  
             _T( "ascent_ad_config" ), 88, iniFileName ) );

         metaData.DefaultGainLeft = static_cast<uint16_t>( GetPrivateProfileInt( _T( "system" ),  
             _T( "ascent_ad_gain_left_c0" ), 0, iniFileName ) );

         metaData.DefaultOffsetLeft = static_cast<uint16_t>( GetPrivateProfileInt( _T( "system" ),  
             _T( "ascent_ad_offset_left_c0" ), 0, iniFileName ) );

         metaData.DefaultGainRight = static_cast<uint16_t>( GetPrivateProfileInt( _T( "system" ),  
             _T( "ascent_ad_gain_right_c0" ), 0, iniFileName ) );

         metaData.DefaultOffsetRight = static_cast<uint16_t>( GetPrivateProfileInt( _T( "system" ),  
             _T( "ascent_ad_offset_right_c0" ), 0, iniFileName ) );

         metaData.AlternativeADLatency = static_cast<uint16_t>( GetPrivateProfileInt( _T( "system" ), 
        _T( "ad_latency_correct_count" ), 1, iniFileName ) );

         //hard coding this b/c it doesn't appear in the ini file
         metaData.AlternativeADType = CamCfg::ApnAdType_Ascent_Sixteen;
         metaData.PrimaryADType = CamCfg::ApnAdType_Ascent_Sixteen;
    }
    else
    {
         // TODO - is this no op?
         metaData.AdCfg = static_cast<uint16_t>( GetPrivateProfileInt( _T( "system" ), _T( "twelve_bit_ad_init " ), 
             8, iniFileName ) );

         metaData.DefaultGainLeft  = static_cast<uint16_t>( GetPrivateProfileInt( _T( "system" ), 
             _T( "twelve_bit_gain" ), 0, iniFileName ) );

        metaData.DefaultOffsetLeft  = static_cast<uint16_t>( GetPrivateProfileInt( _T( "system" ), 
             _T( "twelve_bit_clamp" ), 0, iniFileName ) );

        //hard coding this b/c it doesn't appear in the ini file
        metaData.AlternativeADLatency = 12;
        metaData.AlternativeADType = CamCfg::ApnAdType_Alta_Twelve;
        metaData.PrimaryADType = CamCfg::ApnAdType_Alta_Sixteen;
    }

    //load the meta and pattern data into the camera
    CApnCamData dataFromIni( metaData, 
        vertPattern, clampNorm, skipNorm, roiNorm,
        clampFast, skipFast, roiFast,
        vertVideoPattern, clampVideo, skipVideo, roiVideo,
        clampNormDual, skipNormDual, roiNormDual,
        clampFastDual, skipFastDual, roiFastDual );

    return dataFromIni;
}

void TestCamHelpers::GetHPattern( const std::string & iniName, const std::string & iniSectionName,
                                 const std::string & iniSectionNoOp, std::string & patternName,
                                 CamCfg::APN_HPATTERN_FILE & hPattern )
{
    USES_CONVERSION;
    CString iniFileName( iniName.c_str() );
    CString sectionName( iniSectionName.c_str() );
    CString sectionNoOp( iniSectionNoOp.c_str() );

    TCHAR iniStr[512];
    GetPrivateProfileString( _T( "system" ), sectionName, sectionNoOp,
            iniStr, 511, iniFileName );

    patternName = T2A(iniStr) ;
    
    hPattern = parseCfgTabDelim::FetchHorizontalPattern( patternName );
}
