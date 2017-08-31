/*! 
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/.
*
* Copyright(c) 2010 Apogee Instruments, Inc. 
* \brief namespace that contains functions that are useful for the test camera classes 
* 
*/ 


#ifndef TESTCAMHELPERS_INCLUDE_H__ 
#define TESTCAMHELPERS_INCLUDE_H__ 

#include <vector>
#include <string>
#include <memory>
#include <stdint.h>

#include "ApnCamData.h"
#include "DefDllExport.h"

class CameraIo;
class ApogeeCam;
class CcdAcqParams;

namespace TestCamHelpers 
{ 
    std::vector<uint16_t> DLL_EXPORT GetListOfIds();
    
    std::string DLL_EXPORT GetSoftwareVersions();
    
    std::string DLL_EXPORT MkPatternFileName( const std::string & path, 
        const std::string & baseName);

    std::tr1::shared_ptr<CApnCamData> DLL_EXPORT CreateCApnCamDataFromFile( 
        const std::string & path, const std::string & cfgFileName );

    std::vector<uint16_t> DLL_EXPORT RunFifoTest( 
        std::tr1::shared_ptr<CameraIo> camIo,
        const uint16_t numRows, const uint16_t numCols, 
        const uint16_t speed, double & fifoGetImgTime);

    std::vector<uint16_t> DLL_EXPORT RunAdsTest( 
        ApogeeCam * theCam,
        std::tr1::shared_ptr<CcdAcqParams> acq,
        uint16_t numRows, 
        uint16_t numCols, 
        double & fifoGetImgTime);
    
    CApnCamData DLL_EXPORT MkMetaDataFromIni( 
        const std::string & iniName );

    void DLL_EXPORT GetHPattern( const std::string & iniName, 
        const std::string & iniSectionName, const std::string & iniSectionNoOp, 
        std::string & patternName, CamCfg::APN_HPATTERN_FILE & hPattern );

}; 

#endif
