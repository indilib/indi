//==========================================================================
#if 0
	File name  : sbigcam.cpp
	Driver type: SBIG CCD Camera INDI Driver

	Copyright (C) 2005-2006 Jan Soldan (jsoldan AT asu DOT cas DOT cz)
	251 65 Ondrejov-236, Czech Republic
	
	Acknowledgement:
  Jasem Mutlaq 	(mutlaqja AT ikarustech DOT com)
	Matt Longmire 	(matto AT sbig DOT com)	

	This library is free software; you can redistribute it and/or modify 
	it under the terms of the GNU Lesser General Public License as published 
	by the Free Software Foundation; either version 2.1 of the License, or 
	(at your option) any later version.

	This library is distributed in the hope that it will be useful, but 
	WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
	or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public 
	License for more details.

	You should have received a copy of the GNU Lesser General Public License 
	along with this library; if not, write to the Free Software Foundation, 
        Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
	
#endif

//FIXME remove after debugging

#include <iostream>
#include <cstring>

//==========================================================================
#include "sbigcam.h"
using namespace std;
//==========================================================================
#ifdef INDI
// We declare an auto pointer to SBIG CCD camera.
auto_ptr<SbigCam> sbig_cam(0);
#endif
//==========================================================================
// Initialize variables. Here we create new instance of SbigCam class.
//==========================================================================
#ifdef INDI
void ISInit()
{
	if(sbig_cam.get() == 0) sbig_cam.reset(new SbigCam());
}
#endif
//==========================================================================
// INDI will call this function is called when the client inquires about 
// the device properties.    
//==========================================================================
#ifdef INDI
void ISGetProperties(const char *dev)
{ 
	if(dev && strcmp(DEVICE_NAME, dev)) return;
	ISInit();
	sbig_cam->ISGetProperties();
}
#endif
//==========================================================================
#ifdef INDI
void ISNewSwitch(	const char *dev, const char *name, ISState *states, 
				 					char *names[], int num)
{
	if(dev && strcmp (DEVICE_NAME, dev)) return;
	ISInit();
	sbig_cam->ISNewSwitch(name, states, names, num);
}
#endif
//==========================================================================
#ifdef INDI
void ISNewText(	const char *dev, const char *name, char *texts[], 
			   				char *names[], int num)
{
	if(dev && strcmp (DEVICE_NAME, dev)) return;
	ISInit();
	sbig_cam->ISNewText(name, texts, names, num);
}
#endif
//==========================================================================
#ifdef INDI
void ISNewNumber(	const char *dev, const char *name, double values[], 
				 					char *names[], int num)
{
	if(dev && strcmp (DEVICE_NAME, dev)) return;
	ISInit();
	sbig_cam->ISNewNumber(name, values, names, num);
}
#endif
//==========================================================================
#ifdef INDI
void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n) 
{
  INDI_UNUSED(dev);
  INDI_UNUSED(name);
  INDI_UNUSED(sizes);
  INDI_UNUSED(blobsizes);
  INDI_UNUSED(blobs);
  INDI_UNUSED(formats);
  INDI_UNUSED(names);
  INDI_UNUSED(n);
}
void ISSnoopDevice (XMLEle *root) 
{
  INDI_UNUSED(root);
}
#endif
//==========================================================================
SbigCam::SbigCam()
{	
	InitVars(); 
 	OpenDriver();
	#ifdef INDI	
	IEAddTimer(POLL_TEMPERATURE_MS, SbigCam::UpdateTemperature, this);
	IEAddTimer(POLL_EXPOSURE_MS, SbigCam::UpdateExposure, this);
	#endif
}
//==========================================================================
SbigCam::SbigCam(const char* devName)
{	
	InitVars();
 	if(OpenDriver() == CE_NO_ERROR) OpenDevice(devName);
	#ifdef INDI	
	IEAddTimer(POLL_TEMPERATURE_MS, SbigCam::UpdateTemperature, this);
	IEAddTimer(POLL_EXPOSURE_MS, SbigCam::UpdateExposure, this);
	#endif
}
//==========================================================================
SbigCam::~SbigCam()
{
	CloseDevice();
	CloseDriver();
}
//==========================================================================
int SbigCam::OpenDriver()
{
	int res;
	GetDriverHandleResults gdhr;
 	SetDriverHandleParams  sdhp;

 	// Call the driver directly.
 	if((res = ::SBIGUnivDrvCommand(CC_OPEN_DRIVER, 0, 0)) == CE_NO_ERROR){
	   	// The driver was not open, so record the driver handle.
	   	res = ::SBIGUnivDrvCommand(CC_GET_DRIVER_HANDLE, 0, &gdhr);
 	}else if(res == CE_DRIVER_NOT_CLOSED){
		// The driver is already open which we interpret as having been
		// opened by another instance of the class so get the driver to 
		// allocate a new handle and then record it.
		sdhp.handle = INVALID_HANDLE_VALUE;
		res = ::SBIGUnivDrvCommand(CC_SET_DRIVER_HANDLE, &sdhp, 0);
		if(res == CE_NO_ERROR){
				if((res = ::SBIGUnivDrvCommand(CC_OPEN_DRIVER, 0, 0)) == CE_NO_ERROR){
						res = ::SBIGUnivDrvCommand(CC_GET_DRIVER_HANDLE, 0, &gdhr);
				}
		}
 	}
 	if(res == CE_NO_ERROR) SetDriverHandle(gdhr.handle);
 	return(res);
}
//==========================================================================
int SbigCam::CloseDriver()
{	
	int res;

	if((res = ::SBIGUnivDrvCommand(CC_CLOSE_DRIVER, 0, 0)) == CE_NO_ERROR){
 			SetDriverHandle();
	}
 	return(res);
}
//==========================================================================
int SbigCam::OpenDevice(const char *devName)
{
	int res;
	OpenDeviceParams odp;

	// Check if device already opened:
	if(IsDeviceOpen()) return(CE_NO_ERROR);

	// Try to open new device:
 	if(strcmp(devName, SBIG_USB0) == 0){
			odp.deviceType = DEV_USB1;
 	}else if(strcmp(devName, SBIG_USB1) == 0){
			odp.deviceType = DEV_USB2;
 	}else if(strcmp(devName, SBIG_USB2) == 0){
			odp.deviceType = DEV_USB3;
 	}else if(strcmp(devName, SBIG_USB3) == 0){
			odp.deviceType = DEV_USB4;
 	}else if(strcmp(devName, SBIG_LPT0) == 0){
			odp.deviceType = DEV_LPT1;
 	}else if(strcmp(devName, SBIG_LPT1) == 0){
			odp.deviceType = DEV_LPT2;
 	}else if(strcmp(devName, SBIG_LPT2) == 0){
			odp.deviceType = DEV_LPT3;
 	}else{
			return(CE_BAD_PARAMETER);
 	}

 	if((res = SBIGUnivDrvCommand(CC_OPEN_DEVICE, &odp, 0)) == CE_NO_ERROR){
			SetDeviceName(devName);
			SetFileDescriptor(true);
 	}
 	return(res);
}
//==========================================================================
int SbigCam::CloseDevice()
{	
	int res = CE_NO_ERROR;

 	if(IsDeviceOpen()){	
 		if((res = SBIGUnivDrvCommand(CC_CLOSE_DEVICE, 0, 0)) == CE_NO_ERROR){
				SetFileDescriptor();	// set value to -1
				SetCameraType(); 		// set value to NO_CAMERA
		}
	}
 	return(res);
}
//=========================================================================
int SbigCam::GetDriverInfo(GetDriverInfoParams *gdip, void *res)
{
 	return(SBIGUnivDrvCommand(CC_GET_DRIVER_INFO, gdip, res));
}
//==========================================================================
int SbigCam::SetDriverHandle(SetDriverHandleParams *sdhp)
{
 	return(SBIGUnivDrvCommand(CC_SET_DRIVER_HANDLE, sdhp, 0));
}
//==========================================================================
int SbigCam::GetDriverHandle(GetDriverHandleResults *gdhr)
{
 	return(SBIGUnivDrvCommand(CC_GET_DRIVER_HANDLE, 0, gdhr));
}
//==========================================================================
int SbigCam::StartExposure(StartExposureParams *sep)
{
 	return(SBIGUnivDrvCommand(CC_START_EXPOSURE, sep, 0));
}
//==========================================================================
int SbigCam::EndExposure(EndExposureParams *eep)
{
 	return(SBIGUnivDrvCommand(CC_END_EXPOSURE, eep, 0));
}
//==========================================================================
int SbigCam::StartReadout(StartReadoutParams *srp)
{
 	return(SBIGUnivDrvCommand(CC_START_READOUT, srp, 0));
}
//==========================================================================
int SbigCam::ReadoutLine(ReadoutLineParams   *rlp,
                         unsigned short      *results,
                         bool				bSubtract)
{
	int	res;

 	if(bSubtract){
    	res = SBIGUnivDrvCommand(CC_READ_SUBTRACT_LINE, rlp, results);
 	}else{
    	res = SBIGUnivDrvCommand(CC_READOUT_LINE, rlp, results);
 	}
	return(res);
}
//==========================================================================
int SbigCam::DumpLines(DumpLinesParams *dlp)
{
 	return(SBIGUnivDrvCommand(CC_DUMP_LINES, dlp, 0));
}
//==========================================================================
int SbigCam::EndReadout(EndReadoutParams *erp)
{
 	return(SBIGUnivDrvCommand(CC_END_READOUT, erp, 0));
}
//==========================================================================
int SbigCam::SetTemperatureRegulation(SetTemperatureRegulationParams *strp)
{	
	return(SBIGUnivDrvCommand(CC_SET_TEMPERATURE_REGULATION, strp, 0));
}
//==========================================================================
int SbigCam::SetTemperatureRegulation(double temperature, bool enable)
{
	int res;
 	SetTemperatureRegulationParams strp;

 	if(CheckLink()){
    	strp.regulation  = enable ? REGULATION_ON : REGULATION_OFF;
    	strp.ccdSetpoint = CalcSetpoint(temperature);
    	res = SBIGUnivDrvCommand(CC_SET_TEMPERATURE_REGULATION, &strp, 0);
	}else{
			res = CE_DEVICE_NOT_OPEN;
	}
 	return(res);
}
//==========================================================================
int SbigCam::QueryTemperatureStatus(	bool &enabled, double &ccdTemp,
																		double &setpointTemp, double &power)
{
	int res;
 	QueryTemperatureStatusResults qtsr;

 	if(CheckLink()){
		res = SBIGUnivDrvCommand(CC_QUERY_TEMPERATURE_STATUS, 0, &qtsr);
		if(res == CE_NO_ERROR){
				enabled      = qtsr.enabled;
				ccdTemp      = CalcTemperature(CCD_THERMISTOR, qtsr.ccdThermistor);
				setpointTemp = CalcTemperature(CCD_THERMISTOR, qtsr.ccdSetpoint);
				power        = qtsr.power/255.0;
		}
 	}else{
		res = CE_DEVICE_NOT_OPEN;
	}
 	return(res);
}
//==========================================================================
unsigned short SbigCam::CalcSetpoint(double temperature)
{
 	// Calculate 'setpoint' from the temperature T in degr. of Celsius.
 	double expo = (log(R_RATIO_CCD) * (T0 - temperature)) / DT_CCD;
 	double r    = R0 * exp(expo);
 	return((unsigned short)(((MAX_AD / (R_BRIDGE_CCD / r + 1.0)) + 0.5)));
}
//==========================================================================
double SbigCam::CalcTemperature(short thermistorType, short setpoint)
{
 	double r, expo, rBridge, rRatio, dt;

 	switch(thermistorType){
		case 	AMBIENT_THERMISTOR:
					rBridge = R_BRIDGE_AMBIENT;
					rRatio  = R_RATIO_AMBIENT;
					dt      = DT_AMBIENT;
					break;
   	case CCD_THERMISTOR:
   	default:
					rBridge = R_BRIDGE_CCD;
					rRatio  = R_RATIO_CCD;
					dt      = DT_CCD;
					break;
 	}

 	// Calculate temperature T in degr. Celsius from the 'setpoint'
 	r = rBridge / ((MAX_AD / setpoint) - 1.0);
 	expo = log(r / R0) / log(rRatio);
 	return(T0 - dt * expo);
}
//==========================================================================
int SbigCam::ActivateRelay(ActivateRelayParams *arp)
{
 	return(SBIGUnivDrvCommand(CC_ACTIVATE_RELAY, arp, 0));
}
//==========================================================================
int SbigCam::PulseOut(PulseOutParams *pop)
{
 	return(SBIGUnivDrvCommand(CC_PULSE_OUT, pop, 0));
}
//==========================================================================
int SbigCam::TxSerialBytes(	TXSerialBytesParams  *txsbp,
                           	TXSerialBytesResults *txsbr)
{
 	return(SBIGUnivDrvCommand(CC_TX_SERIAL_BYTES, txsbp, txsbr));
}
//==========================================================================
int SbigCam::GetSerialStatus(GetSerialStatusResults *gssr)
{
 	return(SBIGUnivDrvCommand(CC_GET_SERIAL_STATUS, 0, gssr));
}
//==========================================================================
int SbigCam::AoTipTilt(AOTipTiltParams *aottp)
{
 	return(SBIGUnivDrvCommand(CC_AO_TIP_TILT, aottp, 0));
}
//==========================================================================
int SbigCam::AoDelay(AODelayParams *aodp)
{
 	return(SBIGUnivDrvCommand(CC_AO_DELAY, aodp, 0));
}
//==========================================================================
int SbigCam::Cfw(CFWParams *cfwp, CFWResults *cfwr)
{
 	return(SBIGUnivDrvCommand(CC_CFW, cfwp, cfwr));
}
//==========================================================================
int SbigCam::EstablishLink()
{
	int 									res;
 	EstablishLinkParams  elp;
 	EstablishLinkResults elr;

 	elp.sbigUseOnly = 0;

 	if((res = SBIGUnivDrvCommand(CC_ESTABLISH_LINK, &elp, &elr)) == CE_NO_ERROR){
    	SetCameraType((CAMERA_TYPE)elr.cameraType);
    	SetLinkStatus(true);
 	}
 	return(res);
}
//==========================================================================
int SbigCam::GetCcdInfo(GetCCDInfoParams *gcp, void *gcr)
{
 	return(SBIGUnivDrvCommand(CC_GET_CCD_INFO, gcp, gcr));
}	
//==========================================================================
int	SbigCam::GetCcdSizeInfo(	int ccd, int binning, int &frmW, int &frmH, 
						   							double &pixW, double &pixH)
{
	int res;
	GetCCDInfoParams		gcp;
  GetCCDInfoResults0	gcr;

  gcp.request = ccd;
	res = SBIGUnivDrvCommand(CC_GET_CCD_INFO, &gcp, &gcr);
	if(res == CE_NO_ERROR){
			frmW = gcr.readoutInfo[binning].width;
			frmH = gcr.readoutInfo[binning].height;
			pixW = BcdPixel2double(gcr.readoutInfo[binning].pixelWidth);
			pixH = BcdPixel2double(gcr.readoutInfo[binning].pixelHeight);
	}
	return(res);
}
//==========================================================================
int SbigCam::QueryCommandStatus(	QueryCommandStatusParams  *qcsp,
                                QueryCommandStatusResults *qcsr)
{
 	return(SBIGUnivDrvCommand(CC_QUERY_COMMAND_STATUS, qcsp, qcsr));
}
//==========================================================================
int SbigCam::MiscellaneousControl(MiscellaneousControlParams *mcp)
{
 	return(SBIGUnivDrvCommand(CC_MISCELLANEOUS_CONTROL, mcp, 0));		
}
//==========================================================================
int SbigCam::ReadOffset(ReadOffsetParams *rop, ReadOffsetResults *ror)
{
 	return(SBIGUnivDrvCommand(CC_READ_OFFSET, rop, ror));		
}
//==========================================================================
int SbigCam::GetLinkStatus(GetLinkStatusResults *glsr)
{
 	return(SBIGUnivDrvCommand(CC_GET_LINK_STATUS, glsr, 0));		
}
//==========================================================================
string SbigCam::GetErrorString(int err)
{
	int res;
 	GetErrorStringParams		gesp;
 	GetErrorStringResults		gesr;
 
 	gesp.errorNo = err;
 	res = SBIGUnivDrvCommand(CC_GET_ERROR_STRING, &gesp, &gesr);
	if(res == CE_NO_ERROR) return(string(gesr.errorString));

	char str [128];
	sprintf(str, "No error string found! Error code: %d", err);
 	return(string(str));
}
//==========================================================================
int SbigCam::SetDriverControl(SetDriverControlParams *sdcp)
{
 	return(SBIGUnivDrvCommand(CC_SET_DRIVER_CONTROL, sdcp, 0));		
}
//==========================================================================
int SbigCam::GetDriverControl(	GetDriverControlParams  *gdcp,
							  							GetDriverControlResults *gdcr)
{
 	return(SBIGUnivDrvCommand(CC_GET_DRIVER_CONTROL, gdcp, gdcr));		
}
//==========================================================================
int SbigCam::UsbAdControl(USBADControlParams *usbadcp)
{
 	return(SBIGUnivDrvCommand(CC_USB_AD_CONTROL, usbadcp, 0));		
}
//==========================================================================
int SbigCam::QueryUsb(QueryUSBResults *qusbr)
{
 	return(SBIGUnivDrvCommand(CC_QUERY_USB, 0, qusbr));		
}
//==========================================================================
int	SbigCam::RwUsbI2c(RWUSBI2CParams *rwusbi2cp)
{
 	return(SBIGUnivDrvCommand(CC_RW_USB_I2C, rwusbi2cp, 0));	
}
//==========================================================================
int	SbigCam::BitIo(BitIOParams *biop, BitIOResults *bior)
{
 	return(SBIGUnivDrvCommand(CC_BIT_IO, biop, bior));	
}
//==========================================================================
string SbigCam::GetCameraName()
{
	int									res;
 	string             	name = "Unknown camera";
 	GetCCDInfoParams   	gccdip;
 	GetCCDInfoResults0	gccdir;
 	char               	*p1, *p2;

 	gccdip.request = CCD_INFO_IMAGING;  // request 0
 	res = SBIGUnivDrvCommand(CC_GET_CCD_INFO, &gccdip, &gccdir);
	if(res == CE_NO_ERROR){
    	name = gccdir.name;
    	switch(gccdir.cameraType){
      		case	ST237_CAMERA:
            	 	if(gccdir.readoutInfo[0].gain >= 0x100) name += 'A';
	    		 			break;
       		case 	STL_CAMERA:
 	    		 			// driver reports name as "SBIG ST-L-XXX..."
    	    	 		p1 = gccdir.name + 5;
    	    	 		if((p2  = strchr(p1,' ')) != NULL){
    	       				*p2  = '\0';
    	       				name = p1;
    	    	 		}
	    		 			break;
       		case 	NO_CAMERA:
            	 	name = "No camera";
	    		 			break;
       		default:
	    		 			break;
    	}
 	}
	return(name);
} 					
//==========================================================================
string SbigCam::GetCameraID()
{ 	
	string            	str; 
	GetCCDInfoParams		gccdip;
 	GetCCDInfoResults2	gccdir2;

	gccdip.request = 2;
	if(GetCcdInfo(&gccdip, (void *)&gccdir2) == CE_NO_ERROR){
  		str = gccdir2.serialNumber;
 	}
	return(str);
}
//==========================================================================
int SbigCam::SetDeviceName(const char *name)
{
	int res = CE_NO_ERROR;

 	if(strlen(name) < PATH_MAX){
			strcpy(m_dev_name, name);
 	}else{
 			res = CE_BAD_PARAMETER;
 	}
 	return(res);
}
//==========================================================================
// SBIGUnivDrvCommand:
// Bottleneck function for all calls to the driver that logs the command
// and error. First it activates our handle and then it calls the driver.
// Activating the handle first allows having multiple instances of this
// class dealing with multiple cameras on different communications port.
// Also allows direct access to the SBIG Universal Driver after the driver
// has been opened.
int SbigCam::SBIGUnivDrvCommand(PAR_COMMAND command, void *params, void *results)
{	
	int res;
 	SetDriverHandleParams sdhp;
 	// Make sure we have a valid handle to the driver.
 	if(GetDriverHandle() == INVALID_HANDLE_VALUE){
			res = CE_DRIVER_NOT_OPEN;
 	}else{
			// Handle is valid so install it in the driver.
			sdhp.handle = GetDriverHandle();
			res = ::SBIGUnivDrvCommand(CC_SET_DRIVER_HANDLE, &sdhp, 0);

			if(res == CE_FAKE_DRIVER)
			{
			// The user is using the dummy driver. Tell him to download the real driver
			IDMessage(DEVICE_NAME, "Error: SBIG Dummy Driver is being used now. You can only control your camera by downloading SBIG driver from INDI website @ indi.sf.net");
			}
			else if(res == CE_NO_ERROR){
					res = ::SBIGUnivDrvCommand(command, params, results);
			}
 	}
 	return(res);
}
//==========================================================================
bool SbigCam::CheckLink()
{
 	if(GetCameraType() != NO_CAMERA && GetLinkStatus()) return(true);
 	return(false);
}	
//==========================================================================
int SbigCam::GetNumOfCcdChips()
{	
	int res;

	switch(GetCameraType()){
		case 	ST237_CAMERA:
		case 	ST5C_CAMERA:
		case 	ST402_CAMERA:
			 		res = 1;
			 		break;
		case 	ST7_CAMERA:
		case 	ST8_CAMERA:
		case 	ST9_CAMERA:
		case 	ST2K_CAMERA:
			 		res = 2;
			 		break;
		case 	STL_CAMERA:
			 		res = 3;
			 		break;
		case 	NO_CAMERA:
		default:
			 		res = 0;
			 		break;
	}
	return(res);
}
//==========================================================================
bool SbigCam::IsFanControlAvailable()
{
	CAMERA_TYPE camera = GetCameraType();
	if(camera == ST5C_CAMERA || camera == ST402_CAMERA) return(false);
	return(true);
}
//==========================================================================
double SbigCam::BcdPixel2double(ulong bcd)
{
	double value = 0.0;
	double digit = 0.01;
  
	for(int i = 0; i < 8; i++){
			value += (bcd & 0x0F) * digit;
			digit *= 10.0;
			bcd  >>= 4;
	} 
  return(value);
 }
