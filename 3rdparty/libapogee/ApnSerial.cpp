// ApnSerial.cpp: implementation of the CApnSerial class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ApnSerial.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CApnSerial::CApnSerial()
{
	m_SerialId = -1;
        m_BytesRead     = 0;
}

CApnSerial::~CApnSerial()
{

}
