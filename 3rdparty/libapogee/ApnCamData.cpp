//////////////////////////////////////////////////////////////////////
//
// ApnCamData.cpp:  Implementation of the CApnCamData class.
//
// Copyright (c) 2003-2007 Apogee Instruments, Inc.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ApnCamData.h"

#include <stdlib.h>
#include <malloc.h>


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CApnCamData::CApnCamData()
{
	init_vpattern();

	init_hpattern( &m_ClampPatternSixteen );
	init_hpattern( &m_SkipPatternSixteen );
	init_hpattern( &m_RoiPatternSixteen );

	init_hpattern( &m_ClampPatternTwelve );
	init_hpattern( &m_SkipPatternTwelve );
	init_hpattern( &m_RoiPatternTwelve );
}

CApnCamData::~CApnCamData()
{
	clear_vpattern();

	clear_hpattern( &m_ClampPatternSixteen );
	clear_hpattern( &m_SkipPatternSixteen );
	clear_hpattern( &m_RoiPatternSixteen );

	clear_hpattern( &m_ClampPatternTwelve );
	clear_hpattern( &m_SkipPatternTwelve );
	clear_hpattern( &m_RoiPatternTwelve );
}


void CApnCamData::init_vpattern( )
{
	// OutputDebugString( "init_vpattern()" );

	m_VerticalPattern.Mask			= 0x0;
	m_VerticalPattern.NumElements	= 0;
	m_VerticalPattern.PatternData	= NULL;
}


void CApnCamData::clear_vpattern( )
{
	// OutputDebugString( "clear_vpattern()" );

	m_VerticalPattern.Mask			= 0x0;
	m_VerticalPattern.NumElements	= 0;
	
	if ( m_VerticalPattern.PatternData != NULL )
	{
		free( m_VerticalPattern.PatternData );
		m_VerticalPattern.PatternData = NULL;
	}
}


void CApnCamData::init_hpattern( APN_HPATTERN_FILE *Pattern )
{
	int Counter;

	// OutputDebugString( "init_hpattern()" );

	Pattern->Mask		    = 0x0;
	Pattern->RefNumElements = 0;
	Pattern->SigNumElements = 0;
	Pattern->BinningLimit	= 0;
	
	Pattern->RefPatternData = NULL;
	Pattern->SigPatternData = NULL;

	for ( Counter=0; Counter<APN_MAX_HBINNING; Counter++ )
	{
		Pattern->BinNumElements[Counter] = 0;
		Pattern->BinPatternData[Counter] = NULL;
	}
}


void CApnCamData::clear_hpattern( APN_HPATTERN_FILE *Pattern )
{
	int  Counter;
	// char szMsg[80];

	// OutputDebugString( "clear_hpattern()" );

	Pattern->Mask		    = 0x0;
	Pattern->RefNumElements = 0;
	Pattern->SigNumElements = 0;
	Pattern->BinningLimit	= 0;

	if ( Pattern->RefPatternData != NULL )
	{
		// OutputDebugString( "Freeing Allocated Reference Pattern Memory" );
		free( Pattern->RefPatternData );

		Pattern->RefPatternData = NULL;
	}
	if ( Pattern->SigPatternData != NULL )
	{
		// OutputDebugString( "Freeing Allocated Signal Pattern Memory" );
		free( Pattern->SigPatternData );

		Pattern->SigPatternData = NULL;
	}
	
	for ( Counter=0; Counter<APN_MAX_HBINNING; Counter++ )
	{
		Pattern->BinNumElements[Counter] = 0;
		if ( Pattern->BinPatternData[Counter] != NULL )
		{
			// sprintf( szMsg, "Freeing Allocated Binning Pattern Memory (Binning = %d)", Counter+1 );
			// OutputDebugString( szMsg );
			free( Pattern->BinPatternData[Counter] );

			Pattern->BinPatternData[Counter] = NULL;
		}
	}
}
