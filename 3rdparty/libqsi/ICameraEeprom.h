/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * qsilib
 * Copyright (C) David Challis 2012 <dchallis@qsimaging.com>
 * 
 */
#pragma once
#include <WinTypes.h>

class ICameraEeprom
{ 
public:
	virtual BYTE EepromRead( USHORT address ) =0;
};

