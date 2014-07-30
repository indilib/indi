/*******************************************************************************
  Copyright(c) 2014. Jasem Mutlaq.

  SBIG CFW

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#ifndef SBIGCFW_H
#define SBIGCFW_H

#include <indifilterwheel.h>
#include <iostream>
#include "sbigudrv.h"

using namespace std;

#define MAX_CFW_TYPES   9

//=============================================================================
const int		INVALID_HANDLE_VALUE		= -1;	// for file operations
//=============================================================================
// SBIG CCD camera port definitions:
#define			SBIG_USB0 					"sbigusb0"
#define			SBIG_USB1 					"sbigusb1"
#define			SBIG_USB2 					"sbigusb2"
#define			SBIG_USB3 					"sbigusb3"
#define			SBIG_LPT0 					"sbiglpt0"
#define			SBIG_LPT1 					"sbiglpt1"
#define			SBIG_LPT2 					"sbiglpt2"

class SBIGCFW : public INDI::FilterWheel
{
    public:
        SBIGCFW();
        SBIGCFW(const char* devName);
        virtual ~SBIGCFW();

        const char *getDefaultName();

        bool Connect();
        bool Disconnect();

        virtual bool SelectFilter(int);
        virtual int QueryFilter();
        virtual bool SetFilterNames();
        virtual bool GetFilterNames(const char* groupName);
        virtual bool saveConfigItems(FILE *fp);

        bool initProperties();
        void ISGetProperties(const char *dev);
        bool updateProperties();

        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);
        virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);

    protected:

        inline int		GetFileDescriptor(){return(m_fd);}
        inline void		SetFileDescriptor(int val = -1){m_fd = val;}
        inline bool		IsDeviceOpen(){return((m_fd == -1) ? false : true);}
        inline int 		GetDriverHandle(){return(m_drv_handle);}
        inline void		SetDriverHandle(int val = INVALID_HANDLE_VALUE){m_drv_handle = val;}
        inline bool		GetLinkStatus(){return(m_link_status);}
        inline void		SetLinkStatus(bool val = false){m_link_status = val;}
        inline char		*GetDeviceName(){return(m_dev_name);}
        int				SetDeviceName(const char*);
        string			GetErrorString(int err);
        int				QueryCommandStatus(	QueryCommandStatusParams *,
                                            QueryCommandStatusResults *);
        int				MiscellaneousControl(MiscellaneousControlParams *);
        int				GetLinkStatus(GetLinkStatusResults *);
        int				SetDriverControl(SetDriverControlParams *);
        int				GetDriverControl(GetDriverControlParams *,
                                    GetDriverControlResults *);
        int				UsbAdControl(USBADControlParams *);
        int				QueryUsb(QueryUSBResults *);
        int				RwUsbI2c(RWUSBI2CParams *);
        int				BitIo(BitIOParams *, BitIOResults *);
        bool			CheckLink();

        // SBIG's software interface to the Universal Driver Library function:
        int				SBIGUnivDrvCommand(PAR_COMMAND, void *, void *);

        // Driver Related Commands:
        int				EstablishLink();
        int 			OpenDevice(const char *name);
        int				CloseDevice();
        int				GetDriverInfo(GetDriverInfoParams *, void *);
        int				SetDriverHandle(SetDriverHandleParams *);
        int				GetDriverHandle(GetDriverHandleResults *);
        void            InitVars();
        int     		OpenDriver();
        int             CloseDriver();


        int 			CFW(CFWParams *, CFWResults *);
        int				CFWConnect();
        int				CFWDisconnect();
        int 			CFWOpenDevice(CFWResults *);
        int		 		CFWCloseDevice(CFWResults *);
        int		 		CFWInit(CFWResults *);
        int		 		CFWGetInfo(CFWResults *);
        int		 		CFWQuery(CFWResults*);
        int				CFWGoto(CFWResults *, int position);
        int				CFWGotoMonitor(CFWResults *);
        void 			CFWShowResults(string name, CFWResults);
        void			CFWUpdateProperties(CFWResults);
        int				GetCFWSelType();

        // CFW GROUP:
        IText					FilterProdcutT[2];
        ITextVectorProperty     FilterProdcutTP;

        ISwitch                 FilterTypeS[MAX_CFW_TYPES];
        ISwitchVectorProperty	FilterTypeSP;

        IText                   PortT[1];
        ITextVectorProperty     PortTP;

        char name[MAXINDINAME];
        int						m_fd;
        int						m_drv_handle;
        bool					m_link_status;
        char					m_dev_name[MAXRBUF];

        friend void ::ISGetProperties(const char *dev);
        friend void ::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num);
        friend void ::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num);
        friend void ::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num);
        friend void ::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n);

};

#endif // SBIGCFW_H
