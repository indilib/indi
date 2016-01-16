/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * qsilib
 * Copyright (C) David Challis 2012 <dchallis@qsimaging.com>
 * 
 */
#pragma once

#ifdef OSX_EMBEDED_MODE
#include "WinTypes.h"
#else
#include <WinTypes.h>
#endif

class ICameraEeprom
{ 
public:
	virtual BYTE EepromRead( USHORT address ) =0;
};

