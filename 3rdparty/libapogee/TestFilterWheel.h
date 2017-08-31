/*! 
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/.
*
* Copyright(c) 2011 Apogee Instruments, Inc. 
* \class TestFilterWheel
* \brief test object for programming filter wheel
* 
*/ 


#ifndef TESTFILTERWHEEL_INCLUDE_H__ 
#define TESTFILTERWHEEL_INCLUDE_H__ 

#include "ApogeeFilterWheel.h" 
#include <string>

class DLL_EXPORT TestFilterWheel: public ApogeeFilterWheel
{ 
    public: 
        TestFilterWheel();

        virtual ~TestFilterWheel(); 

        void ProgramFx2( const std::string & FilenameFx2, 
            const std::string & FilenameDescriptor);

    private:
        const std::string m_fileName;

}; 

#endif