//==========================================================================
void SbigCam::InitVars()
{
	SetFileDescriptor();
 	SetCameraType();
 	SetLinkStatus();
 	SetDeviceName("");

	#ifdef INDI

	// CCD PRODUCT:
	IUFillText(&m_icam_product_t[0], PRODUCT_NAME_T, PRODUCT_LABEL_T, UNKNOWN_LABEL);
	IUFillText(&m_icam_product_t[1], PRODUCT_ID_NAME_T, PRODUCT_ID_LABEL_T, UNKNOWN_LABEL);	
	IUFillTextVector(	&m_icam_product_tp, 
									m_icam_product_t,
									NARRAY(m_icam_product_t),
									DEVICE_NAME, 
									CCD_PRODUCT_NAME_TP,
									CCD_PRODUCT_LABEL_TP,
									CAMERA_GROUP,
									IP_RO,
									INDI_TIMEOUT, 
									IPS_IDLE);

	// CCD DEVICE PORT:
	IUFillText(&m_icam_device_port_t[0], PORT_NAME_T, PORT_LABEL_T, SBIG_USB0);
	IUFillTextVector(	&m_icam_device_port_tp, 
									m_icam_device_port_t,
									NARRAY(m_icam_device_port_t),
									DEVICE_NAME, 
									CCD_DEVICE_PORT_NAME_TP,
									CCD_DEVICE_PORT_LABEL_TP,
									CAMERA_GROUP,
									IP_RW,
									INDI_TIMEOUT, 
									IPS_IDLE);

	// CCD CONNECTION:
  	IUFillSwitch(&m_icam_connection_s[0], CONNECT_NAME_S, CONNECT_LABEL_S, ISS_OFF);
  	IUFillSwitch(&m_icam_connection_s[1], DISCONNECT_NAME_S, DISCONNECT_LABEL_S, ISS_ON);
  	IUFillSwitchVector(&m_icam_connection_sp, 
										m_icam_connection_s, 
										NARRAY(m_icam_connection_s), 
										DEVICE_NAME, 
										CCD_CONNECTION_NAME_SP, 
										CCD_CONNECTION_LABEL_SP, 
										CAMERA_GROUP, 
										IP_RW, 
										ISR_1OFMANY, 
										INDI_TIMEOUT, 
										IPS_IDLE);

	// CCD FAN STATE:
  IUFillSwitch(&m_icam_fan_state_s[0], CCD_FAN_ON_NAME_S, CCD_FAN_ON_LABEL_S, ISS_ON);
  IUFillSwitch(&m_icam_fan_state_s[1], CCD_FAN_OFF_NAME_S, CCD_FAN_OFF_LABEL_S, ISS_OFF);
 	IUFillSwitchVector(&m_icam_fan_state_sp, 
									m_icam_fan_state_s, 
									NARRAY(m_icam_fan_state_s), 
									DEVICE_NAME, 
									CCD_FAN_NAME_SP, 
									CCD_FAN_LABEL_SP, 
									TEMPERATURE_GROUP, 
									IP_RW, 
									ISR_1OFMANY, 
									INDI_TIMEOUT, 
									IPS_OK);

	// CCD TEMPERATURE:
 	IUFillNumber(	&m_icam_temperature_n[0], CCD_TEMPERATURE_NAME_N, 
							CCD_TEMPERATURE_LABEL_N, "%+.1f", MIN_CCD_TEMP, 
							MAX_CCD_TEMP, CCD_TEMP_STEP, DEF_CCD_TEMP);
  IUFillNumberVector(&m_icam_temperature_np, 
									m_icam_temperature_n, 
									NARRAY(m_icam_temperature_n), 
									DEVICE_NAME, 
									CCD_TEMPERATURE_NAME_NP, 
									CCD_TEMPERATURE_LABEL_NP, 
									TEMPERATURE_GROUP, 
									IP_RW, 
									INDI_TIMEOUT, 
									IPS_IDLE);

	// CCD COOLER:
 	IUFillNumber(	&m_icam_cooler_n[0], CCD_COOLER_NAME_N, 
							CCD_COOLER_LABEL_N, "%.1f", 0, 0, 0, 0);
  IUFillNumberVector(&m_icam_cooler_np, 
									m_icam_cooler_n, 
									NARRAY(m_icam_cooler_n), 
									DEVICE_NAME, 
									CCD_COOLER_NAME_NP, 
									CCD_COOLER_LABEL_NP, 
									TEMPERATURE_GROUP, 
									IP_RO, 
									INDI_TIMEOUT, 
									IPS_IDLE);
	
	// CCD TEMPERATURE POLLING:
 	IUFillNumber(	&m_icam_temperature_polling_n[0], 
							CCD_TEMPERATURE_POLLING_NAME_N, 
							CCD_TEMPERATURE_POLLING_LABEL_N, "%.1f", 
							MIN_POLLING_TIME, 
							MAX_POLLING_TIME,
							STEP_POLLING_TIME, 
							CUR_POLLING_TIME);
  IUFillNumberVector(&m_icam_temperature_polling_np, 
									m_icam_temperature_polling_n, 
									NARRAY(m_icam_temperature_polling_n), 
									DEVICE_NAME, 
									CCD_TEMPERATURE_POLLING_NAME_NP, 
									CCD_TEMPERATURE_POLLING_LABEL_NP, 
									TEMPERATURE_GROUP, 
									IP_RW, 
									INDI_TIMEOUT, 
									IPS_IDLE);

	// CCD TEMPERATURE MSG:
  IUFillSwitch(	&m_icam_temperature_msg_s[0], 
							CCD_TEMPERATURE_MSG_YES_NAME_S, 
							CCD_TEMPERATURE_MSG_YES_LABEL_S, 
							ISS_ON);
 	IUFillSwitch(	&m_icam_temperature_msg_s[1], 
							CCD_TEMPERATURE_MSG_NO_NAME_S, 
							CCD_TEMPERATURE_MSG_NO_LABEL_S, 
							ISS_OFF);
  IUFillSwitchVector(&m_icam_temperature_msg_sp, 
									m_icam_temperature_msg_s, 
									NARRAY(m_icam_temperature_msg_s), 
									DEVICE_NAME, 
									CCD_TEMPERATURE_MSG_NAME_SP, 
									CCD_TEMPERATURE_MSG_LABEL_SP, 
									TEMPERATURE_GROUP, 
									IP_RW, 
									ISR_1OFMANY, 
									INDI_TIMEOUT, 
									IPS_IDLE);

	// CCD FRAME TYPE:
	IUFillSwitch(	&m_icam_frame_type_s[0], CCD_FRAME_LIGHT_NAME_N, 
							CCD_FRAME_LIGHT_LABEL_N, ISS_ON);
	IUFillSwitch(	&m_icam_frame_type_s[1], CCD_FRAME_DARK_NAME_N,  
							CCD_FRAME_DARK_LABEL_N, ISS_OFF);
	IUFillSwitch(	&m_icam_frame_type_s[2], CCD_FRAME_FLAT_NAME_N,  
							CCD_FRAME_FLAT_LABEL_N, ISS_OFF);
	IUFillSwitch(	&m_icam_frame_type_s[3], CCD_FRAME_BIAS_NAME_N,  
							CCD_FRAME_BIAS_LABEL_N, ISS_OFF);
	IUFillSwitchVector(&m_icam_frame_type_sp, 
									m_icam_frame_type_s, 
									NARRAY(m_icam_frame_type_s), 
									DEVICE_NAME, 
									CCD_FRAME_TYPE_NAME_NP,
									CCD_FRAME_TYPE_LABEL_NP, 
									FRAME_GROUP, 
									IP_RW, 
									ISR_1OFMANY, 
									INDI_TIMEOUT, 
									IPS_OK);
 
	// CCD REQUEST:
  IUFillSwitch(	&m_icam_ccd_request_s[0], CCD_IMAGING_NAME_S,  
							CCD_IMAGING_LABEL_S, ISS_ON);
  IUFillSwitch(	&m_icam_ccd_request_s[1], CCD_TRACKING_NAME_S, 
							CCD_TRACKING_LABEL_S, ISS_OFF);
 	IUFillSwitch(	&m_icam_ccd_request_s[2], CCD_EXT_TRACKING_NAME_S, 
							CCD_EXT_TRACKING_LABEL_S, ISS_OFF);
  IUFillSwitchVector(&m_icam_ccd_request_sp, 
									m_icam_ccd_request_s, 
									NARRAY(m_icam_ccd_request_s), 
									DEVICE_NAME, 
									CCD_REQUEST_NAME_SP, 
									CCD_REQUEST_LABEL_SP, 
									FRAME_GROUP, 
									IP_RW, 
									ISR_1OFMANY, 
									INDI_TIMEOUT, 
									IPS_OK);

	// CCD BINNING
	#ifdef USE_CCD_BINNING_STANDARD_PROPERTY  
 	IUFillNumber(	&m_icam_ccd_binning_n[0], CCD_HOR_BIN_NAME_N, 
							CCD_HOR_BIN_LABEL_N, "%.0f", CCD_MIN_BIN, CCD_MAX_BIN, 1, 1);
  IUFillNumber(	&m_icam_ccd_binning_n[1], CCD_VER_BIN_NAME_N, 
							CCD_VER_BIN_LABEL_N, "%.0f", CCD_MIN_BIN, CCD_MAX_BIN, 1, 1);
  IUFillNumberVector(&m_icam_ccd_binning_np, 
									m_icam_ccd_binning_n, 
									NARRAY(m_icam_ccd_binning_n), 
									DEVICE_NAME, 
									CCD_BINNING_NAME_NP,
									CCD_BINNING_LABEL_NP, 
									FRAME_GROUP, 
									IP_RW, 
									INDI_TIMEOUT, 
									IPS_IDLE);
	#else
	IUFillSwitch(	&m_icam_binning_mode_s[0], CCD_BIN_1x1_I_NAME_S, 
							CCD_BIN_1x1_I_LABEL_S, ISS_ON);
	IUFillSwitch(	&m_icam_binning_mode_s[1], CCD_BIN_2x2_I_NAME_S, 
							CCD_BIN_2x2_I_LABEL_S, ISS_OFF);
	IUFillSwitch(	&m_icam_binning_mode_s[2], CCD_BIN_3x3_I_NAME_S, 
							CCD_BIN_3x3_I_LABEL_S, ISS_OFF);
	IUFillSwitch(	&m_icam_binning_mode_s[3], CCD_BIN_9x9_I_NAME_S, 
							CCD_BIN_9x9_I_LABEL_S, ISS_OFF);
	IUFillSwitch(	&m_icam_binning_mode_s[4], CCD_BIN_2x2_E_NAME_S, 
							CCD_BIN_2x2_E_LABEL_S, ISS_OFF);
	IUFillSwitch(	&m_icam_binning_mode_s[5], CCD_BIN_3x3_E_NAME_S, 
							CCD_BIN_3x3_E_LABEL_S, ISS_OFF);
	IUFillSwitchVector(&m_icam_binning_mode_sp, 
									m_icam_binning_mode_s, 
									NARRAY(m_icam_binning_mode_s), 
									DEVICE_NAME, 
									CCD_BINNING_MODE_NAME_SP, 
									CCD_BINNING_MODE_LABEL_SP, 
									FRAME_GROUP, 
									IP_RW, 
									ISR_1OFMANY, 
									INDI_TIMEOUT, 
									IPS_OK);
	#endif

	// CCD PIXEL INFO:	
 	IUFillNumber(	&m_icam_pixel_size_n[0], CCD_PIXEL_WIDTH_NAME_N, 
							CCD_PIXEL_WIDTH_LABEL_N, "%.2f", 0, 0, 0, 0);
  IUFillNumber(	&m_icam_pixel_size_n[1], CCD_PIXEL_HEIGHT_NAME_N, 
							CCD_PIXEL_HEIGHT_LABEL_N, "%.2f", 0, 0, 0, 0);
  IUFillNumberVector(&m_icam_pixel_size_np, 
									m_icam_pixel_size_n, 
									NARRAY(m_icam_pixel_size_n), 
									DEVICE_NAME, 
									CCD_PIXEL_INFO_NAME_NP, 
									CCD_PIXEL_INFO_LABEL_NP, 
									FRAME_GROUP, 
									IP_RO, 
									INDI_TIMEOUT, 
									IPS_IDLE);

	// CCD FRAME
	#ifdef USE_CCD_FRAME_STANDARD_PROPERTY  
	IUFillNumber(	&m_icam_ccd_frame_n[0], CCD_FRAME_X_NAME_N, 
							CCD_FRAME_X_LABEL_N, "%.0f", 0,0,0,0);
	IUFillNumber(	&m_icam_ccd_frame_n[1], CCD_FRAME_Y_NAME_N, 
							CCD_FRAME_Y_LABEL_N, "%.0f", 0,0,0,0);
	IUFillNumber(	&m_icam_ccd_frame_n[2], CCD_FRAME_W_NAME_N, 
							CCD_FRAME_W_LABEL_N, "%.0f", 0,0,0,0);
	IUFillNumber(	&m_icam_ccd_frame_n[3], CCD_FRAME_H_NAME_N, 
							CCD_FRAME_H_LABEL_N, "%.0f", 0,0,0,0);
  IUFillNumberVector(&m_icam_ccd_frame_np, 
									m_icam_ccd_frame_n, 
									NARRAY(m_icam_ccd_frame_n), 
									DEVICE_NAME, 
									CCD_FRAME_NAME_NP, 
									CCD_FRAME_LABEL_NP, 
									FRAME_GROUP, 
									IP_RW, 
									INDI_TIMEOUT, 
									IPS_IDLE);
	#else
	// FRAME X:
  IUFillNumber(	&m_icam_frame_x_n[0], CCD_FRAME_X_NAME_N, 
							CCD_FRAME_X_LABEL_N, "%.0f", 0,0,0,0);
  IUFillNumberVector(&m_icam_frame_x_np, 
									m_icam_frame_x_n, 
									NARRAY(m_icam_frame_x_n), 
									DEVICE_NAME, 
									CCD_FRAME_X_NAME_NP, 
									CCD_FRAME_X_LABEL_NP, 
									FRAME_GROUP, 
									IP_RW, 
									INDI_TIMEOUT, 
									IPS_IDLE);

	// FRAME Y:
  IUFillNumber(	&m_icam_frame_y_n[0], CCD_FRAME_Y_NAME_N, 	
							CCD_FRAME_Y_LABEL_N, "%.0f", 0,0,0,0);
  IUFillNumberVector(&m_icam_frame_y_np, 
									m_icam_frame_y_n, 
									NARRAY(m_icam_frame_y_n), 
									DEVICE_NAME, 
									CCD_FRAME_Y_NAME_NP, 
									CCD_FRAME_Y_LABEL_NP, 
									FRAME_GROUP, 
									IP_RW, 
									INDI_TIMEOUT, 
									IPS_IDLE);

	// FRAME W:
  IUFillNumber(	&m_icam_frame_w_n[0], CCD_FRAME_W_NAME_N, 
							CCD_FRAME_W_LABEL_N, "%.0f",0,0,0,0);
  IUFillNumberVector(&m_icam_frame_w_np, 
									m_icam_frame_w_n, 
									NARRAY(m_icam_frame_w_n), 
									DEVICE_NAME, 
									CCD_FRAME_W_NAME_NP, 
									CCD_FRAME_W_LABEL_NP, 
									FRAME_GROUP, 
									IP_RW, 
									INDI_TIMEOUT, 
									IPS_IDLE);

	// FRAME H:
  IUFillNumber(	&m_icam_frame_h_n[0], CCD_FRAME_H_NAME_N, 
							CCD_FRAME_H_LABEL_N, "%.0f",0,0,0,0);
  IUFillNumberVector(&m_icam_frame_h_np, 
									m_icam_frame_h_n, 
									NARRAY(m_icam_frame_h_n), 
									DEVICE_NAME, 
									CCD_FRAME_H_NAME_NP, 
									CCD_FRAME_H_LABEL_NP, 
									FRAME_GROUP, 
									IP_RW, 
									INDI_TIMEOUT, 
									IPS_IDLE);
	#endif
	
	// CFW PRODUCT:
	IUFillText(&m_icfw_product_t[0], PRODUCT_NAME_T, PRODUCT_LABEL_T, UNKNOWN_LABEL);
	IUFillText(&m_icfw_product_t[1], PRODUCT_ID_NAME_T, PRODUCT_ID_LABEL_T, UNKNOWN_LABEL);	
	IUFillTextVector(	&m_icfw_product_tp, 
									m_icfw_product_t,
									NARRAY(m_icfw_product_t),
									DEVICE_NAME, 
									CFW_PRODUCT_NAME_TP,
									CFW_PRODUCT_LABEL_TP,
									CFW_GROUP,
									IP_RO,
									INDI_TIMEOUT, 
									IPS_IDLE);

	// CFW_MODEL:
  IUFillSwitch(&m_icfw_type_s[0], CFW1_NAME_S, CFW1_LABEL_S, ISS_OFF);
 	IUFillSwitch(&m_icfw_type_s[1], CFW2_NAME_S, CFW2_LABEL_S, ISS_OFF);
	IUFillSwitch(&m_icfw_type_s[2], CFW3_NAME_S, CFW3_LABEL_S, ISS_OFF);
 	IUFillSwitch(&m_icfw_type_s[3], CFW4_NAME_S, CFW4_LABEL_S, ISS_OFF);
 	IUFillSwitch(&m_icfw_type_s[4], CFW5_NAME_S, CFW5_LABEL_S, ISS_OFF);
	IUFillSwitch(&m_icfw_type_s[5], CFW6_NAME_S, CFW6_LABEL_S, ISS_OFF);
	IUFillSwitch(&m_icfw_type_s[6], CFW7_NAME_S, CFW7_LABEL_S, ISS_OFF);
	IUFillSwitch(&m_icfw_type_s[7], CFW8_NAME_S, CFW8_LABEL_S, ISS_OFF);
	IUFillSwitch(&m_icfw_type_s[8], CFW9_NAME_S, CFW9_LABEL_S, ISS_OFF);
	#ifdef	 USE_CFW_AUTO
	IUFillSwitch(&m_icfw_type_s[9], CFW10_NAME_S, CFW10_LABEL_S, ISS_OFF);
	#endif
  IUFillSwitchVector(&m_icfw_type_sp, 
									m_icfw_type_s, 
									NARRAY(m_icfw_type_s), 
									DEVICE_NAME, 
									CFW_TYPE_NAME_SP, 
									CFW_TYPE_LABEL_SP, 
									CFW_GROUP, 
									IP_RW, 
									ISR_1OFMANY, 
									INDI_TIMEOUT, 
									IPS_IDLE);

	// CFW CONNECTION:
  	IUFillSwitch(&m_icfw_connection_s[0], CONNECT_NAME_S, CONNECT_LABEL_S, ISS_OFF);
  	IUFillSwitch(&m_icfw_connection_s[1], DISCONNECT_NAME_S, DISCONNECT_LABEL_S, ISS_ON);
  	IUFillSwitchVector(&m_icfw_connection_sp, 
										m_icfw_connection_s, 
										NARRAY(m_icfw_connection_s), 
										DEVICE_NAME, 
										CFW_CONNECTION_NAME_SP, 
										CFW_CONNECTION_LABEL_SP, 
										CFW_GROUP, 
										IP_RW, 
										ISR_1OFMANY, 
										INDI_TIMEOUT, 
										IPS_IDLE);

	// CFW_SLOT:
  	IUFillNumber(	&m_icfw_slot_n[0], CFW_SLOT_NAME_N, CFW_SLOT_LABEL_N, 
			   				"%.0f", MIN_FILTER_SLOT, MAX_FILTER_SLOT, 
								FILTER_SLOT_STEP, DEF_FILTER_SLOT);
  	IUFillNumberVector(&m_icfw_slot_np, 
										m_icfw_slot_n, 
										NARRAY(m_icfw_slot_n), 
										DEVICE_NAME, 
										CFW_SLOT_NAME_NP, 
										CFW_SLOT_LABEL_NP, 
										CFW_GROUP, 
										IP_RW, 
										INDI_TIMEOUT, 
										IPS_IDLE);

	// CCD EXPOSE DURATION:
  IUFillNumber(	&m_icam_expose_time_n[0], CCD_EXPOSE_DURATION_NAME_N, 
							CCD_EXPOSE_DURATION_LABEL_N, "%.2f", MIN_EXP_TIME, MAX_EXP_TIME, 
							EXP_TIME_STEP, DEF_EXP_TIME);
  IUFillNumberVector(&m_icam_expose_time_np, 
									m_icam_expose_time_n, 
									NARRAY(m_icam_expose_time_n), 
									DEVICE_NAME, 
									CCD_EXPOSE_DURATION_NAME_NP, 
									CCD_EXPOSE_DURATION_LABEL_NP, 
									EXPOSURE_GROUP, 
									IP_RW, 
									INDI_TIMEOUT, 
									IPS_IDLE);

 	// BLOB - Binary Large Object:
  	strcpy(m_icam_fits_b.name, 	BLOB_NAME_B);
  	strcpy(m_icam_fits_b.label, 	BLOB_LABEL_B);
  	strcpy(m_icam_fits_b.format,	BLOB_FORMAT_B);
  	m_icam_fits_b.blob    = 0;
  	m_icam_fits_b.bloblen = 0;
  	m_icam_fits_b.size    = 0;
  	m_icam_fits_b.bvp     = 0;
  	m_icam_fits_b.aux0    = 0;
  	m_icam_fits_b.aux1    = 0;
  	m_icam_fits_b.aux2    = 0;

  	strcpy(m_icam_fits_bp.device,	DEVICE_NAME);
  	strcpy(m_icam_fits_bp.name, 	BLOB_NAME_BP);
  	strcpy(m_icam_fits_bp.label, 	BLOB_LABEL_BP);
  	strcpy(m_icam_fits_bp.group,	EXPOSURE_GROUP);
  	strcpy(m_icam_fits_bp.timestamp, "");
  	m_icam_fits_bp.p				= IP_RO;
  	m_icam_fits_bp.timeout	= INDI_TIMEOUT;
  	m_icam_fits_bp.s				= IPS_IDLE;
  	m_icam_fits_bp.bp			= &m_icam_fits_b;
  	m_icam_fits_bp.nbp			= 1;
  	m_icam_fits_bp.aux			= 0;

	// FITS file name:
	IUFillText(&m_icam_fits_name_t[0], FITS_NAME_T, FITS_LABEL_T, "");
	IUFillTextVector(	&m_icam_fits_name_tp, 
									m_icam_fits_name_t,
									NARRAY(m_icam_fits_name_t),
									DEVICE_NAME, 
									FITS_NAME_TP,
									FITS_LABEL_TP,
									EXPOSURE_GROUP,
									IP_RO,
									INDI_TIMEOUT, 
									IPS_IDLE);

 	#endif // INDI
}
//==========================================================================
#ifdef INDI
void SbigCam::ISGetProperties()
{
	// When a client first connects to the driver, we will offer only 3 basic 
	// properties. After the camera is later detected, we will offer full 
	// set of properties depending on the camera type.

	// CAMERA GROUP:		
	IDDefText(&m_icam_product_tp, 0);				// 1. CCD product	
	IDDefText(&m_icam_device_port_tp, 0);		// 2. CCD device port
	IDDefSwitch(&m_icam_connection_sp, 0);	// 3. CCD connection
}
#endif // INDI
//==========================================================================
#ifdef INDI
int SbigCam::UpdateProperties()
{
	int			res = CE_NO_ERROR;
	string 	msg;
	IText 		*pIText;

	// TEMPERATURE GROUP:
	IDDelete(DEVICE_NAME, CCD_FAN_NAME_SP, 0);
	IDDelete(DEVICE_NAME, CCD_TEMPERATURE_NAME_NP, 0);
	IDDelete(DEVICE_NAME, CCD_COOLER_NAME_NP, 0);
	IDDelete(DEVICE_NAME, CCD_TEMPERATURE_POLLING_NAME_NP, 0);
	IDDelete(DEVICE_NAME, CCD_TEMPERATURE_MSG_NAME_SP, 0);

	// CFW GROUP:
	IDDelete(DEVICE_NAME, CFW_PRODUCT_NAME_TP, 0);
	IDDelete(DEVICE_NAME, CFW_TYPE_NAME_SP, 0);
	IDDelete(DEVICE_NAME, CFW_CONNECTION_NAME_SP, 0);
	IDDelete(DEVICE_NAME, CFW_SLOT_NAME_NP, 0);

	// FRAME GROUP:
	IDDelete(DEVICE_NAME, CCD_FRAME_TYPE_NAME_NP, 0);
	IDDelete(DEVICE_NAME, CCD_REQUEST_NAME_SP, 0);
	IDDelete(DEVICE_NAME, CCD_PIXEL_INFO_NAME_NP, 0);

	#ifdef USE_CCD_BINNING_STANDARD_PROPERTY
	IDDelete(DEVICE_NAME, CCD_BINNING_NAME_NP, 0);
	#else
	IDDelete(DEVICE_NAME, CCD_BINNING_MODE_NAME_SP, 0);
	#endif	

	#ifdef USE_CCD_FRAME_STANDARD_PROPERTY
	IDDelete(DEVICE_NAME, CCD_FRAME_NAME_NP, 0);
	#else
	IDDelete(DEVICE_NAME, CCD_FRAME_X_NAME_NP, 0);
	IDDelete(DEVICE_NAME, CCD_FRAME_Y_NAME_NP, 0);
	IDDelete(DEVICE_NAME, CCD_FRAME_W_NAME_NP, 0);
	IDDelete(DEVICE_NAME, CCD_FRAME_H_NAME_NP, 0);
	#endif

	// EXPOSURE GROUP:
	IDDelete(DEVICE_NAME, CCD_EXPOSE_DURATION_NAME_NP, 0);
	IDDelete(DEVICE_NAME, FITS_NAME_TP, 0);
	IDDelete(DEVICE_NAME, BLOB_NAME_BP, 0);

	// Create new properties:
	if(GetCameraType() == NO_CAMERA){
		// Device is closed. We again offer only three basic properties,
		// namely: CCD_PRODUCT, CCD_DEVICE_PORT & CCD_CONNECTION.

		// CCD PRODUCT:
		pIText = IUFindText(&m_icam_product_tp, PRODUCT_NAME_T); 
		if(pIText) IUSaveText(pIText, UNKNOWN_LABEL);
		pIText = IUFindText(&m_icam_product_tp, PRODUCT_ID_NAME_T); 
		if(pIText) IUSaveText(pIText, UNKNOWN_LABEL);
		m_icam_product_tp.s = IPS_IDLE;
		IDSetText(&m_icam_product_tp, 0);

		// CCD DEVICE PORT:
		m_icam_device_port_tp.s = IPS_IDLE;
		IDSetText(&m_icam_device_port_tp, 0);

		// CCD CONNECTION:
		m_icam_connection_s[0].s 	= ISS_OFF;
    m_icam_connection_s[1].s 	= ISS_ON;
		m_icam_connection_sp.s 	= IPS_IDLE;
		msg = "SBIG CCD camera is offline.";
		IDSetSwitch(&m_icam_connection_sp, "%s", msg.c_str());

	}else{

		// Device is open, so we offer the full set of properties 
		// which are supported by the detected camera.	

		// CCD PRODUCT:
		msg = GetCameraName();
		pIText = IUFindText(&m_icam_product_tp, PRODUCT_NAME_T); 
		if(pIText) IUSaveText(pIText, msg.c_str());
		msg = GetCameraID(); 
		pIText = IUFindText(&m_icam_product_tp, PRODUCT_ID_NAME_T); 
		if(pIText) IUSaveText(pIText, msg.c_str());
		m_icam_product_tp.s = IPS_OK;
		IDSetText(&m_icam_product_tp, 0);		

		// CCD DEVICE PORT:	
		IText *pIText = IUFindText(&m_icam_device_port_tp, PORT_NAME_T); 
		if(pIText) IUSaveText(pIText, GetDeviceName());
		m_icam_device_port_tp.s = IPS_OK;
		IDSetText(&m_icam_device_port_tp, 0);		

		// CCD CONNECTION:
 		m_icam_connection_s[0].s 	= ISS_ON;
		m_icam_connection_s[1].s 	= ISS_OFF;
		m_icam_connection_sp.s 	= IPS_OK;
		msg = IUFindText(&m_icam_product_tp, PRODUCT_NAME_T)->text;
		msg += " is online. SN: ";
		msg += IUFindText(&m_icam_product_tp, PRODUCT_ID_NAME_T)->text;
		IDSetSwitch(&m_icam_connection_sp, "%s", msg.c_str());

		// CCD FAN:				
		if(IsFanControlAvailable()){
			IDDefSwitch(&m_icam_fan_state_sp, 0);	
		}

		// CCD TEMPERATURE:
		m_icam_temperature_np.s = IPS_BUSY;
		IDDefNumber(&m_icam_temperature_np, 0); 
		res = SetTemperatureRegulation(m_icam_temperature_n[0].value);
		if(res == CE_NO_ERROR){
    	// Set property to busy and poll in UpdateTemperature for CCD temp
			IDSetNumber(&m_icam_temperature_np, 
									"Setting CCD temperature to %+.1f [C].", 
									m_icam_temperature_n[0].value);
		}else{
   		m_icam_temperature_np.s = IPS_ALERT;
			IDSetNumber(&m_icam_temperature_np, 
									"Error: Cannot set CCD temperature to %+.1f [C]. %s",
									m_icam_temperature_n[0].value, 
									GetErrorString(res).c_str());
		}	

		// CCD COOLER:
		m_icam_cooler_np.s = IPS_BUSY;
		IDDefNumber(&m_icam_cooler_np, 0);

		// CCD TEMPERATURE POOLING:
		m_icam_temperature_polling_np.s = IPS_OK;
		IDDefNumber(&m_icam_temperature_polling_np, 0);		

		// CCD TEMPERATURE MSG:
		m_icam_temperature_msg_sp.s = IPS_OK;
		IDDefSwitch(&m_icam_temperature_msg_sp, 0);

		// CFW PRODUCT:
		IDDefText(&m_icfw_product_tp, 0);

		// CFW TYPE:
		IDDefSwitch(&m_icfw_type_sp, 0);

		// CFW CONNECTION:
		IDDefSwitch(&m_icfw_connection_sp, 0);

		// CFW SLOT:
		IDDefNumber(&m_icfw_slot_np, 0);		

		// CCD FRAME TYPE: 
		IDDefSwitch(&m_icam_frame_type_sp, 0);

 		// CCD REQUEST:
		if(GetNumOfCcdChips() > 1) IDDefSwitch(&m_icam_ccd_request_sp, 0); 

		// CCD BINNING: 	
		#ifdef USE_CCD_BINNING_STANDARD_PROPERTY
  	IDDefNumber(&m_icam_ccd_binning_np, 0);
		#else	 
  	IDDefSwitch(&m_icam_binning_mode_sp, 0);
		#endif
		UpdateCcdFrameProperties();

		// CCD PIXEL INFO:
		IDDefNumber(&m_icam_pixel_size_np, 0);

		// CCD FRAME
		#ifdef USE_CCD_FRAME_STANDARD_PROPERTY
		IDDefNumber(&m_icam_ccd_frame_np, 0);
		#else
		IDDefNumber(&m_icam_frame_x_np, 0);
		IDDefNumber(&m_icam_frame_y_np, 0);
		IDDefNumber(&m_icam_frame_w_np, 0);
		IDDefNumber(&m_icam_frame_h_np, 0);
		#endif

		// CCD EXPOSE DURATION:
		IDDefNumber(&m_icam_expose_time_np, 0);

		// CCD BLOB NAME:
		IDDefBLOB(&m_icam_fits_bp, 0);

		// CCD FITS NAME:
		IDDefText(&m_icam_fits_name_tp, 0);
	}
	return(res);
}
#endif // INDI
//=========================================================================
#ifdef INDI
int SbigCam::UpdateCcdFrameProperties(bool bUpdateClient)
{	
	int 		res = CE_NO_ERROR, wCcd, hCcd, ccd, binning;
	double	wPixel, hPixel;

	if((res = GetSelectedCcdChip(ccd)) != CE_NO_ERROR) return(res);
 	if((res = GetSelectedCcdBinningMode(binning)) != CE_NO_ERROR) return(res);
	res = GetCcdSizeInfo(ccd, binning, wCcd, hCcd, wPixel, hPixel);

	if(res == CE_NO_ERROR){
		// CCD INFO:
		m_icam_pixel_size_n[0].value = wPixel;
		m_icam_pixel_size_n[1].value = hPixel;
		m_icam_pixel_size_np.s 				= IPS_OK;

		// CCD FRAME
		#ifdef USE_CCD_FRAME_STANDARD_PROPERTY
		// X
		m_icam_ccd_frame_n[0].min		= 0;
		m_icam_ccd_frame_n[0].max		= wCcd-1;
		m_icam_ccd_frame_n[0].value		= 0;
		// Y
		m_icam_ccd_frame_n[1].min		= 0;
		m_icam_ccd_frame_n[1].max		= hCcd-1;
		m_icam_ccd_frame_n[1].value		= 0;
		// WIDTH
		m_icam_ccd_frame_n[2].min		= 1;
		m_icam_ccd_frame_n[2].max		= wCcd;
		m_icam_ccd_frame_n[2].value		= wCcd;
		// HEIGHT
		m_icam_ccd_frame_n[3].min		= 1;
		m_icam_ccd_frame_n[3].max		= hCcd;
		m_icam_ccd_frame_n[3].value		= hCcd;
		// STATE
		m_icam_ccd_frame_np.s 				= IPS_OK;
		#else
		// CCD FRAME X:
		m_icam_frame_x_n[0].min		= 0;
		m_icam_frame_x_n[0].max		= 0;
		m_icam_frame_x_n[0].value	= 0;
		m_icam_frame_x_np.s 				= IPS_OK;
		// CCD FRAME Y:
		m_icam_frame_y_n[0].min		= 0;
		m_icam_frame_y_n[0].max		= 0;
		m_icam_frame_y_n[0].value	= 0;
		m_icam_frame_y_np.s 				= IPS_OK;
		// CCD FRAME W:
		m_icam_frame_w_n[0].min		= 1;
		m_icam_frame_w_n[0].max		= wCcd;
		m_icam_frame_w_n[0].value	= wCcd;
		m_icam_frame_w_np.s 				= IPS_OK;
		// CCD FRAME H:
		m_icam_frame_h_n[0].min		= 1;
		m_icam_frame_h_n[0].max		= hCcd;
		m_icam_frame_h_n[0].value 	= hCcd;
		// STATE
		m_icam_frame_h_np.s 				= IPS_OK;
		#endif

		if(bUpdateClient){
			IDSetNumber(&m_icam_pixel_size_np, 0);
			#ifdef USE_CCD_FRAME_STANDARD_PROPERTY		
			IDSetNumber(&m_icam_ccd_frame_np, 0);
			IUUpdateMinMax(&m_icam_ccd_frame_np);
			#else
			IDSetNumber(&m_icam_frame_x_np, 0);
			IDSetNumber(&m_icam_frame_y_np, 0);
			IDSetNumber(&m_icam_frame_w_np, 0);
			IDSetNumber(&m_icam_frame_h_np, 0);
 
			IUUpdateMinMax(&m_icam_frame_x_np);
			IUUpdateMinMax(&m_icam_frame_y_np);
			IUUpdateMinMax(&m_icam_frame_w_np);
			IUUpdateMinMax(&m_icam_frame_h_np);		
			#endif
		}
	}
	return(res);
}
#endif // INDI
//==========================================================================
#ifdef INDI
int SbigCam::GetSelectedCcdChip(int &ccd_request)
{
	int res = CE_NO_ERROR;
	ISwitch *p = IUFindOnSwitch(&m_icam_ccd_request_sp);
	if(p){
		if(!strcmp(p->name, CCD_IMAGING_NAME_S)){
 				ccd_request = CCD_IMAGING;
		}else if(!strcmp(p->name, CCD_TRACKING_NAME_S)){
 				ccd_request = CCD_TRACKING;
		}else if(!strcmp(p->name, CCD_EXT_TRACKING_NAME_S)){
		 		ccd_request = CCD_EXT_TRACKING;
		}else{
				res = CE_BAD_PARAMETER;			
				IDMessage(DEVICE_NAME, "Error: No CCD chip found! "
					 			  "[m_icam_ccd_request_sp]!");
		}
	}else{
		res = CE_OS_ERROR;
		IDMessage(DEVICE_NAME, "Error: No switch ON found! "
							"[m_icam_ccd_request_sp].");
	}
	return(res);
}
#endif // INDI
//==========================================================================
#ifdef INDI
int SbigCam::GetSelectedCcdBinningMode(int &binning)
{	
	int res = CE_NO_ERROR;

	#ifdef USE_CCD_BINNING_STANDARD_PROPERTY
	if((m_icam_ccd_binning_n[0].value == 1)&& 
		 (m_icam_ccd_binning_n[1].value == 1)){
		 binning = CCD_BIN_1x1_I;
	}else if((m_icam_ccd_binning_n[0].value == 2)&& 
					 (m_icam_ccd_binning_n[1].value == 2)){
					 binning = CCD_BIN_2x2_I;
	}else if((m_icam_ccd_binning_n[0].value == 3)&& 
					 (m_icam_ccd_binning_n[1].value == 3)){
					 binning = CCD_BIN_3x3_I;
	}else if((m_icam_ccd_binning_n[0].value == 9)&& 
					 (m_icam_ccd_binning_n[1].value == 9)){
					 binning = CCD_BIN_9x9_I;
	}else{
			res = CE_BAD_PARAMETER;
			IDMessage(DEVICE_NAME, "Error: Bad CCD binning mode! "
					 			  					"Use: 1x1, 2x2 or 3x3");
	}
	#else
	ISwitch *p = IUFindOnSwitch(&m_icam_binning_mode_sp);
	if(p){
		if(!strcmp(p->name, CCD_BIN_1x1_I_NAME_S)){
 				binning = CCD_BIN_1x1_I;
		}else if(!strcmp(p->name, CCD_BIN_2x2_I_NAME_S)){
 				binning = CCD_BIN_2x2_I;
		}else if(!strcmp(p->name, CCD_BIN_3x3_I_NAME_S)){
		 		binning = CCD_BIN_3x3_I;
		}else if(!strcmp(p->name, CCD_BIN_9x9_I_NAME_S)){
 				binning = CCD_BIN_9x9_I;
		}else if(!strcmp(p->name, CCD_BIN_2x2_E_NAME_S)){
 				binning = CCD_BIN_2x2_E;
		}else if(!strcmp(p->name, CCD_BIN_3x3_E_NAME_S)){
 				binning = CCD_BIN_3x3_E;
		}else{
				res = CE_BAD_PARAMETER;
				IDMessage(DEVICE_NAME, "Error: No CCD binning mode found! "
					 			  "[m_icam_binning_mode_sp]!");
		}
	}else{
		res = CE_OS_ERROR;
		IDMessage(DEVICE_NAME, "Error: No switch ON found! "
							"[m_icam_binning_mode_sp]");
	}
	#endif
	return(res);
}
#endif // INDI		
//==========================================================================
#ifdef INDI
int SbigCam::GetSelectedCcdFrameType(string &frame_type)
{		
	int res = CE_NO_ERROR; 
	ISwitch *p = IUFindOnSwitch(&m_icam_frame_type_sp);
	if(p){
 			frame_type = p->name;
	}else{
			res = CE_OS_ERROR;
			IDMessage(DEVICE_NAME, "Error: No switch ON found! "
							  "[m_icam_frame_type_sp]");
	}
	return(res);
}
#endif // INDI			
//==========================================================================
#ifdef INDI
int SbigCam::GetCcdShutterMode(int &shutter, int ccd)
{	 
	string 	frame_type;
	int res = GetSelectedCcdFrameType(frame_type);
	if(res != CE_NO_ERROR) return(res);

	if(	!strcmp(frame_type.c_str(), CCD_FRAME_LIGHT_NAME_N)	||
			!strcmp(frame_type.c_str(), CCD_FRAME_FLAT_NAME_N)		||
			!strcmp(frame_type.c_str(), CCD_FRAME_BIAS_NAME_N)		){
			if(ccd == CCD_EXT_TRACKING){
					shutter = SC_OPEN_EXT_SHUTTER;
			}else{
					shutter = SC_OPEN_SHUTTER;
			}
	}else if(!strcmp(frame_type.c_str(), CCD_FRAME_DARK_NAME_N)){
			if(ccd == CCD_EXT_TRACKING){
					shutter = SC_CLOSE_EXT_SHUTTER;
			}else{
					shutter = SC_CLOSE_SHUTTER;
			}
	}else{
			res = CE_OS_ERROR;
			IDMessage(DEVICE_NAME, "Error: Unknown selected CCD frame type! "
							 	"[m_icam_frame_type_sp]");
	}
	return(res);
}
#endif // INDI	
//==========================================================================
#ifdef INDI
void SbigCam::ISNewSwitch(const char *name, ISState *states, char *names[], int num)
{
	int				res;
	string			str;
	CFWResults	cfwr;
 
	// CCD CONNECTION:
	if(!strcmp(name, m_icam_connection_sp.name)){
  	IUResetSwitch(&m_icam_connection_sp);
  	IUUpdateSwitch(&m_icam_connection_sp, states, names, num);
		// Check open/close request:
		if(m_icam_connection_s[0].s == ISS_ON){
			
				// Open device:
				if((res = OpenDevice(m_icam_device_port_tp.tp->text)) == CE_NO_ERROR){
						// Establish link:
						if((res = EstablishLink()) == CE_NO_ERROR){
								// Link established.
								UpdateProperties();					
						}else{
								// Establish link error.
								m_icam_connection_s[0].s 	= ISS_OFF;
      					m_icam_connection_s[1].s 	= ISS_ON;
								m_icam_connection_sp.s 	= IPS_IDLE;
								str = "Error: Cannot establish link to SBIG CCD camera. ";
								str += GetErrorString(res); 
								IDSetSwitch(&m_icam_connection_sp, "%s", str.c_str());
						}
				}else{
						// Open device error.
						m_icam_connection_s[0].s 	= ISS_OFF;
      			m_icam_connection_s[1].s 	= ISS_ON;
						m_icam_connection_sp.s 	= IPS_IDLE;
						str = "Error: Cannot open SBIG CCD camera device. ";
						str += GetErrorString(res);   
						IDSetSwitch(&m_icam_connection_sp, "%s", str.c_str());
				}
		}else{
				// Close device.
				if((res = CloseDevice()) == CE_NO_ERROR){
						UpdateProperties();	
				}else{
						// Close device error:
						m_icam_connection_s[0].s 	= ISS_ON;
						m_icam_connection_s[1].s 	= ISS_OFF;
						m_icam_connection_sp.s 	= IPS_ALERT;
						str = "Error: Cannot close SBIG CCD camera device. ";
						str += GetErrorString(res);   
						IDSetSwitch(&m_icam_connection_sp, "%s", str.c_str());
				}
		}
		return;
	}

	// CCD REQUEST:
  if(!strcmp(name, m_icam_ccd_request_sp.name)){
			if(CheckConnection(&m_icam_ccd_request_sp) == false) return;
			IUResetSwitch(&m_icam_ccd_request_sp);
			IUUpdateSwitch(&m_icam_ccd_request_sp, states, names, num);
			m_icam_ccd_request_sp.s = IPS_OK;
			IDSetSwitch(&m_icam_ccd_request_sp, 0);
			UpdateCcdFrameProperties(true);
			return;
	}	

	// CCD FAN:
  if(!strcmp(name, m_icam_fan_state_sp.name)){
		if(CheckConnection(&m_icam_fan_state_sp) == false) return;
		IUResetSwitch(&m_icam_fan_state_sp);
		IUUpdateSwitch(&m_icam_fan_state_sp, states, names, num);
		// Switch FAN ON/OFF:
		MiscellaneousControlParams mcp;
  	if(m_icam_fan_state_s[0].s == ISS_ON){
   			mcp.fanEnable = 1;
  	}else{
   			mcp.fanEnable = 0;
  	}
  	mcp.shutterCommand = SC_LEAVE_SHUTTER;    
  	mcp.ledState  = LED_OFF;  
  	if((res = MiscellaneousControl(&mcp)) == CE_NO_ERROR){     
			m_icam_fan_state_sp.s = IPS_OK;
			if(mcp.fanEnable == 1){
					str = "Fan turned ON.";
			}else{
					str = "Fan turned OFF.";
			}
		}else{
			m_icam_fan_state_sp.s = IPS_ALERT;
			if(mcp.fanEnable == 1){
					str = "Error: Cannot turn Fan ON. ";
			}else{
					str = "Error: Cannot turn Fan OFF.";
			}
			str += GetErrorString(res);  
		}
		IDSetSwitch(&m_icam_fan_state_sp, "%s", str.c_str());
		return;
	}	

	// CCD FRAME TYPE: 
  if(!strcmp(name, m_icam_frame_type_sp.name)){
		IUResetSwitch(&m_icam_frame_type_sp);
		IUUpdateSwitch(&m_icam_frame_type_sp, states, names, num);
		m_icam_frame_type_sp.s = IPS_OK;
		IDSetSwitch(&m_icam_frame_type_sp, 0);
		return;
	}

	// CCD BINNING:
	#ifndef USE_CCD_BINNING_STANDARD_PROPERTY
  if(!strcmp(name, m_icam_binning_mode_sp.name)){
		if(CheckConnection(&m_icam_binning_mode_sp) == false) return;
		IUResetSwitch(&m_icam_binning_mode_sp);
		IUUpdateSwitch(&m_icam_binning_mode_sp, states, names, num);
		m_icam_binning_mode_sp.s = IPS_OK;
		IDSetSwitch(&m_icam_binning_mode_sp, 0);
		UpdateCcdFrameProperties(true);
		return;
	}
	#endif

	// CCD TEMPERATURE: 
  if(!strcmp(name, m_icam_temperature_msg_sp.name)){
		IUResetSwitch(&m_icam_temperature_msg_sp);
		IUUpdateSwitch(&m_icam_temperature_msg_sp, states, names, num);
		m_icam_temperature_msg_sp.s = IPS_OK;
		IDSetSwitch(&m_icam_temperature_msg_sp, 0);
		return;
	}

	// CFW TYPE:
  if(!strcmp(name, m_icfw_type_sp.name)){
		if(CheckConnection(&m_icfw_type_sp) == false) return;
		// Allow change of CFW's type only if not already connected.
		if(m_icfw_connection_s[0].s == ISS_OFF){
			IUResetSwitch(&m_icfw_type_sp);
			IUUpdateSwitch(&m_icfw_type_sp, states, names, num);
			str = "";
		}else{
			str = "Cannot change CFW type while connected!";
		}
		m_icfw_type_sp.s = IPS_OK;
		IDSetSwitch(&m_icfw_type_sp, "%s", str.c_str());
		return;
	}

	// CFW CONNECTION:
  if(!strcmp(name, m_icfw_connection_sp.name)){
		if(CheckConnection(&m_icfw_connection_sp) == false) return;
		IUResetSwitch(&m_icfw_connection_sp);
		IUUpdateSwitch(&m_icfw_connection_sp, states, names, num);
		m_icfw_connection_sp.s = IPS_BUSY;
		IDSetSwitch(&m_icfw_connection_sp, 0);
		if(m_icfw_connection_s[0].s == ISS_ON){
			// Open device.	
			if(CfwConnect() == CE_NO_ERROR){
				m_icfw_connection_sp.s = IPS_OK;
				IDSetSwitch(&m_icfw_connection_sp, "CFW connected.");
			}else{
				m_icfw_connection_sp.s = IPS_ALERT;
				IDSetSwitch(&m_icfw_connection_sp, "CFW connection error!");
			}
		}else{
			// Close device.
			if(CfwDisconnect() == CE_NO_ERROR){
				m_icfw_connection_sp.s = IPS_ALERT;
				IDSetSwitch(&m_icfw_connection_sp, "CFW disconnection error!");
			}else{	
				// Update CFW's Product/ID texts.
				cfwr.cfwModel 		= CFWSEL_UNKNOWN;
				cfwr.cfwPosition = CFWP_UNKNOWN;
				cfwr.cfwStatus		=	CFWS_UNKNOWN;
				cfwr.cfwError			= CFWE_DEVICE_NOT_OPEN;
				cfwr.cfwResult1		= 0;
				cfwr.cfwResult2		= 0;
				CfwUpdateProperties(cfwr);
				// Remove connection text.
				m_icfw_connection_sp.s = IPS_IDLE;
				IDSetSwitch(&m_icfw_connection_sp, "CFW disconnected.");
			}
		}
		return;
	}
}
#endif // INDI
//==========================================================================
#ifdef INDI
void SbigCam::ISNewText(	const char *name, char *texts[], char	 *names[], int /*num*/)
{
	string str;

	// CCD DEVICE PORT: 
	if(!strcmp(name, m_icam_device_port_tp.name)){
		IText *pIText = IUFindText(&m_icam_device_port_tp, names[0]); 
		if(pIText) IUSaveText(pIText, texts[0]);
		m_icam_device_port_tp.s = IPS_OK;
		IDSetText(&m_icam_device_port_tp, 0);
	}
}
#endif // INDI
//==========================================================================
#ifdef INDI
void SbigCam::ISNewNumber(const char *name, double values[], char *names[], int num)
{
	int 			res = CE_NO_ERROR, wCcd, hCcd, ccd, binning;
	double		wPixel, hPixel;

	// CCD EXPOSE DURATION: 	
	if(!strcmp(name, m_icam_expose_time_np.name)){
		IUUpdateNumber(&m_icam_expose_time_np, values, names, num);
		if(m_icam_expose_time_np.s == IPS_BUSY){
				StopExposure();
		}else{
				StartExposure();
		}	
	}

	// CCD TEMPERATURE:
 	if(!strcmp(name, m_icam_temperature_np.name)){
    if(CheckConnection(&m_icam_temperature_np) == false) return;
		if(values[0] < MIN_CCD_TEMP || values[0] > MAX_CCD_TEMP){
      m_icam_temperature_np.s = IPS_IDLE;
			IDSetNumber(&m_icam_temperature_np, 
									"Error: Bad temperature value! "
									"Range is [%.1f, %.1f] [C].", 
									MIN_CCD_TEMP, MAX_CCD_TEMP);
      return;
    }
		if((res = SetTemperatureRegulation(values[0])) == CE_NO_ERROR){
    	// Set property to busy and poll in ISPoll for CCD temp
			m_icam_temperature_n[0].value = values[0];
    	m_icam_temperature_np.s = IPS_BUSY;
			IDSetNumber(&m_icam_temperature_np, 
									"Setting CCD temperature to %+.1f [C].", 
									values[0]);
		}else{
   		m_icam_temperature_np.s = IPS_ALERT;
			IDSetNumber(&m_icam_temperature_np, 
									"Error: Cannot set CCD temperature to %+.1f [C]. %s",
									values[0], GetErrorString(res).c_str());
		}
	}
  
	// CCD TEMPERATURE POOLING:
	if(!strcmp(name, m_icam_temperature_polling_np.name)){
		m_icam_temperature_polling_np.s = IPS_OK;
		IUUpdateNumber(&m_icam_temperature_polling_np, values, names, num);
		IDSetNumber(&m_icam_temperature_polling_np, 0);
	} 

	// CCD BINNING:
	#ifdef USE_CCD_BINNING_STANDARD_PROPERTY
	if(!strcmp(name, m_icam_ccd_binning_np.name)){
		m_icam_ccd_binning_np.s = IPS_OK;
		// Update the values according to the actual CCD binning mode possibilities.
		// HOR_BIN == value[0], VER_BIN == value[1]
		if(values[0] != values[1]) values[1] = values[0];
		IUUpdateNumber(&m_icam_ccd_binning_np, values, names, num);
		IDSetNumber(&m_icam_ccd_binning_np, 0);
		UpdateCcdFrameProperties(true);
	}
	#endif

	// CCD FRAME:
	#ifdef USE_CCD_FRAME_STANDARD_PROPERTY		
	if(!strcmp(name, m_icam_ccd_frame_np.name)){
		m_icam_ccd_frame_np.s = IPS_OK;
		// Update the values according to the actual CCD info.
		if((res = GetSelectedCcdChip(ccd)) == CE_NO_ERROR){
 			if((res = GetSelectedCcdBinningMode(binning)) == CE_NO_ERROR){
				res = GetCcdSizeInfo(ccd, binning, wCcd, hCcd, wPixel, hPixel);
				if(res == CE_NO_ERROR){
					// CCD_X + CCD_WIDTH
					if((values[0] + values[2]) >= wCcd){
							values[2] = wCcd - values[0];
					}  
					// CCD_Y + CCD_HEIGHT
					if((values[1] + values[3]) >= hCcd){
							values[3] = hCcd - values[1];
					}  
				}
			}
		}
		IUUpdateNumber(&m_icam_ccd_frame_np, values, names, num);
		IDSetNumber(&m_icam_ccd_frame_np, 0);
	} 
	#else
	// CCD FRAME X:
	if(!strcmp(name, m_icam_frame_x_np.name)){
		m_icam_frame_x_np.s = IPS_OK;
		IUUpdateNumber(&m_icam_frame_x_np, values, names, num);
		IDSetNumber(&m_icam_frame_x_np, 0);
	} 
	// CCD FRAME Y:
	if(!strcmp(name, m_icam_frame_y_np.name)){
		m_icam_frame_y_np.s = IPS_OK;
		IUUpdateNumber(&m_icam_frame_y_np, values, names, num);
		IDSetNumber(&m_icam_frame_y_np, 0);
	} 
	// CCD FRAME W:
	if(!strcmp(name, m_icam_frame_w_np.name)){
		m_icam_frame_w_np.s = IPS_OK;
		IUUpdateNumber(&m_icam_frame_w_np, values, names, num);
		IDSetNumber(&m_icam_frame_w_np, 0);
		// Update Min/Max of CCD_FRAME_X:
		m_icam_frame_x_n[0].max = 	m_icam_frame_w_n[0].max - 
									m_icam_frame_w_n[0].value - 1;
		m_icam_frame_x_np.s = IPS_OK;
		IUUpdateMinMax(&m_icam_frame_x_np);
	} 
	// CCD FRAME H:
	if(!strcmp(name, m_icam_frame_h_np.name)){
		m_icam_frame_h_np.s = IPS_OK;
		IUUpdateNumber(&m_icam_frame_h_np, values, names, num);
		IDSetNumber(&m_icam_frame_h_np, 0);
		// Update Min/Max of CCD_FRAME_Y:
		m_icam_frame_y_n[0].max = 	m_icam_frame_h_n[0].max - 
									m_icam_frame_h_n[0].value - 1;
		m_icam_frame_y_np.s = IPS_OK;
		IUUpdateMinMax(&m_icam_frame_y_np);
	} 
	#endif

	// CFW SLOT:
	CFWResults 	cfwr;
	int 					type;
	char					str[64];

	if(!strcmp(name, m_icfw_slot_np.name)){
		// Use CFW's GOTO only if already connected:
		if(m_icfw_connection_s[0].s != ISS_ON) return;
		m_icfw_slot_np.s = IPS_BUSY;
		IDSetNumber(&m_icfw_slot_np, 0);
		IUUpdateNumber(&m_icfw_slot_np, values, names, num);
		if(CfwGoto(&cfwr) == CE_NO_ERROR){
			type = GetCfwSelType();
			if(type == CFWSEL_CFW6A || type == CFWSEL_CFW8){
				sprintf(str, "CFW position reached.");
			}else{
				sprintf(str, "CFW position %d reached.", cfwr.cfwPosition);
			}
			m_icfw_slot_n[0].value = cfwr.cfwPosition;
			m_icfw_slot_np.s = IPS_OK;
		}else{				
			// CFW error occurred, so report all the available infos to the client:
			CfwShowResults("CFWGoto:", cfwr);
			m_icfw_slot_np.s = IPS_ALERT;
			sprintf(str, "Please Connect/Disconnect CFW, than try again...");
		}
		IDSetNumber(&m_icfw_slot_np, "%s", str);
	} 
}
#endif // INDI
//==========================================================================
#ifdef INDI
bool SbigCam::CheckConnection(ISwitchVectorProperty *vp)
{
	if(m_icam_connection_sp.s != IPS_OK){
		IDMessage(DEVICE_NAME, 
				 			"Cannot change property '%s' while the CCD is offline.", 
				 			vp->name);
    					vp->s = IPS_IDLE;
    					IDSetSwitch(vp, NULL);
    return(false);
	}
	return(true);
}
#endif // INDI
//==========================================================================
#ifdef INDI
bool SbigCam::CheckConnection(INumberVectorProperty *vp)
{
	if(m_icam_connection_sp.s != IPS_OK){
		IDMessage(DEVICE_NAME, 
				 			"Cannot change property '%s' while the CCD is offline.", 
				 			vp->name);
		vp->s = IPS_IDLE;
		IDSetNumber(vp, NULL);
		return(false);
  }
  return(true);
}
#endif // INDI
//==========================================================================
#ifdef INDI
bool SbigCam::CheckConnection(ITextVectorProperty *vp)
{
	if(m_icam_connection_sp.s != IPS_OK){
		IDMessage(DEVICE_NAME, 
				 			"Cannot change property '%s' while the CCD is offline.", 
				 			vp->name);
		vp->s = IPS_IDLE;
		IDSetText(vp, NULL);
		return(false);
	}
	return(true);
}
#endif // INDI
//==========================================================================
#ifdef INDI
void SbigCam::UpdateTemperature(void *p)
{
	SbigCam *pSbigCam = (SbigCam *)p;
	if(pSbigCam->CheckLink()) pSbigCam->UpdateTemperature();
	IEAddTimer(	pSbigCam->GetCcdTemperaturePoolingTime(), 
							SbigCam::UpdateTemperature, pSbigCam);
}
#endif // INDI
//==========================================================================
#ifdef INDI
void SbigCam::UpdateTemperature()
{    		
  	bool   	enabled;
  	double 	ccdTemp, setpointTemp, percentTE, power;
	
	// Get temperature status, ignore possible errors.
	if(QueryTemperatureStatus(enabled, ccdTemp, setpointTemp, percentTE) == CE_NO_ERROR){
		// Compare the current temperature against the setpoint value:
		if(fabs(setpointTemp - ccdTemp) <= TEMP_DIFF){
				m_icam_temperature_np.s = IPS_OK;
		}else{
				m_icam_temperature_np.s = IPS_BUSY;
		} 
		m_icam_temperature_n[0].value = ccdTemp;
		// Check the TE cooler if inside the range:
		power = 100.0 * percentTE;					
		if(power <= CCD_COOLER_THRESHOLD){
				m_icam_cooler_np.s = IPS_OK;
		}else{
				m_icam_cooler_np.s = IPS_BUSY;
		}
		m_icam_cooler_n[0].value = power;
		// Update the client's properties:
		if(m_icam_temperature_msg_s[0].s == ISS_ON){
				IDSetNumber(&m_icam_temperature_np, 
										"CCD temperature %+.1f [C], TE cooler: %.1f [%%].", 
										ccdTemp, power);
		}else{
				IDSetNumber(&m_icam_temperature_np, 0);
		}
		IDSetNumber(&m_icam_cooler_np, 0);
	}	
}
#endif // INDI
//==========================================================================
#ifdef INDI
int SbigCam::GetCcdTemperaturePoolingTime()
{		
	return((int)(m_icam_temperature_polling_n[0].value * 1000));
}
#endif // INDI
//==========================================================================
#ifdef INDI
void SbigCam::UpdateExposure(void *p)
{
	SbigCam *pSbigCam = (SbigCam *)p;
	if(pSbigCam->CheckLink()) pSbigCam->UpdateExposure();
	IEAddTimer(POLL_EXPOSURE_MS, SbigCam::UpdateExposure, pSbigCam);
}
#endif // INDI
//==========================================================================
#ifdef INDI
int SbigCam::StartExposure()
{ 		
	int res;

	// Sanity check:
	int ccd, binning, shutter;
 	if((res = GetSelectedCcdChip(ccd)) != CE_NO_ERROR) return(res);
	if((res = GetCcdShutterMode(shutter, ccd)) != CE_NO_ERROR) return(res);
	if((res = GetSelectedCcdBinningMode(binning))!= CE_NO_ERROR) return(res);

	// Is the expose time zero ?
 	if(m_icam_expose_time_n[0].value == 0){
			m_icam_expose_time_np.s = IPS_ALERT;
			IDSetNumber(&m_icam_expose_time_np, 0);
			IDMessage(DEVICE_NAME, "Please set non-zero exposure time and try again."); 
			return(CE_BAD_PARAMETER);
	}
 	
	// Save the current temperature because needed for the FITS file:
  bool   	enabled;
  double 	ccdTemp, setpointTemp, percentTE;
	res = QueryTemperatureStatus(enabled, ccdTemp, setpointTemp, percentTE);
	if(res == CE_NO_ERROR){
			SaveTemperature(ccdTemp);
	}else{
			SaveTemperature(0.0);
	}

	// Save exposure time, necessary for FITS file:
	SaveExposeTime(m_icam_expose_time_n[0].value);

 	// Calculate an expose time:
  ulong expTime = (ulong)floor(m_icam_expose_time_n[0].value * 100.0 + 0.5);

  // Start exposure:
	StartExposureParams	sep;
  sep.ccd = (unsigned short)ccd;
  sep.abgState = (unsigned short)ABG_LOW7;
  sep.openShutter = (unsigned short)shutter;
  sep.exposureTime = expTime;
	if((res = StartExposure(&sep)) != CE_NO_ERROR) return(res);

	// Save start time of the exposure:
	SetStartExposureTimestamp(timestamp());

	// Update client's property:
	string msg, frame_type;
	if((res = GetSelectedCcdFrameType(frame_type)) != CE_NO_ERROR) return(res);

	// Update the expose time property:
	m_icam_expose_time_np.s = IPS_BUSY;
	IDSetNumber(&m_icam_expose_time_np, 0);

	// Update FITS file name:
	IUFillText(&m_icam_fits_name_t[0], FITS_NAME_T, FITS_LABEL_T, "");
	m_icam_fits_name_tp.s = IPS_IDLE;
	IDSetText(&m_icam_fits_name_tp, 0);

	// Update BLOB property:
 	SetBlobState(IPS_BUSY);

	// Update exposure action button properties:	
	if(!strcmp(frame_type.c_str(), CCD_FRAME_LIGHT_NAME_N)){
			msg = "LF exposure in progress...";
	}else if(!strcmp(frame_type.c_str(), CCD_FRAME_DARK_NAME_N)){
			msg = "DF exposure in progress...";
	}else if(!strcmp(frame_type.c_str(), CCD_FRAME_FLAT_NAME_N)){
			msg = "FF exposure in progress...";
	}else if(!strcmp(frame_type.c_str(), CCD_FRAME_BIAS_NAME_N)){
			msg = "BF exposure in progress...";
	}
	IDMessage(DEVICE_NAME, "%s", msg.c_str());

	return(res);
}
#endif // INDI
//==========================================================================
#ifdef INDI
int SbigCam::StopExposure()
{	
	int 			res, ccd;
	string		msg;

	if((res = GetSelectedCcdChip(ccd)) != CE_NO_ERROR) return(res);

	// END_EXPOSURE:
	EndExposureParams eep;
	eep.ccd = (unsigned short)ccd;
	res = EndExposure(&eep);

	// Update expose time property:
	m_icam_expose_time_n[0].value = 0;
  if(res == CE_NO_ERROR){
			m_icam_expose_time_np.s = IPS_IDLE;
			msg = "Exposure cancelled.";
	}else{
			m_icam_expose_time_np.s = IPS_ALERT;
			msg = "Stop exposure error.";
	}	
	IDSetNumber(&m_icam_expose_time_np, 0);
	IDMessage(DEVICE_NAME, "%s", msg.c_str());

	// Update BLOB property:
	SetBlobState(IPS_IDLE);

	return(res);
}
#endif // INDI		
//==========================================================================
#ifdef INDI
void SbigCam::UpdateExposure()
{
	// If no expose in progress, then return:
	if(m_icam_expose_time_np.s != IPS_BUSY) return;
	
	int ccd;
	if(GetSelectedCcdChip(ccd) != CE_NO_ERROR) return;

  EndExposureParams					eep;
	QueryCommandStatusParams		qcsp;
  QueryCommandStatusResults	qcsr;

	// Query command status:
  qcsp.command = CC_START_EXPOSURE;
	if(QueryCommandStatus(&qcsp, &qcsr) != CE_NO_ERROR) return;

	int mask = 12; // Tracking & external tracking CCD chip mask.
  if(ccd == CCD_IMAGING) mask = 3; // Imaging chip mask.

	// Check exposure progress:	
	if((qcsr.status & mask) != mask){
			// The exposure is still in progress, decrement an 
			// exposure time:
			if(--m_icam_expose_time_n[0].value < 0){
 					m_icam_expose_time_n[0].value = 0;
			}
			// Update expose propery, but do not change its status now:
 			IDSetNumber(&m_icam_expose_time_np, 0);
			return;
	}
	
	// Exposure done - update client's property:	
	eep.ccd = ccd;
  EndExposure(&eep);

	// Get image size:
	#ifdef USE_CCD_FRAME_STANDARD_PROPERTY		
 	unsigned short left	= (unsigned short)m_icam_ccd_frame_n[0].value;
  unsigned short top		= (unsigned short)m_icam_ccd_frame_n[1].value;
	unsigned short width	= (unsigned short)m_icam_ccd_frame_n[2].value;
	unsigned short height	= (unsigned short)m_icam_ccd_frame_n[3].value;
	#else
 	unsigned short left	= (unsigned short)m_icam_frame_x_n[0].value;
  unsigned short top		= (unsigned short)m_icam_frame_y_n[0].value;
	unsigned short width	= (unsigned short)m_icam_frame_w_n[0].value;
	unsigned short height	= (unsigned short)m_icam_frame_h_n[0].value;
	#endif

	// Allocate image buffer:
 	//unsigned short **buffer = AllocateBuffer(width, height);
	unsigned short *buffer = AllocateBuffer(width, height);
	if(!buffer) return;

	// Readout CCD:
	IDMessage(DEVICE_NAME, "CCD readout in progress...");
	if(ReadoutCcd(left, top, width, height, buffer) != CE_NO_ERROR){
			ReleaseBuffer(height, buffer);
			IDMessage(DEVICE_NAME, "CCD readout error!");
	 		return;
	}

	// Create unige FITS name:
	string fits_name = CreateFitsName();

	// Write FITS:
	if(WriteFits(fits_name, width, height, buffer) != CE_NO_ERROR){
			ReleaseBuffer(height, buffer);
			IDMessage(DEVICE_NAME, "WriteFits error!");
	 		return;
	}

	// Release image buffer:
	if(ReleaseBuffer(height, buffer) != CE_NO_ERROR){
			IDMessage(DEVICE_NAME, "ReleaseBuffer error!");
 			return;
	}

	// Upload FITS file name:
	IUFillText(&m_icam_fits_name_t[0], FITS_NAME_T, FITS_LABEL_T, fits_name.c_str());
	m_icam_fits_name_tp.s = IPS_OK;
	IDSetText(&m_icam_fits_name_tp, 0);

	// Upload FITS file data:
	if (( UploadFits(fits_name) != CE_NO_ERROR)) return;
	
	// Update exposure time properties:
	m_icam_expose_time_n[0].value = GetExposeTime();
	m_icam_expose_time_np.s = IPS_OK;
 	IDSetNumber(&m_icam_expose_time_np, 0);

	// Update BLOB property:
	//SetBlobState(IPS_OK);

	// Send exposure done message:
	IDMessage(DEVICE_NAME, "CCD exposure done!");
}
#endif // INDI
//==========================================================================
#ifdef INDI
unsigned short *SbigCam::AllocateBuffer(unsigned short width, unsigned short height)
{	
	unsigned short *buffer = 0;
	try
 	{
		// Allocate new image buffer:
 		//size_t wlen = width * sizeof(unsigned short);
 		buffer = new unsigned short [width * height];
		memset(buffer, 0, height * width * sizeof(unsigned short));
 		//memset(buffer, 0, height * sizeof(unsigned short*));
  	//for(int h = 0; h < height; h++){
	//			buffer[h] = new unsigned short [width];
     	//	memset(buffer[h], 0, wlen);
 		//}
 	}
 	catch(bad_alloc &exception)
 	{
  	ReleaseBuffer(height, buffer);
		buffer = 0;
		IDMessage(DEVICE_NAME, "Error: AllocateBuffer - exception!");
 	} 
	return(buffer);
}
#endif // INDI
//==========================================================================
#ifdef INDI
int SbigCam::ReleaseBuffer(unsigned short height, unsigned short *buffer)
{	
	if(!buffer) return(CE_NO_ERROR);

	// Release buffer now:
 	//for(int y = 0; y < height; y++){
	//		if(buffer[y]) delete [] buffer[y];
 	//}
 	delete [] buffer;
 	buffer = 0;
	return(CE_NO_ERROR);
}
#endif // INDI	
//==========================================================================
#ifdef INDI
int SbigCam::ReadoutCcd(	unsigned short left,	unsigned short top,
												unsigned short width,	unsigned short height,
												unsigned short *buffer)
{	
	int h, w, ccd, binning, res;
 	if((res = GetSelectedCcdChip(ccd)) != CE_NO_ERROR) return(res);
	if((res = GetSelectedCcdBinningMode(binning)) != CE_NO_ERROR) return(res);
 
	StartReadoutParams srp;
  srp.ccd 					= ccd;
  srp.readoutMode	= binning;
  srp.left					= left;
  srp.top					= top;
  srp.width				= width;
  srp.height				= height;
  if((res = StartReadout(&srp)) != CE_NO_ERROR){
			IDMessage(DEVICE_NAME, "ReadoutCcd - StartReadout error!");
 			return(res);
  }

  	// Readout lines.
  ReadoutLineParams rlp;
  rlp.ccd					= ccd;
  rlp.readoutMode	= binning;
  rlp.pixelStart		= left;
  rlp.pixelLength	= width;

	// Readout CCD row by row:
	for(h = 0; h < height; h++){
			ReadoutLine(&rlp, buffer + (h * width), false);
 	}

	// Perform little endian to big endian (network order) 
	// conversion since FITS is stored in big endian. 
	/*for(h = 0; h < height; h++){
			for(w = 0 ; w < width; w++){
					//buffer[h][w] = GET_BIG_ENDIAN(buffer[h][w] - 32768);
					buffer[h * width + w] = GET_BIG_ENDIAN(buffer[h * width + w]);
 			}
	}*/

 	// End readout:
	EndReadoutParams	erp;
  erp.ccd = ccd;
  if((res = EndReadout(&erp)) != CE_NO_ERROR){
			IDMessage(DEVICE_NAME, "ReadoutCcd - EndReadout error!");
 			return(res);
	}

 	return(res);
}
#endif // INDI
//==========================================================================
#ifdef INDI
string SbigCam::CreateFitsName()
{	
	// Create unige FITS name:
	// Each file name has a form: XY_YYYY-MM-DDTHH:MM:SS.fits
	// where XY is: LF for taking light frame
	//				DF for taking dark frame
	//				BF for taking bias frame
	//				FF for taking flat field
	//				XX if file type is not recognized.
	string frame_type;
	GetSelectedCcdFrameType(frame_type);
	string file_name = "XX_";
	if(!strcmp(frame_type.c_str(), CCD_FRAME_LIGHT_NAME_N)){
			file_name = "LF_";
	}else if(!strcmp(frame_type.c_str(), CCD_FRAME_DARK_NAME_N)){
			file_name = "DF_";
	}else if(!strcmp(frame_type.c_str(), CCD_FRAME_FLAT_NAME_N)){
			file_name = "FF_";
	}else if(!strcmp(frame_type.c_str(), CCD_FRAME_BIAS_NAME_N)){
			file_name = "BF_";
	}
	file_name += GetStartExposureTimestamp() + ".fits";
  return(file_name);
}
#endif // INDI
//==========================================================================
#ifdef INDI
int SbigCam::WriteFits(	string						fits_name,
												unsigned short 	width, 
												unsigned short 	height, 
												unsigned short 	*buffer)
{
	fitsfile *fptr;       /* pointer to the FITS file; defined in fitsio.h */
	int status=0;
	long  fpixel = 1, naxis = 2, nelements;
  	long naxes[2];
	int res = CE_NO_ERROR;

	naxes[0] = width;
	naxes[1] = height;
	nelements = naxes[0] * naxes[1];          /* number of pixels to write */

	// Insert ! to overwrite if file already exists
	fits_name.insert(0, "!");
	/* create new file */
	if (fits_create_file(&fptr, fits_name.c_str(), &status))
	{
		IDMessage(DEVICE_NAME, "Error: WriteFits - cannot open FITS file for writing.");
		return(CE_OS_ERROR);
	}

	/* Create the primary array image (16-bit short integer pixels */
	if (fits_create_img(fptr, USHORT_IMG, naxis, naxes, &status))
	{
		IDMessage(DEVICE_NAME, "Error: WriteFits - cannot create FITS image.");
		return(CE_OS_ERROR);
	}
	
	CreateFitsHeader(fptr, width, height);

	/* Write the array of integers to the image */
  	if (fits_write_img(fptr, TUSHORT, fpixel, nelements, buffer, &status))
	{
	    	IDMessage(DEVICE_NAME, "Error: WriteFits - write error occurred.");
	    	res = CE_OS_ERROR;
  	}

	fits_close_file(fptr, &status);		/* close the file */
	fits_report_error(stderr, status);	/* print out any error messages */

	return(res);
}
#endif // INDI

