/////////////////////////////////////////////////////////////
//
// ApnCamData_KAI2001CL.h:  Interface file for the CApnCamData_KAI2001CL class.
//
// Copyright (c) 2003-2007 Apogee Instruments, Inc.
//
/////////////////////////////////////////////////////////////

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER> 1000

#include "ApnCamData.h"

class CApnCamData_KAI2001CL : public CApnCamData
{
public:
	CApnCamData_KAI2001CL();
	virtual ~CApnCamData_KAI2001CL();

	void Initialize();

private:

	void set_vpattern();

	void set_hpattern_clamp_sixteen();
	void set_hpattern_skip_sixteen();
	void set_hpattern_roi_sixteen();

	void set_hpattern_clamp_twelve();
	void set_hpattern_skip_twelve();
	void set_hpattern_roi_twelve();

	void set_hpattern(	APN_HPATTERN_FILE	*Pattern,
						unsigned short		Mask,
						unsigned short		BinningLimit,
						unsigned short		RefNumElements,
						unsigned short		SigNumElements,
						unsigned short		BinNumElements[],
						unsigned short		RefPatternData[],
						unsigned short		SigPatternData[],
						unsigned short		BinPatternData[][APN_MAX_PATTERN_ENTRIES] );

}; 
