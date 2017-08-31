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

#include "TestFilterWheel.h" 
#include "FilterWheelIo.h" 

//////////////////////////// 
// CTOR 
TestFilterWheel::TestFilterWheel() : m_fileName( __FILE__ )
{
}

//////////////////////////// 
// DTOR 
TestFilterWheel::~TestFilterWheel() 
{ 
} 

//////////////////////////// 
//      PROGRAM     FX2
void TestFilterWheel::ProgramFx2(const std::string & FilenameFx2, const std::string & FilenameDescriptor)
{
    
   m_Usb->Program( FilenameFx2, FilenameDescriptor );
}