//==========================================================================
#ifdef INDI
void	SbigCam::CreateFitsHeader(fitsfile *fptr, unsigned int width, unsigned int height)
{
  int status=0;
  char key[16];
  char comment[32];
  
  double temp_val;

  strncpy(key, "INSTRUME", 16);
  strncpy(comment, "CCD Name", 32);
  fits_update_key(fptr, TSTRING, key, m_icam_product_t[0].text, comment , &status);

  strncpy(key, "DETNAM", 16);
  strncpy(comment, "", 32);
  fits_update_key(fptr, TSTRING, key, m_icam_product_t[1].text, comment, &status);
  temp_val = GetLastExposeTime();

  strncpy(key, "EXPTIME", 16);
  strncpy(comment, "Total Exposure Time (s)", 32);
  fits_update_key(fptr, TDOUBLE, key, &temp_val, comment, &status);
  temp_val = GetLastTemperature();

  strncpy(key, "CCD-TEMP", 16);
  strncpy(comment, "degrees celcius", 32);
  fits_update_key(fptr, TDOUBLE, key , &temp_val, comment, &status);

  strncpy(key, "XPIXSZ", 16);
  strncpy(comment, "um", 32);
  fits_update_key(fptr, TDOUBLE, key, &m_icam_pixel_size_n[0].value, comment, &status);

  strncpy(key, "YPIXSZ", 16);
  strncpy(comment, "um", 32);
  fits_update_key(fptr, TDOUBLE, key, &m_icam_pixel_size_n[0].value, comment, &status);

  // XBINNING & YBINNING:	
  int binning;
  if(GetSelectedCcdBinningMode(binning) == CE_NO_ERROR)
  {
		switch(binning)
   	{
			case 	CCD_BIN_1x1_I:
						binning = 1;
						break;
			case 	CCD_BIN_2x2_I:
 			case 	CCD_BIN_2x2_E:
						binning = 2;
						break;		
			case 	CCD_BIN_3x3_I:
			case 	CCD_BIN_3x3_E:
						binning = 3;
						break;	
			case 	CCD_BIN_9x9_I:
						binning = 9;
						break;
			default:
						binning = 0;
						break;
	}

        strncpy(key, "XBINNING", 16);
        strncpy(comment, "1=1x1, 2=2x2, etc.", 32);
        fits_update_key(fptr, TINT, key , &binning, comment, &status);

        strncpy(key, "YBINNING", 16);
        strncpy(comment, "1=1x1, 2=2x2, etc.", 32);
        fits_update_key(fptr, TINT, key, &binning, comment, &status);
	
  }

	#ifdef USE_CCD_FRAME_STANDARD_PROPERTY		
	// XORGSUBF:
        strncpy(key, "XORGSUBF", 16);
        strncpy(comment, "", 32);
        fits_update_key(fptr, TINT, key, &m_icam_ccd_frame_n[0].value, comment, &status);
	// YORGSUBF:
        strncpy(key, "YORGSUBF", 16);
        strncpy(comment, "", 32);
        fits_update_key(fptr, TINT, key, &m_icam_ccd_frame_n[1].value, comment, &status);
	#else
	// XORGSUBF:
        strncpy(key, "XORGSUBF", 16);
        strncpy(comment, "", 32);
        fits_update_key(fptr, TINT, key, &m_icam_frame_x_n[0].value, comment, &status);
        strncpy(key, "YORGSUBF", 16);
        strncpy(comment, "", 32);
        fits_update_key(fptr, TINT, key, &m_icam_frame_y_n[0].value, comment, &status);
	#endif

	// IMAGETYP:
	string str;
	GetSelectedCcdFrameType(str);
	if(!strcmp(str.c_str(), 		CCD_FRAME_LIGHT_NAME_N)){
			str = "Light Frame";
	}else if(!strcmp(str.c_str(),	CCD_FRAME_DARK_NAME_N)){
			str = "Dark Frame";
	}else if(!strcmp(str.c_str(), CCD_FRAME_FLAT_NAME_N)){
			str = "Flat Field";
	}else if(!strcmp(str.c_str(),	CCD_FRAME_BIAS_NAME_N)){
			str = "Bias Frame";
	}else{
			str = "Unknown";
	}
	char frame[64];
	strncpy(frame, str.c_str(), 64);

        strncpy(key, "IMAGETYP", 16);
        strncpy(comment, "Frame Type", 32);
        fits_update_key(fptr, TSTRING, key, frame, comment, &status);
}

