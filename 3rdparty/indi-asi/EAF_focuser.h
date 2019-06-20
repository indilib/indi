/**************************************************
this is the ZWO focuser EAF SDK
any question feel free contact us:yang.zhou@zwoptical.com

***************************************************/
#ifndef EAF_FOCUSER_H
#define EAF_FOCUSER_H

#ifdef _WINDOWS
#define EAF_API __declspec(dllexport)
#else
#define EAF_API 
#endif

#define EAF_ID_MAX 128

typedef struct _EAF_INFO
{
	int ID;
	char Name[64];
	int MaxStep;//fixed maximum position
} EAF_INFO;


typedef enum _EAF_ERROR_CODE{
	EAF_SUCCESS = 0,
	EAF_ERROR_INVALID_INDEX,
	EAF_ERROR_INVALID_ID,
	EAF_ERROR_INVALID_VALUE,
	EAF_ERROR_REMOVED, //failed to find the focuser, maybe the focuser has been removed
	EAF_ERROR_MOVING,//focuser is moving
	EAF_ERROR_ERROR_STATE,//focuser is in error state
	EAF_ERROR_GENERAL_ERROR,//other error
	EAF_ERROR_NOT_SUPPORTED,
	EAF_ERROR_CLOSED,
	EAF_ERROR_END = -1
}EAF_ERROR_CODE;

