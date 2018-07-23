/**************************************************
this is the ZWO USB2ST4 converter SDK
any question feel free contact us:yang.zhou@zwoptical.com

here is the suggested procedure.

--> USB2ST4GetNum
--> USB2ST4GetID for each converter

--> USB2ST4Open
--> USB2ST4PulseGuide

	...
--> USB2ST4Close

***************************************************/
#ifndef USB2ST4_FILTER_H
#define USB2ST4_FILTER_H


#ifdef _WINDOWS
#define USB2ST4_API __declspec(dllexport)
#else
#define USB2ST4_API 
#endif

#define USB2ST4_ID_MAX 128

typedef enum USB2ST4_DIRECTION{ //Direction
	USB2ST4_NORTH=0,
	USB2ST4_SOUTH,
	USB2ST4_EAST,
	USB2ST4_WEST
}USB2ST4_DIRECTION;

typedef enum _USB2ST4_ERROR_CODE{
	USB2ST4_SUCCESS = 0,
	USB2ST4_ERROR_INVALID_INDEX,
	USB2ST4_ERROR_INVALID_ID,
	USB2ST4_ERROR_INVALID_VALUE,
	USB2ST4_ERROR_REMOVED, //failed to find the converter, maybe the converter has been removed
	USB2ST4_ERROR_ERROR_STATE,//converter is in error state
	USB2ST4_ERROR_GENERAL_ERROR,//other error
	USB2ST4_ERROR_CLOSED,
	USB2ST4_ERROR_END = -1
}USB2ST4_ERROR_CODE;

#ifdef __cplusplus
extern "C" {
#endif
/***************************************************************************
Descriptions:
this should be the first API to be called
get number of connected USB2ST4 converter, call this API to refresh device list if USB2ST4 is connected
or disconnected

Return: number of connected USB2ST4 converter. 1 means 1 converter is connected.
***************************************************************************/
USB2ST4_API int USB2ST4GetNum();

/***************************************************************************
Descriptions:
get the product ID of each wheel, at first set pPIDs as 0 and get length and then malloc a buffer to load the PIDs

Paras:
int* pPIDs: pointer to array of PIDs

Return: length of the array.
***************************************************************************/
USB2ST4_API int USB2ST4GetProductIDs(int* pPIDs);

/***************************************************************************
Descriptions:
get ID of converter

Paras:
int index: the index of converter, from 0 to N - 1, N is returned by GetNum()

int* ID: pointer to ID. the ID is a unique integer, between 0 to USB2ST4_ID_MAX - 1, after opened,
all the operation is base on this ID, the ID will not change.


Return: 
USB2ST4_ERROR_INVALID_INDEX: index value is invalid
USB2ST4_SUCCESS:  operation succeeds

***************************************************************************/
USB2ST4_API USB2ST4_ERROR_CODE USB2ST4GetID(int index, int* ID);

/***************************************************************************
Descriptions:
determine if the converter is opened

Paras:
int ID: the ID of converter

Return: 
USB2ST4_ERROR_INVALID_ID: invalid ID value
USB2ST4_ERROR_CLOSED: not opened
USB2ST4_SUCCESS: operation succeeds
USB2ST4_ERROR_INVALID_VALUE: Position value is invalid
USB2ST4_ERROR_ERROR_STATE: converter is in error state
USB2ST4_ERROR_REMOVED: converter is removed

***************************************************************************/
USB2ST4_API USB2ST4_ERROR_CODE USB2ST4IsOpened(int ID);

/***************************************************************************
Descriptions:
open converter

Paras:
int ID: the ID of converter

Return: 
USB2ST4_ERROR_INVALID_ID: invalid ID value
USB2ST4_ERROR_GENERAL_ERROR: number of opened converter reaches the maximum value.
USB2ST4_ERROR_REMOVED: the converter is removed.
USB2ST4_SUCCESS: operation succeeds
***************************************************************************/
USB2ST4_API	USB2ST4_ERROR_CODE USB2ST4Open(int ID);
	
/***************************************************************************
Descriptions:
pulse guide in the given direction 

Paras:
int ID: the ID of converter

bool bSet: true is on, false is off

Return: 
USB2ST4_ERROR_INVALID_ID: invalid ID value
USB2ST4_ERROR_CLOSED: not opened
USB2ST4_SUCCESS: operation succeeds
USB2ST4_ERROR_INVALID_VALUE: Position value is invalid
USB2ST4_ERROR_ERROR_STATE: converter is in error state
USB2ST4_ERROR_REMOVED: converter is removed

***************************************************************************/
USB2ST4_API	USB2ST4_ERROR_CODE USB2ST4PulseGuide(int ID, USB2ST4_DIRECTION Dirction, bool bSet);


/***************************************************************************
Descriptions:
close converter

Paras:
int ID: the ID of converter

Return: 
USB2ST4_ERROR_INVALID_ID: invalid ID value
USB2ST4_SUCCESS: operation succeeds
***************************************************************************/
USB2ST4_API	USB2ST4_ERROR_CODE USB2ST4Close(int ID);


#ifdef __cplusplus
}
#endif

#endif