/*! 
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/.
*
* Copyright(c) 2009 Apogee Instruments, Inc. 
* \class CLibCurlWrap 
* \brief C++ wrapper for the libcurl c interface 
* 
*/ 

#include "libCurlWrap.h" 
#include <stdexcept>

#include "apgHelper.h" 

//////////////////////////// 
// Write any errors in here  
static char errorBuffer[CURL_ERROR_SIZE];  

//////////////////////////// 
// VECT WRITER
static std::vector<uint8_t> bufferVect;  
static int32_t vectWriter(uint8_t *data, size_t size, size_t nmemb,  
                  std::vector<uint8_t> &bufferVect) 
{
 
    const int32_t numBytes = apgHelper::SizeT2Int32(size) * apgHelper::SizeT2Int32(nmemb);

    for(int32_t i = 0; i < numBytes; ++i)
    {
        bufferVect.push_back( data[i] );
    }

    return numBytes;
}
 
//////////////////////////// 
// STR WRITER
// This is the writer call back function used by curl  
static std::string bufferStr; 
static int32_t strWriter(char *data, size_t size, size_t nmemb,  
                  std::string &bufferStr) 
{
 
    const size_t numBytes = size * nmemb;

    bufferStr.append( data, numBytes );

    return apgHelper::SizeT2Int32( numBytes );
}

//////////////////////////// 
// LOCAL     NAMESPACE
namespace
{
    const long OPERATION_TIMEOUT = (60*1);  //60 seconds * the number of minutes
}

//////////////////////////// 
// CTOR 
CLibCurlWrap::CLibCurlWrap() : m_curlHandle( 0 ),
                               m_fileName( __FILE__ )
{ 
    m_curlHandle = curl_easy_init();

    if( !m_curlHandle )
    {
        std::string errStr("curl_easy_init failed");
         apgHelper::throwRuntimeException( m_fileName, 
             errStr, __LINE__, Apg::ErrorType_Connection );
    }
} 

//////////////////////////// 
// DTOR 
CLibCurlWrap::~CLibCurlWrap() 
{ 
    curl_easy_cleanup( m_curlHandle );
} 



//////////////////////////// 
// HTTP GET 
void CLibCurlWrap::HttpGet(const std::string & url,
                            std::string & result)
{
    CurlSetupStrWrite ( url );
    result = ExecuteStr();
}

//////////////////////////// 
// HTTP GET 
void CLibCurlWrap::HttpGet(const std::string & url,
            std::vector<uint8_t> & result)
{
    CurlSetupVectWrite ( url );
    result = ExecuteVect();
}

//////////////////////////// 
// HTTP POST 
void CLibCurlWrap::HttpPost(const std::string & url,
                            const std::string & postFields,
                            std::string & result)
{
    CurlSetupStrWrite ( url );
    curl_easy_setopt(m_curlHandle, CURLOPT_POSTFIELDS, postFields.c_str());

    result = ExecuteStr();
}

//////////////////////////// 
// HTTP POST 
void CLibCurlWrap::HttpPost(const std::string & url,
            const std::string & postFields, 
            std::vector<uint8_t> & result)
{
    CurlSetupVectWrite ( url );
    curl_easy_setopt(m_curlHandle, CURLOPT_POSTFIELDS, postFields.c_str());

    result = ExecuteVect();
}


//////////////////////////// 
// CURL     SETUP  STR  WRITE
void CLibCurlWrap::CurlSetupStrWrite(const std::string & url)
{
     // Now set up all of the curl options  
    curl_easy_setopt(m_curlHandle, CURLOPT_ERRORBUFFER, errorBuffer);  
    curl_easy_setopt(m_curlHandle, CURLOPT_URL, url.c_str());  
    curl_easy_setopt(m_curlHandle, CURLOPT_WRITEFUNCTION, strWriter);  
    curl_easy_setopt(m_curlHandle, CURLOPT_WRITEDATA, &bufferStr); 
    curl_easy_setopt(m_curlHandle, CURLOPT_TIMEOUT,OPERATION_TIMEOUT);
    
}

//////////////////////////// 
// CURL     SETUP       VECTOR          WRITE
void CLibCurlWrap::CurlSetupVectWrite(const std::string & url)
{
     // Now set up all of the curl options  
    curl_easy_setopt(m_curlHandle, CURLOPT_ERRORBUFFER, errorBuffer);  
    curl_easy_setopt(m_curlHandle, CURLOPT_URL, url.c_str());  
    curl_easy_setopt(m_curlHandle, CURLOPT_WRITEFUNCTION, vectWriter);  
    curl_easy_setopt(m_curlHandle, CURLOPT_WRITEDATA, &bufferVect); 
    curl_easy_setopt(m_curlHandle, CURLOPT_TIMEOUT,OPERATION_TIMEOUT);
}

//////////////////////////// 
// EXECUTE  STR
std::string CLibCurlWrap::ExecuteStr()
{
    //clear out the string
    bufferStr.clear();

    //perform the transfer
    const CURLcode result = curl_easy_perform(m_curlHandle);

    if( CURLE_OK != result )
    {
        std::string curlError( errorBuffer );

        apgHelper::throwRuntimeException( m_fileName, curlError, 
            __LINE__, Apg::ErrorType_Critical );
    }

    return bufferStr;
}

//////////////////////////// 
// EXECUTE  VECT
std::vector<uint8_t> CLibCurlWrap::ExecuteVect()
{
    //clear out the vector
    bufferVect.clear();

    //perform the transfer
    const CURLcode result = curl_easy_perform(m_curlHandle);

    if( CURLE_OK != result )
    {
        std::string curlError( errorBuffer );

        apgHelper::throwRuntimeException( m_fileName, curlError, 
            __LINE__, Apg::ErrorType_Critical );
    }

    return bufferVect;
}

//////////////////////////// 
//      GET    DRIVER   VERSION
std::string CLibCurlWrap::GetVerison()
{
    std::string version( curl_version() );
    return version;
}