#ifdef __cplusplus
extern "C" {
#endif
/***************************************************************************
Descriptions:
This should be the first API to be called
get number of connected EAF focuser, call this API to refresh device list if EAF is connected
or disconnected

Return: number of connected EAF focuser. 1 means 1 focuser is connected.
***************************************************************************/
EAF_API int EAFGetNum();

/***************************************************************************
Descriptions:
Get the product ID of each wheel, at first set pPIDs as 0 and get length and then malloc a buffer to load the PIDs

Paras:
int* pPIDs: pointer to array of PIDs

Return: length of the array.
***************************************************************************/
EAF_API int EAFGetProductIDs(int* pPIDs);

/***************************************************************************
Descriptions:
Get ID of focuser

Paras:
int index: the index of focuser, from 0 to N - 1, N is returned by GetNum()

int* ID: pointer to ID. the ID is a unique integer, between 0 to EAF_ID_MAX - 1, after opened,
all the operation is base on this ID, the ID will not change.


Return: 
EAF_ERROR_INVALID_INDEX: index value is invalid
EAF_SUCCESS:  operation succeeds

***************************************************************************/
EAF_API EAF_ERROR_CODE EAFGetID(int index, int* ID);

/***************************************************************************
Descriptions:
Open focuser

Paras:
int ID: the ID of focuser

Return: 
EAF_ERROR_INVALID_ID: invalid ID value
EAF_ERROR_GENERAL_ERROR: number of opened focuser reaches the maximum value.
EAF_ERROR_REMOVED: the focuser is removed.
EAF_SUCCESS: operation succeeds
***************************************************************************/
EAF_API	EAF_ERROR_CODE EAFOpen(int ID);

/***************************************************************************
Descriptions:
Get property of focuser.

Paras:
int ID: the ID of focuser

EAF_INFO *pInfo:  pointer to structure containing the property of EAF

Return: 
EAF_ERROR_INVALID_ID: invalid ID value
EAF_ERROR_MOVING: focuser is moving.
EAF_SUCCESS: operation succeeds
EAF_ERROR_REMOVED: focuser is removed

***************************************************************************/

EAF_API	EAF_ERROR_CODE EAFGetProperty(int ID, EAF_INFO *pInfo); 



/***************************************************************************
Descriptions:
Move focuser to an absolute position.

Paras:
int ID: the ID of focuser

int iStep: step value is between 0 to EAF_INFO::MaxStep

Return: 
EAF_ERROR_INVALID_ID: invalid ID value
EAF_ERROR_CLOSED: not opened
EAF_SUCCESS: operation succeeds
EAF_ERROR_ERROR_STATE: focuser is in error state
EAF_ERROR_REMOVED: focuser is removed

***************************************************************************/
EAF_API	EAF_ERROR_CODE EAFMove(int ID, int iStep);


/***************************************************************************
Descriptions:
Stop moving.

Paras:
int ID: the ID of focuser

Return: 
EAF_ERROR_INVALID_ID: invalid ID value
EAF_ERROR_CLOSED: not opened
EAF_SUCCESS: operation succeeds
EAF_ERROR_ERROR_STATE: focuser is in error state
EAF_ERROR_REMOVED: focuser is removed

***************************************************************************/
EAF_API	EAF_ERROR_CODE EAFStop(int ID); 


/***************************************************************************
Descriptions:
Check if the focuser is moving.

Paras:
int ID: the ID of focuser
bool *pbVal: pointer to the value, imply if focuser is moving
bool* pbHandControl: pointer to the value, imply focuser is moved by handle control, can't be stopped by calling EAFStop()
Return: 
EAF_ERROR_INVALID_ID: invalid ID value
EAF_ERROR_CLOSED: not opened
EAF_SUCCESS: operation succeeds
EAF_ERROR_ERROR_STATE: focuser is in error state
EAF_ERROR_REMOVED: focuser is removed

***************************************************************************/
EAF_API	EAF_ERROR_CODE EAFIsMoving(int ID, bool *pbVal, bool* pbHandControl = 0); 


/***************************************************************************
Descriptions:
Get current position.

Paras:
int ID: the ID of focuser
bool *piStep: pointer to the value

Return: 
EAF_ERROR_INVALID_ID: invalid ID value
EAF_ERROR_CLOSED: not opened
EAF_SUCCESS: operation succeeds
EAF_ERROR_ERROR_STATE: focuser is in error state
EAF_ERROR_REMOVED: focuser is removed

***************************************************************************/
EAF_API	EAF_ERROR_CODE EAFGetPosition(int ID, int* piStep);

/***************************************************************************
Descriptions:
Set as current position

Paras:
int ID: the ID of focuser
int iStep: step value

Return: 
EAF_ERROR_INVALID_ID: invalid ID value
EAF_ERROR_CLOSED: not opened
EAF_SUCCESS: operation succeeds
EAF_ERROR_ERROR_STATE: focuser is in error state
EAF_ERROR_REMOVED: focuser is removed

***************************************************************************/
EAF_API	EAF_ERROR_CODE EAFResetPostion(int ID, int iStep);

/***************************************************************************
Descriptions:
Get the value of the temperature detector, if it's moved by handle, the temperature value is unreasonable, the value is -273 and return error

Paras:
int ID: the ID of focuser
bool *pfTemp: pointer to the value

Return: 
EAF_ERROR_INVALID_ID: invalid ID value
EAF_ERROR_CLOSED: not opened
EAF_SUCCESS: operation succeeds
EAF_ERROR_ERROR_STATE: focuser is in error state
EAF_ERROR_REMOVED: focuser is removed
EAF_ERROR_GENERAL_ERROR: temperature value is unusable
***************************************************************************/
EAF_API	EAF_ERROR_CODE EAFGetTemp(int ID, float* pfTemp);

/***************************************************************************
Descriptions:
Turn on/off beep, if true the focuser will beep at the moment when it begins to move 

Paras:
int ID: the ID of focuser
bool bVal: turn on beep if true

Return: 
EAF_ERROR_INVALID_ID: invalid ID value
EAF_ERROR_CLOSED: not opened
EAF_SUCCESS: operation succeeds
EAF_ERROR_ERROR_STATE: focuser is in error state
EAF_ERROR_REMOVED: focuser is removed

***************************************************************************/
EAF_API	EAF_ERROR_CODE EAFSetBeep(int ID, bool bVal);

/***************************************************************************
Descriptions:
Get if beep is turned on 

Paras:
int ID: the ID of focuser
bool *pbVal: pointer to the value

Return: 
EAF_ERROR_INVALID_ID: invalid ID value
EAF_ERROR_CLOSED: not opened
EAF_SUCCESS: operation succeeds
EAF_ERROR_ERROR_STATE: focuser is in error state
EAF_ERROR_REMOVED: focuser is removed

***************************************************************************/
EAF_API	EAF_ERROR_CODE EAFGetBeep(int ID, bool* pbVal);

/***************************************************************************
Descriptions:
Set the maximum position

Paras:
int ID: the ID of focuser
int iVal: maximum position

Return: 
EAF_ERROR_INVALID_ID: invalid ID value
EAF_ERROR_CLOSED: not opened
EAF_SUCCESS: operation succeeds
EAF_ERROR_MOVING: focuser is moving, should wait until idle
EAF_ERROR_ERROR_STATE: focuser is in error state
EAF_ERROR_REMOVED: focuser is removed
***************************************************************************/
EAF_API	EAF_ERROR_CODE EAFSetMaxStep(int ID, int iVal);

/***************************************************************************
Descriptions:
Get the maximum position

Paras:
int ID: the ID of focuser
bool *piVal: pointer to the value

Return: 
EAF_ERROR_INVALID_ID: invalid ID value
EAF_ERROR_CLOSED: not opened
EAF_SUCCESS: operation succeeds
EAF_ERROR_MOVING: focuser is moving, should wait until idle
EAF_ERROR_ERROR_STATE: focuser is in error state
EAF_ERROR_REMOVED: focuser is removed
***************************************************************************/
EAF_API	EAF_ERROR_CODE EAFGetMaxStep(int ID, int* piVal);

/***************************************************************************
Descriptions:
Set moving direction of focuser

Paras:
int ID: the ID of focuser

bool bVal: if set as true, the focuser will move along reverse direction

Return: 
EAF_ERROR_INVALID_ID: invalid ID value
EAF_ERROR_CLOSED: not opened
EAF_SUCCESS: operation succeeds
***************************************************************************/
EAF_API	EAF_ERROR_CODE EAFSetReverse(int ID, bool bVal);

/***************************************************************************
Descriptions:
Get moving direction of focuser

Paras:
int ID: the ID of focuser

bool *pbVal: pointer to direction value.

Return: 
EAF_ERROR_INVALID_ID: invalid ID value
EAF_ERROR_CLOSED: not opened
EAF_SUCCESS: operation succeeds
***************************************************************************/
EAF_API	EAF_ERROR_CODE EAFGetReverse(int ID, bool* pbVal);

/***************************************************************************
Descriptions:
Set backlash of focuser

Paras:
int ID: the ID of focuser

int iVal: backlash value.

Return: 
EAF_ERROR_INVALID_ID: invalid ID value
EAF_ERROR_CLOSED: not opened
EAF_SUCCESS: operation succeeds
***************************************************************************/
EAF_API	EAF_ERROR_CODE EAFSetBacklash(int ID, int iVal);

/***************************************************************************
Descriptions:
Get backlash of focuser

Paras:
int ID: the ID of focuser

int *piVal: pointer to backlash value.

Return: 
EAF_ERROR_INVALID_ID: invalid ID value
EAF_ERROR_CLOSED: not opened
EAF_SUCCESS: operation succeeds
***************************************************************************/
EAF_API	EAF_ERROR_CODE EAFGetBacklash(int ID, int* piVal);

/***************************************************************************
Descriptions:
Close focuser

Paras:
int ID: the ID of focuser

Return: 
EAF_ERROR_INVALID_ID: invalid ID value
EAF_SUCCESS: operation succeeds
***************************************************************************/
EAF_API	EAF_ERROR_CODE EAFClose(int ID);

/***************************************************************************
Descriptions:
Get firmware version of focuser

Paras:
int ID: the ID of focuser

int *major, int *minor, int *build: pointer to value.

Return: 
EAF_ERROR_INVALID_ID: invalid ID value
EAF_ERROR_CLOSED: not opened
EAF_SUCCESS: operation succeeds
***************************************************************************/
EAF_API	EAF_ERROR_CODE EAFGetFirmwareVersion(int ID, unsigned char *major, unsigned char *minor, unsigned char *build);

#ifdef __cplusplus
}
#endif

#endif