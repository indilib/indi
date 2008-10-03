#if !defined(AFX_APOGEE__INCLUDED_)
#define AFX_APOGEE__INCLUDED_

#define Apn_Platform int
#define Apn_Platform_Unknown 0
#define Apn_Platform_Alta    1
#define Apn_Platform_Ascent  2

#define	Apn_Interface int
#define	Apn_Interface_NET 0
#define	Apn_Interface_USB 1

#define	Apn_NetworkMode int
#define	Apn_NetworkMode_Tcp	0
#define	Apn_NetworkMode_Udp	1

#define	Apn_Resolution int
#define	Apn_Resolution_SixteenBit 0
#define	Apn_Resolution_TwelveBit 1

#define	Apn_CameraMode int
#define	Apn_CameraMode_Normal 0
#define	Apn_CameraMode_TDI 1
#define	Apn_CameraMode_Test 2
#define	Apn_CameraMode_ExternalTrigger 3
#define	Apn_CameraMode_ExternalShutter 4
#define	Apn_CameraMode_Kinetics 5

#define	Apn_Status int
#define	Apn_Status_DataError -2
#define	Apn_Status_PatternError	 -1
#define	Apn_Status_Idle	 0
#define	Apn_Status_Exposing  1
#define	Apn_Status_ImagingActive  2
#define	Apn_Status_ImageReady  3
#define	Apn_Status_Flushing  4
#define	Apn_Status_WaitingOnTrigger 5
#define	Apn_Status_ConnectionError 6

#define	Apn_LedMode int
#define	Apn_LedMode_DisableAll 0
#define	Apn_LedMode_DisableWhileExpose 1
#define	Apn_LedMode_EnableAll 2

#define	Apn_LedState int
#define	Apn_LedState_Expose 0
#define	Apn_LedState_ImageActive 1
#define	Apn_LedState_Flushing 2
#define	Apn_LedState_ExtTriggerWaiting 3
#define	Apn_LedState_ExtTriggerReceived 4
#define	Apn_LedState_ExtShutterInput 5
#define	Apn_LedState_ExtStartReadout 6
#define	Apn_LedState_AtTemp 7

#define	Apn_CoolerStatus int
#define	Apn_CoolerStatus_Off 0
#define	Apn_CoolerStatus_RampingToSetPoint 1
#define	Apn_CoolerStatus_AtSetPoint 2
#define	Apn_CoolerStatus_Revision 3

#define	Apn_FanMode int
#define	Apn_FanMode_Off	0
#define	Apn_FanMode_Low 1
#define	Apn_FanMode_Medium 2
#define	Apn_FanMode_High 3

#define ApnUsbParity int
#define ApnNetParity int
#define Apn_SerialParity int
#define Apn_SerialFlowControl bool
#define ApnUsbParity_None 0
#define ApnUsbParity_Odd 1
#define ApnUsbParity_Even 2
#define ApnNetParity_None 0
#define ApnNetParity_Odd 1
#define ApnNetParity_Even 2
#define Apn_SerialFlowControl_Unknown 0
#define Apn_SerialFlowControl_Off 0
#define Apn_SerialFlowControl_On 1
#define Apn_SerialParity_Unknown 0
#define Apn_SerialParity_None 0
#define Apn_SerialParity_Odd 1
#define Apn_SerialParity_Even 2

#define Apn_Filter int
#define Apn_Filter_Unknown  0
#define Apn_Filter_FW50_9R  1
#define Apn_Filter_FW50_7S  2
#define Apn_Filter_AFW25_4R 3
#define Apn_Filter_AFW30_7R 4
#define Apn_Filter_AFW50_5R 5

#define Apn_FilterStatus int
#define Apn_FilterStatus_NotConnected 0
#define Apn_FilterStatus_Ready 1
#define Apn_FilterStatus_Active 2

#define Apn_BayerShift int
#define Apn_BayerShift_None 0
#define Apn_BayerShift_Column 1
#define Apn_BayerShift_Row 2
#define Apn_BayerShift_Both 3
#define Apn_BayerShift_Automatic 4

#define	Camera_Status int
#define	Camera_Status_Idle 0
#define	Camera_Status_Waiting 1
#define	Camera_Status_Exposing 2
#define	Camera_Status_Downloading 3
#define	Camera_Status_LineReady 4
#define	Camera_Status_ImageReady 5
#define	Camera_Status_Flushing 6

#define	Camera_CoolerStatus int
#define	Camera_CoolerStatus_Off	 0
#define	Camera_CoolerStatus_RampingToSetPoint 1
#define	Camera_CoolerStatus_Correcting 2
#define	Camera_CoolerStatus_RampingToAmbient 3
#define	Camera_CoolerStatus_AtAmbient 4
#define	Camera_CoolerStatus_AtMax 5
#define	Camera_CoolerStatus_AtMin 6 
#define	Camera_CoolerStatus_AtSetPoint 7

#define	Camera_CoolerMode int
#define	Camera_CoolerMode_Off 0
#define	Camera_CoolerMode_On  1
#define	Camera_CoolerMode_Shutdown 2

#endif



