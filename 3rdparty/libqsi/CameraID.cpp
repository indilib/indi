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

#include "CameraID.h"

CameraID::CameraID()
{
	SerialNumber = "";
	Description = "";
	VendorID = 0;
	ProductID = 0;
	InterfaceName = "";
	SerialToOpen = "";
}

CameraID::CameraID(std::string Serial, std::string Desc, int vid, int pid)
{
	SerialNumber = Serial;
	Description = Desc;
	VendorID = vid;
	ProductID = pid;
	InterfaceName = "USB";
	SerialToOpen = Serial;
}

CameraID::~CameraID()
{

}

CameraID::CameraID(const CameraID & cid)
{
	this->SerialNumber = cid.SerialNumber;
	this->Description  = cid.Description;
	this->VendorID     = cid.VendorID;
	this->ProductID    = cid.ProductID;
	this->InterfaceName = cid.InterfaceName;
	this->SerialToOpen = cid.SerialToOpen;
}

