/*****************************************************************************************
NAME
 CameraID

DESCRIPTION
 Class to specify info to define a specific camera
	Inherited by USBID and EthernetID

COPYRIGHT (C)
 QSI (Quantum Scientific Imaging) 2005-2006

REVISION HISTORY
DRC 03.23.09 Original Version
******************************************************************************************/

#pragma once

#include <string>

class CameraID
{
public:
	CameraID();
	CameraID(std::string Serial, std::string Desc, int vid, int pid);
	~CameraID();
	CameraID(const CameraID & cid);

	std::string SerialNumber;
	std::string Description;
	std::string InterfaceName;
	int VendorID;
	int ProductID;
	std::string SerialToOpen;
};