#endif // INDI
//==========================================================================
#ifdef INDI
int SbigCam::UploadFits(string fits_name)
{
	struct stat	stat_p;
	if(stat(fits_name.c_str(), &stat_p) < 0){ 
		IDMessage(	DEVICE_NAME, "Error: UploadFits - stat %s.", 
							fits_name.c_str()); 
		return(CE_OS_ERROR); 
	}

	unsigned long total_bytes = stat_p.st_size;
	unsigned char *fits_data	 = 	(unsigned char *) malloc 
								(sizeof(unsigned char) * total_bytes);
	if(fits_data == 0){
		IDMessage(	DEVICE_NAME, "Error: UploadFits - low memory. "
							"Unable to initialize FITS buffers.");
		return(CE_OS_ERROR);
	}

	#ifdef USE_BLOB_COMPRESS	
	unsigned char *compressed_data = (unsigned char *) malloc(sizeof(unsigned char) * 
									total_bytes + total_bytes / 64 + 16 + 3);
	if(compressed_data == 0){
		IDMessage(	DEVICE_NAME, "Error: UploadFits - low memory. "
							"Unable to initialize FITS buffers.");
		free (fits_data);
		return(CE_OS_ERROR);
	}	
	#endif   

	FILE *fits_file = fopen(fits_name.c_str(), "r");
	if(fits_file == 0) {
		free (fits_data);
		return(CE_OS_ERROR);
	}
   
	// Read FITS file from disk:
	unsigned int	i = 0, nr = 0;
	for (i=0; i < total_bytes; i+= nr){
				nr = fread(fits_data + i, 1, total_bytes - i, fits_file);
				if(nr <= 0){
						IDMessage(DEVICE_NAME, "Error: UploadFits - reading temporary FITS file.");
						free(compressed_data);
						return(CE_OS_ERROR);
				}
	}
	fclose(fits_file);

 	#ifdef USE_BLOB_COMPRESS  
	unsigned long compressed_bytes = sizeof(char) * total_bytes + 
									total_bytes / 64 + 16 + 3;
	// Compress it: 
	int r = compress2(compressed_data, &compressed_bytes, fits_data, total_bytes, 9);
	if(r != Z_OK){
		// This should NEVER happen. 
		IDMessage(DEVICE_NAME, "Error: UploadFits - compression failed: %d", r);
		return(CE_OS_ERROR);
	}
	#endif

	// Send BLOB:  	
	strcpy(m_icam_fits_b.format, BLOB_FORMAT_B);
	#ifdef USE_BLOB_COMPRESS  
	m_icam_fits_b.blob 		= compressed_data;
	m_icam_fits_b.bloblen 	= compressed_bytes;
	#else
	m_icam_fits_b.blob 		= fits_data;
	m_icam_fits_b.bloblen 	= total_bytes;
	#endif
	m_icam_fits_b.size 		= total_bytes;
	m_icam_fits_bp.s 			= IPS_OK;

	//IDMessage(DEVICE_NAME, "--> Format	: %s", 	m_icam_fits_b.format);
	//IDMessage(DEVICE_NAME, "--> Bytes		: %u", 	m_icam_fits_b.bloblen);
	//IDMessage(DEVICE_NAME, "--> nbp			: %d", 	m_icam_fits_bp.nbp);
	//IDMessage(DEVICE_NAME, "--> Size    : %u", 	m_icam_fits_b.size);
	//IDMessage(DEVICE_NAME, "==> IDSetBLOB start...");

	IDSetBLOB(&m_icam_fits_bp, 0);

	//IDMessage(DEVICE_NAME, "==> IDSetBLOB stop...");

	// Remove FITS file from the server site:
	remove(fits_name.c_str());

	// Release memory:
	free(fits_data);   
	#ifdef USE_BLOB_COMPRESS  
	free(compressed_data);
	#endif
	return(CE_NO_ERROR);
}
#endif // INDI
//==========================================================================
#ifdef INDI
void SbigCam::SetBlobState(IPState state)
{	
 	strcpy(m_icam_fits_b.format,	BLOB_FORMAT_B);
  m_icam_fits_b.blob    	= 0;
  m_icam_fits_b.bloblen	= 0;
  m_icam_fits_b.size   	= 0;
  m_icam_fits_bp.s		 		= state;
  m_icam_fits_bp.bp			= &m_icam_fits_b;
	IDSetBLOB(&m_icam_fits_bp, 0);	
}
#endif // INDI
//==========================================================================
#ifdef INDI
int SbigCam::CfwConnect()
{		
	int				res;
	CFWResults	cfwr;

	ISwitch *p = IUFindOnSwitch(&m_icfw_type_sp);
	if(!p) return(CE_OS_ERROR);

	do{
		// 1. CFWC_OPEN_DEVICE:
		if((res = CfwOpenDevice(&cfwr)) != CE_NO_ERROR){
			m_icfw_connection_sp.s = IPS_IDLE;
			IDMessage(DEVICE_NAME, "CFWC_OPEN_DEVICE error: %s !", 
					  		GetErrorString(res).c_str());
			continue;
		}

		// 2. CFWC_INIT:
		if((res = CfwInit(&cfwr)) != CE_NO_ERROR){
			IDMessage(	DEVICE_NAME, "CFWC_INIT error: %s !", 
					 			GetErrorString(res).c_str());
			CfwCloseDevice(&cfwr);
			IDMessage(DEVICE_NAME, "CFWC_CLOSE_DEVICE called."); 
			continue;
		}

		// 3. CFWC_GET_INFO:
		if((res = CfwGetInfo(&cfwr)) != CE_NO_ERROR){
			IDMessage(DEVICE_NAME, "CFWC_GET_INFO error!"); 
			continue;
		}

		// 4. CfwUpdateProperties:
		CfwUpdateProperties(cfwr);

		// 5. Set CFW's filter min/max values:
		m_icfw_slot_n[0].min = 1;
 		m_icfw_slot_n[0].max = cfwr.cfwResult2;
		IUUpdateMinMax(&m_icfw_slot_np); 

	}while(0);
	return(res);
}
#endif // INDI
//==========================================================================
#ifdef INDI
int SbigCam::CfwDisconnect()
{		
	CFWResults	cfwr;

	ISwitch *p = IUFindOnSwitch(&m_icfw_type_sp);
	if(!p) return(CE_OS_ERROR);

	// Close CFW device:				
	return(CfwCloseDevice(&cfwr));
}
#endif // INDI
//==========================================================================
#ifdef INDI
int SbigCam::CfwOpenDevice(CFWResults *cfwr)
{	
	// Under Linux we always try to open the "sbigcfw" device. There has to be a
  // symbolic link (ln -s) between the actual device and this name.
	
	CFWParams 	cfwp;
	int 				res = CE_NO_ERROR;
	int				cfwModel = GetCfwSelType();

	switch(cfwModel){
		case 	CFWSEL_CFW10_SERIAL:
					cfwp.cfwModel 		= cfwModel;
					cfwp.cfwCommand 	= CFWC_OPEN_DEVICE;
					res = SBIGUnivDrvCommand(CC_CFW, &cfwp, cfwr);
					break;
		default:
					break;
	}
	return(res);
}
#endif // INDI
//==========================================================================
#ifdef INDI
int SbigCam::CfwCloseDevice(CFWResults *cfwr)
{	
	CFWParams cfwp;

	cfwp.cfwModel 		= GetCfwSelType();
	cfwp.cfwCommand 	= CFWC_CLOSE_DEVICE;
	return(SBIGUnivDrvCommand(CC_CFW, &cfwp, cfwr));
}
#endif // INDI
//==========================================================================
#ifdef INDI
int SbigCam::CfwInit(CFWResults *cfwr)
{				
	// Try to init CFW maximum three times:
	int res;
	CFWParams cfwp;

	cfwp.cfwModel 		= GetCfwSelType();
	cfwp.cfwCommand 	= CFWC_INIT;	
	for(int i=0; i < 3; i++){
			if((res = SBIGUnivDrvCommand(CC_CFW, &cfwp, cfwr)) == CE_NO_ERROR) break;
			sleep(1);
	}

	if(res != CE_NO_ERROR) return(res);
	return(CfwGotoMonitor(cfwr));
}	
#endif // INDI
//==========================================================================
#ifdef INDI
int SbigCam::CfwGetInfo(CFWResults *cfwr)
{	
	CFWParams cfwp;

	cfwp.cfwModel 		= GetCfwSelType();
	cfwp.cfwCommand 	= CFWC_GET_INFO;	
	cfwp.cfwParam1		= CFWG_FIRMWARE_VERSION;		
	return(SBIGUnivDrvCommand(CC_CFW, &cfwp, cfwr));
}
#endif // INDI
//==========================================================================
#ifdef INDI
int SbigCam::CfwQuery(CFWResults *cfwr)
{	
	CFWParams cfwp;

	cfwp.cfwModel 		= GetCfwSelType();
	cfwp.cfwCommand 	= CFWC_QUERY;
	return(SBIGUnivDrvCommand(CC_CFW, &cfwp, cfwr));
}
#endif // INDI
//==========================================================================
#ifdef INDI
int SbigCam::CfwGoto(CFWResults *cfwr)
{	
	int res;
	CFWParams cfwp;

	cfwp.cfwModel 		= GetCfwSelType();
	cfwp.cfwCommand 	= CFWC_GOTO;	
	cfwp.cfwParam1	= (unsigned long)m_icfw_slot_n[0].value;
	if((res = SBIGUnivDrvCommand(CC_CFW, &cfwp, cfwr)) != CE_NO_ERROR) return(res);
	return(CfwGotoMonitor(cfwr));	
}
#endif // INDI
//==========================================================================
#ifdef INDI
int SbigCam::CfwGotoMonitor(CFWResults *cfwr)
{	
	int res;
	do{
			if((res = CfwQuery(cfwr)) != CE_NO_ERROR) return(res);
	}while(cfwr->cfwStatus != CFWS_IDLE);
	return(res);
}
#endif // INDI		
//==========================================================================
#ifdef INDI
void SbigCam::CfwUpdateProperties(CFWResults cfwr)
{
	char 	str[64];
	bool	bClear = false;

	switch(cfwr.cfwModel){
		case	CFWSEL_CFW2:
					sprintf(str, "%s", "CFW - 2");
					break;
		case 	CFWSEL_CFW5:
					sprintf(str, "%s", "CFW - 5");
					break;
		case 	CFWSEL_CFW6A:
					sprintf(str, "%s", "CFW - 6A");			
					break;
		case 	CFWSEL_CFW8:
					sprintf(str, "%s", "CFW - 8");
					break;
		case 	CFWSEL_CFW402:
					sprintf(str, "%s", "CFW - 402");
					break;
		case 	CFWSEL_CFW10:
					sprintf(str, "%s", "CFW - 10");
					break;
		case 	CFWSEL_CFW10_SERIAL:
					sprintf(str, "%s", "CFW - 10SA");	
					break;
		case 	CFWSEL_CFWL:
					sprintf(str, "%s", "CFW - L");			
					break;
		case	CFWSEL_CFW9:
					sprintf(str, "%s", "CFW - 9");
					break ;
		default:
					sprintf(str, "%s", "Unknown");
					bClear = true;	
					break;
	}			
	// Set CFW's product ID:
	IText *pIText = IUFindText(&m_icfw_product_tp, PRODUCT_NAME_T); 
	if(pIText) IUSaveText(pIText, str);

	// Set CFW's firmware version:
	if(bClear){
			sprintf(str, "%s", "Unknown");
	}else{
			sprintf(str, "%d", (int)cfwr.cfwResult1);
	}
	pIText = IUFindText(&m_icfw_product_tp, PRODUCT_ID_NAME_T); 
	if(pIText) IUSaveText(pIText, str);
	m_icfw_product_tp.s = IPS_OK;
	IDSetText(&m_icfw_product_tp, 0);

	// Set CFW's filter min/max values:
	if(!bClear){
			m_icfw_slot_n[0].min = 1;
 			m_icfw_slot_n[0].max = cfwr.cfwResult2;
			IUUpdateMinMax(&m_icfw_slot_np); 
	}
}
#endif // INDI		
//==========================================================================
#ifdef INDI
int SbigCam::GetCfwSelType()
{		
	int 	type = CFWSEL_UNKNOWN;;
	ISwitch *p = IUFindOnSwitch(&m_icfw_type_sp);

	if(p){
		if(!strcmp(p->name, CFW1_NAME_S)){
 				type = CFWSEL_CFW2;
		}else if(!strcmp(p->name, CFW2_NAME_S)){
 				type = CFWSEL_CFW5;
		}else if(!strcmp(p->name, CFW3_NAME_S)){
				type = CFWSEL_CFW6A;
		}else if(!strcmp(p->name, CFW4_NAME_S)){
 				type = CFWSEL_CFW8;
		}else if(!strcmp(p->name, CFW5_NAME_S)){
				type = CFWSEL_CFW402;
		}else if(!strcmp(p->name, CFW6_NAME_S)){
 				type = CFWSEL_CFW10;
		}else if(!strcmp(p->name, CFW7_NAME_S)){
				type = CFWSEL_CFW10_SERIAL;
		}else if(!strcmp(p->name, CFW8_NAME_S)){
				type = CFWSEL_CFWL;
		}else if(!strcmp(p->name, CFW9_NAME_S)){
				type = CFWSEL_CFW9;
		#ifdef	 USE_CFW_AUTO
		}else if(!strcmp(p->name, CFW10_NAME_S)){
				type = CFWSEL_AUTO;
		#endif
		}
	}
	return(type);
}
#endif // INDI
//==========================================================================
#ifdef INDI
void SbigCam::CfwShowResults(string name, CFWResults cfwr)
{
	IDMessage(DEVICE_NAME, "%s", name.c_str()); 
	IDMessage(DEVICE_NAME, "CFW Model:	%d", cfwr.cfwModel);
	IDMessage(DEVICE_NAME, "CFW Position:	%d", cfwr.cfwPosition);
	IDMessage(DEVICE_NAME, "CFW Status:	%d", cfwr.cfwStatus);
	IDMessage(DEVICE_NAME, "CFW Error:	%d", cfwr.cfwError);
	IDMessage(DEVICE_NAME, "CFW Result1:	%ld", cfwr.cfwResult1);
	IDMessage(DEVICE_NAME, "CFW Result2:	%ld", cfwr.cfwResult2);
}
#endif // INDI
//==========================================================================
