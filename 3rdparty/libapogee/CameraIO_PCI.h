// CameraIO_PCI.h: interface for the CCameraIO_PCI class.
//
// Copyright (c) 2000 Apogee Instruments Inc.
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_CAMERAIO_PCI_H__0F583058_8596_11D4_915F_0060676644C1__INCLUDED_)
#define AFX_CAMERAIO_PCI_H__0F583058_8596_11D4_915F_0060676644C1__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "CameraIO.h"

class CCameraIO_PCI : public CCameraIO  
{
public:

	CCameraIO_PCI();
	virtual ~CCameraIO_PCI();

	bool InitDriver();
	long ReadLine( long SkipPixels, long Pixels, unsigned short* pLineBuffer );
	long Write( unsigned short reg, unsigned short val );
	long Read( unsigned short reg, unsigned short& val );

private:

	BOOLEAN	m_IsWDM;
	HANDLE	m_hDriver;

	void	CloseDriver();
};

#endif // !defined(AFX_CAMERAIO_PCI_H__0F583058_8596_11D4_915F_0060676644C1__INCLUDED_)
