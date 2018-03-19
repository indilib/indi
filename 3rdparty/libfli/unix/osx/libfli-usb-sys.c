/*
 
 Copyright (c) 2002 Finger Lakes Instrumentation (FLI), L.L.C.
 All rights reserved.
 
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:
 
 Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 
 Redistributions in binary form must reproduce the above
 copyright notice, this list of conditions and the following
 disclaimer in the documentation and/or other materials
 provided with the distribution.
 
 Neither the name of Finger Lakes Instrumentation (FLI), LLC
 nor the names of its contributors may be used to endorse or
 promote products derived from this software without specific
 prior written permission.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
 
 ======================================================================
 
 Finger Lakes Instrumentation, L.L.C. (FLI)
 web: http://www.fli-cam.com
 email: support@fli-cam.com
 
 */

#include "libfli-usb-sys.h"

//==========================================================================
// MAC_FLI_LIST
// Function to enumerate connected devices
//
//==========================================================================
long mac_fli_list(flidomain_t domain, char ***names)
{
	char **list = NULL;
    if((list = malloc((MAX_SEARCH + 1) * sizeof(char *))) == NULL)
    {
        return -ENOMEM;
    }
    
    int id = 0;
    
	kern_return_t kret;
	IOReturn ioret;
    
    io_name_t deviceName;
    
	UInt16 usbProduct, usbVendor;
    
    UInt32 locationID;
    
	SInt32 score;
	io_service_t usbDevice;
	
	io_iterator_t dev_iterator;
    
	IOUSBDeviceInterface182 **dev_int;
	IOCFPlugInInterface **plugInInterface;
    
	CFMutableDictionaryRef matchingDict = IOServiceMatching(kIOUSBDeviceClassName);
	if(!matchingDict)
    {
		debug(FLIDEBUG_FAIL, "mac_fli_list: could not get matching dictionary");
        return -1;
	}
    
	IONotificationPortRef gNotifyPort = IONotificationPortCreate(kIOMasterPortDefault);
    CFRunLoopSourceRef runLoopSource = IONotificationPortGetRunLoopSource(gNotifyPort);
    CFRunLoopRef gRunLoop = CFRunLoopGetCurrent();
    CFRunLoopAddSource(gRunLoop, runLoopSource, kCFRunLoopDefaultMode);
    
	kret = IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDict, &dev_iterator);
	if(kret)
    {
		debug(FLIDEBUG_FAIL, "mac_fli_list: could not get Matching Services");
        return -1;
	}
    
	while((usbDevice = IOIteratorNext(dev_iterator)))
    {        
		ioret = IOCreatePlugInInterfaceForService(usbDevice, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &plugInInterface, &score);
		if(ioret)
        {
			debug(FLIDEBUG_FAIL, "mac_fli_list: could not get Plug In Interface");
            return -1;
		}
		
        kret = IORegistryEntryGetName(usbDevice, deviceName);
        if(kret)
        {
            debug(FLIDEBUG_FAIL, "mac_fli_list: could not get Registry Entry Name");
            return -1;
        }
        
        kret = IOObjectRelease(usbDevice);
		if(kret)
        {
			debug(FLIDEBUG_FAIL, "mac_fli_list: could not release device");
            return -1;
		}
        
		ioret = (*plugInInterface)->QueryInterface(plugInInterface, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID182), (LPVOID*)&dev_int);
		if(ioret)
        {
			debug(FLIDEBUG_FAIL, "mac_fli_list: could not Query Plug In Interface");
            return -1;
		}
		
		(*plugInInterface)->Release(plugInInterface);
        
		ioret = (*dev_int)->GetDeviceVendor(dev_int, &usbVendor);
		if(ioret)
        {
			debug(FLIDEBUG_FAIL, "mac_fli_list: could not get Device Vendor");
            return -1;
		}
        
		// Check if its an FLI device
		if(usbVendor == FLI_VENDOR_ID)
        {
            ioret = (*dev_int)->GetDeviceProduct(dev_int, &usbProduct);
            if(ioret)
            {
                debug(FLIDEBUG_FAIL, "mac_fli_list: could not get Device Product");
                return -1;
            }
            
            switch(usbProduct)
            {
                    FLIUSB_PRODUCTS
                    break;
                default:
                    continue;
            }
            
            ioret = (*dev_int)->GetLocationID(dev_int, &locationID);
            if(ioret)
            {
                debug(FLIDEBUG_FAIL, "mac_fli_list: could not get location id");
                return -1;
            }
            
            asprintf(&list[id], "%08x;%s", (unsigned int)locationID, deviceName);
            
            id++;
        }
        
	}
    
    IOObjectRelease(dev_iterator);
    
	if(id == 0)
	{
		*names = NULL;
		free(list);
		return 0;
	}
    
	list[id] = NULL;
	*names = list;
    
	return 0;
}

//-------------------------------------------------------------------------


//==========================================================================
// MAC_FLI_CONNECT
// Function to enumerate and establish connection to device
//
//==========================================================================
long mac_fli_connect(flidev_t dev, char *name, long domain)
{
    debug(FLIDEBUG_INFO, "mac_fli_connect");
    
    CHKDEVICE(dev);
    
    fli_unixio_t *io;
    
    if(name == NULL)
    {
        return -EINVAL;
    }
    
    /* Lock functions should be set before any other functions used */
    DEVICE->fli_lock = mac_fli_lock;
    DEVICE->fli_unlock = mac_fli_unlock;
    
    DEVICE->domain = domain & 0x00ff;
    DEVICE->devinfo.type = domain & 0xff00;
    
    if((io = calloc(1, sizeof(fli_unixio_t))) == NULL)
    {
        return -ENOMEM;
    }
    
    if(!(io->fd = open(name, O_RDWR)))
    {
        return -ENOMEM;
    }
    
    int r;
    
    // Note: unix_usb_connect is defined to mac_usb_connect in OSX
    
    if((r = mac_usb_connect(dev, io, name)) != 0)
    {
        close(io->fd);
        free(io);
        return r;
    }
    
    DEVICE->fli_io = unix_usbio;

    switch(DEVICE->devinfo.type)
    {
        case FLIDEVICE_CAMERA: //0x100
            DEVICE->fli_open = fli_camera_open;
            DEVICE->fli_close = fli_camera_close;
            DEVICE->fli_command = fli_camera_command;
            break;
            
        case FLIDEVICE_FOCUSER: //0x300
            DEVICE->fli_open = fli_focuser_open;
            DEVICE->fli_close = fli_focuser_close;
            DEVICE->fli_command = fli_focuser_command;
            break;
            
        case FLIDEVICE_FILTERWHEEL: //0x200
            DEVICE->fli_open = fli_filter_open;
            DEVICE->fli_close = fli_filter_close;
            DEVICE->fli_command = fli_filter_command;
            break;
            
        default:
            mac_usb_disconnect(dev,io);
            close(io->fd);
            free(io);
            return -EINVAL;
    }
    
    DEVICE->io_data = io;
    
    //char *nameCopy = NULL;
    //asprintf(&nameCopy, "%s", name);
    
    // xstrdup(name);
    DEVICE->name = strdup(name); //nameCopy;
    
    DEVICE->io_timeout = 3000; // 60 * 1000; /* 1 min. */
    
    debug(FLIDEBUG_INFO, "mac_fli_connect: connected");
    
    return 0;
}
//-------------------------------------------------------------------------


//==========================================================================
// MAC_FLI_DISCONNECT
// Disconnect device
//
//==========================================================================
long mac_fli_disconnect(flidev_t dev, fli_unixio_t *io)
{
    int err = 0;
    
    CHKDEVICE(dev);
    
    if((io = DEVICE->io_data) == NULL)
    {
        debug(FLIDEBUG_FAIL, "mac_fli_disconnect: io data error");
        return -EINVAL;
    }
    
    switch(DEVICE->domain)
    {
        case FLIDOMAIN_USB:
            err = mac_usb_disconnect(dev,io);
            break;
            
        default:
            break;
    }
    
    if(close(io->fd))
    {
        if(!err)
        {
            err = -errno;
        }
    }
    
    free(DEVICE->io_data);
    free(DEVICE->sys_data);
    
    DEVICE->io_data = NULL;
    DEVICE->sys_data = NULL;
    
    DEVICE->fli_lock = NULL;
    DEVICE->fli_unlock = NULL;
    DEVICE->fli_io = NULL;
    DEVICE->fli_open = NULL;
    DEVICE->fli_close = NULL;
    DEVICE->fli_command = NULL;
    
    free(DEVICE->name);
    DEVICE->name = NULL;
    
    debug(FLIDEBUG_INFO, "mac_fli_disconnect: disconnected");
    
    return err;
}
//-------------------------------------------------------------------------


//==========================================================================
// MAC_USB_CONNECT
// connect device
//
//==========================================================================
long mac_usb_connect(flidev_t dev, fli_unixio_t *io, char *name)
{
    bool found = false;
    
	kern_return_t kret;
	IOReturn ioret;
    
    io_name_t deviceName;
    
	UInt16 usbRelease, usbProduct, usbVendor;
    UInt32 locationID;
    
	SInt32 score;
	io_service_t usbDevice;
	
	io_iterator_t dev_iterator;
    
	IOUSBDeviceInterface182 **dev_int;
	IOCFPlugInInterface **plugInInterface;
    
	CFMutableDictionaryRef matchingDict = IOServiceMatching(kIOUSBDeviceClassName);
	if(!matchingDict)
    {
		debug(FLIDEBUG_FAIL, "mac_usb_connect: could not get matching dictionary");
        return -ENODEV;
	}
    
	IONotificationPortRef gNotifyPort = IONotificationPortCreate(kIOMasterPortDefault);
    CFRunLoopSourceRef runLoopSource = IONotificationPortGetRunLoopSource(gNotifyPort);
    CFRunLoopRef gRunLoop = CFRunLoopGetCurrent();
    CFRunLoopAddSource(gRunLoop, runLoopSource, kCFRunLoopDefaultMode);
    
	kret = IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDict, &dev_iterator);
	if(kret)
    {
		debug(FLIDEBUG_FAIL, "mac_usb_connect: could not get Matching Services");
        return -ENODEV;
	}
    
	while((usbDevice = IOIteratorNext(dev_iterator)))
    {        
		ioret = IOCreatePlugInInterfaceForService(usbDevice, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &plugInInterface, &score);
		if(ioret)
        {
			debug(FLIDEBUG_FAIL, "mac_usb_connect: could not get Plug In Interface");
            return -ENODEV;
		}
		
        kret = IORegistryEntryGetName(usbDevice, deviceName);
        if(kret)
        {
            debug(FLIDEBUG_FAIL, "mac_usb_connect: could not get Registry Entry Name");
            return -ENODEV;
        }
        
        kret = IOObjectRelease(usbDevice);
		if(kret)
        {
			debug(FLIDEBUG_FAIL, "mac_usb_connect: could not release device");
            return -ENODEV;
		}
        
		ioret = (*plugInInterface)->QueryInterface(plugInInterface, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID182), (LPVOID*)&dev_int);
		if(ioret)
        {
			debug(FLIDEBUG_FAIL, "mac_usb_connect: could not Query Plug In Interface");
            return -ENODEV;
		}
		
		(*plugInInterface)->Release(plugInInterface);
        
		ioret = (*dev_int)->GetDeviceVendor(dev_int, &usbVendor);
		if(ioret)
        {
			debug(FLIDEBUG_FAIL, "mac_usb_connect: could not get Device Vendor");
            return -ENODEV;
		}
        
		// Check if its an FLI device
		if(usbVendor == FLI_VENDOR_ID)
        {
            ioret = (*dev_int)->GetDeviceProduct(dev_int, &usbProduct);
            if(ioret)
            {
                debug(FLIDEBUG_FAIL, "mac_usb_connect: could not get Device Product");
                return -ENODEV;
            }
            
            switch(usbProduct)
            {
                    FLIUSB_PRODUCTS
                    break;
                default:
                    continue;
            }
            
            ioret = (*dev_int)->GetDeviceReleaseNumber(dev_int, &usbRelease);
            if(ioret)
            {
                debug(FLIDEBUG_FAIL, "mac_usb_connect: could not get Device Release Number");
                return -ENODEV;
            }
            
            ioret = (*dev_int)->GetLocationID(dev_int, &locationID);
            if(ioret)
            {
                debug(FLIDEBUG_FAIL, "mac_usb_connect: could not get location id");
                return -ENODEV;
            }
            
            char *deviceLocIDName = NULL;
            
            asprintf(&deviceLocIDName, "%08x", (unsigned int)locationID);
            
            if(strcmp(name, deviceLocIDName) == 0)
            {
                found = true;
                
                debug(FLIDEBUG_INFO, "mac_usb_connect: connecting to usb device");
                      
                //asprintf(&DEVICE->devinfo.model, "%s", deviceName);
                //asprintf(&DEVICE->devinfo.devnam, "%08x", locationID);
                
                DEVICE->devinfo.devid = usbProduct;
                DEVICE->devinfo.fwrev = usbRelease;
                
                ioret = (*dev_int)->USBDeviceOpenSeize(dev_int);
                if(ioret)
                {
                    debug(FLIDEBUG_FAIL, "mac_usb_connect: USBDeviceOpenSeize failed");
                    return -ENODEV;
                }
                
                // Reset the usb device so you will be able to fully configure the device
                ioret = (*dev_int)->ResetDevice(dev_int);
                if(ioret)
                {
                    debug(FLIDEBUG_FAIL, "mac_usb_connect: ResetDevice failed");
                    return -ENODEV;
                }
                
                if(mac_usb_find_interfaces(dev, dev_int) != kIOReturnSuccess)
                {
                    debug(FLIDEBUG_FAIL, "mac_usb_connect: mac_usb_find_interfaces failed");
                    (*dev_int)->USBDeviceClose(dev_int);
                    (*dev_int)->Release(dev_int);
                    return -ENODEV;
                }
                
                debug(FLIDEBUG_INFO, "mac_usb_connect: connected to usb device!");
                
                break;
            }
            
        }
    }
    
    IOObjectRelease(dev_iterator);
    
    if(!found)
    {
        return -ENODEV;
    }
    
    return 0;
}

IOReturn mac_usb_configure_device(IOUSBDeviceInterface182 **dev)
{
    //UInt8 numConfig; // configNum
    IOReturn kr;
    IOUSBConfigurationDescriptorPtr configDesc;
    
    /*
    kr = (*dev)->GetConfiguration(dev, &configNum);
    if(kr)
    {
        debug(FLIDEBUG_FAIL, "mac_usb_configure_device: could not get configuration");
        return -1;
    }
    */
    // Device is already configured (Was already plugged in and connected to in the past, reconfiguring will break it)
    /*if(configNum == 0)
    {
        debug(FLIDEBUG_INFO, "mac_usb_configure_device: already configured %d", configNum);
        return kIOReturnSuccess;
    }*/
    
    /*
    kr = (*dev)->GetNumberOfConfigurations(dev, &numConfig);
    if(!numConfig)
    {
        debug(FLIDEBUG_FAIL, "mac_usb_configure_device: could not get number of configurations");
        return -1;
    }
    */
    
    kr = (*dev)->GetConfigurationDescriptorPtr(dev, 0, &configDesc);
    if(kr)
    {
        debug(FLIDEBUG_FAIL, "mac_usb_configure_device: could not get configuration descriptor");
        return -1;
    }
    
    kr = (*dev)->SetConfiguration(dev, configDesc->bConfigurationValue);
    if(kr)
    {
        debug(FLIDEBUG_FAIL, "mac_usb_configure_device: could not set configuration");
        return -1;
    }
    /*
    UInt8 configNum2;
    kr = (*dev)->GetConfiguration(dev, &configNum2);
    if(kr)
    {
        debug(FLIDEBUG_FAIL, "mac_usb_configure_device: could not get configuration");
        return -1;
    }*/
    
    debug(FLIDEBUG_INFO, "mac_usb_configure_device: Configured to %d", configDesc->bConfigurationValue);
    //debug(FLIDEBUG_INFO, "mac_usb_configure_device: %d %d %d %d", configNum, configNum2, numConfig, configDesc->bConfigurationValue);
    
    return kIOReturnSuccess;
}

IOReturn mac_usb_find_interfaces(flidev_t dev, IOUSBDeviceInterface182 **device)
{
    IOReturn kr;
    IOUSBFindInterfaceRequest request;
    io_iterator_t iterator;
    io_service_t usbInterface;
    IOCFPlugInInterface **plugInInterface = NULL;
    IOUSBInterfaceInterface190 **interface = NULL;
    HRESULT result;
    SInt32 score;
    UInt8 interfaceClass;
    UInt8 interfaceSubClass;
    UInt8 interfaceNumEndpoints;
    
    request.bInterfaceClass = kIOUSBFindInterfaceDontCare;
    request.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
    request.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
    request.bAlternateSetting = kIOUSBFindInterfaceDontCare;
    
    kr = (*device)->CreateInterfaceIterator(device, &request, &iterator);
    
    while((usbInterface = IOIteratorNext(iterator)))
    {
        kr = IOCreatePlugInInterfaceForService(usbInterface, kIOUSBInterfaceUserClientTypeID, kIOCFPlugInInterfaceID, &plugInInterface, &score);
        
        kr = IOObjectRelease(usbInterface);
        if((kr != kIOReturnSuccess) || !plugInInterface)
        {
            debug(FLIDEBUG_FAIL, "mac_usb_find_interfaces: Unable to create a plugin");
            return kr;
        }
        
        result = (*plugInInterface)->QueryInterface(plugInInterface, CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID190), (LPVOID *) &interface);
        (*plugInInterface)->Release(plugInInterface);
        
        if(result || !interface)
        {
            debug(FLIDEBUG_FAIL, "mac_usb_find_interfaces: Couldn’t create a device interface");
            return kr;
        }
        
        kr = (*interface)->GetInterfaceClass(interface, &interfaceClass);
        kr = (*interface)->GetInterfaceSubClass(interface, &interfaceSubClass);
        
        kr = (*interface)->USBInterfaceOpen(interface);
        if(kr != kIOReturnSuccess)
        {
            debug(FLIDEBUG_FAIL, "mac_usb_find_interfaces: Unable to open interface (%08x)", kr);
            (*interface)->Release(interface);
            return kr;
        }
        
        kr = (*interface)->GetNumEndpoints(interface, &interfaceNumEndpoints);
        if(kr != kIOReturnSuccess)
        {
            debug(FLIDEBUG_FAIL, "mac_usb_find_interfaces:  Unable to get number of endpoints (%08x)", kr);
            (*interface)->USBInterfaceClose(interface);
            (*interface)->Release(interface);
            return kr;
        }
        
        mac_device_info *deviceInfo = (mac_device_info*) malloc(sizeof(mac_device_info));
        DEVICE->sys_data = deviceInfo;
        
        DEVICE_DATA->interface = interface;
        DEVICE_DATA->device = device;
        DEVICE_DATA->interfaceNumEndpoints = interfaceNumEndpoints;
        
        if(DEVICE->devinfo.devid == FLIUSB_PROLINECAM)
        {
            DEVICE_DATA->epRead = 2;
            DEVICE_DATA->epWrite = 1;
            DEVICE_DATA->epReadBulk = 3;
        }
        else if(DEVICE->devinfo.devid == FLIUSB_MAXCAM)
        {
            DEVICE_DATA->epRead = 2;
            DEVICE_DATA->epWrite = 3;
            DEVICE_DATA->epReadBulk = 2;
        }
        else if(DEVICE->devinfo.devid == FLIUSB_FILTERWHEEL || FLIUSB_FOCUSER || FLIUSB_STEPPER)
        {
            DEVICE_DATA->epRead = 1;
            DEVICE_DATA->epWrite = 2;
            DEVICE_DATA->epReadBulk = 1;
        }
        
// Debug pipes
/*
        int pipeRef;
        for (pipeRef = 1; pipeRef <= interfaceNumEndpoints; pipeRef++)
        {
            IOReturn        kr2;
            UInt8           direction;
            UInt8           number;
            UInt8           transferType;
            UInt16          maxPacketSize;
            UInt8           interval;
            char            *message;
            
            kr2 = (*interface)->GetPipeProperties(interface,
                                                  pipeRef, &direction,
                                                  &number, &transferType,
                                                  &maxPacketSize, &interval);
            if (kr2 != kIOReturnSuccess)
                printf("Unable to get properties of pipe %d (%08x)\n",
                       pipeRef, kr2);
            else
            {
                printf("PipeRef %d: ", pipeRef);
                switch (direction)
                {
                    case kUSBOut:
                        message = "out";
                        break;
                    case kUSBIn:
                        message = "in";
                        break;
                    case kUSBNone:
                        message = "none";
                        break;
                    case kUSBAnyDirn:
                        message = "any";
                        break;
                    default:
                        message = "???";
                }
                printf("direction %s, ", message);
                
                switch (transferType)
                {
                    case kUSBControl:
                        message = "control";
                        break;
                    case kUSBIsoc:
                        message = "isoc";
                        break;
                    case kUSBBulk:
                        message = "bulk";
                        break;
                    case kUSBInterrupt:
                        message = "interrupt";
                        break;
                    case kUSBAnyType:
                        message = "any";
                        break;
                    default:
                        message = "???";
                }
                printf("transfer type %s, maxPacketSize %d\n", message,
                       maxPacketSize);
            }
        }
*/   
        // We only need 1 interface for FLI devices
        return kIOReturnSuccess;
    }
    
    debug(FLIDEBUG_WARN, "mac_usb_find_interfaces: Found 0 interfaces, setting configuration");
    
    if(mac_usb_configure_device(device) == kIOReturnSuccess)
    {
        return mac_usb_find_interfaces(dev, device);
    }
    
    return -1;
}


//==========================================================================
// MAC_USB_DISCONNECT
// Delete USB interface Data
//
//==========================================================================
long mac_usb_disconnect(flidev_t dev, fli_unixio_t *io)
{
    debug(FLIDEBUG_INFO, "mac_usb_disconnect");
          
    (*DEVICE_DATA->interface)->USBInterfaceClose(DEVICE_DATA->interface);
    (*DEVICE_DATA->interface)->Release(DEVICE_DATA->interface);
    
    (*DEVICE_DATA->device)->USBDeviceClose(DEVICE_DATA->device);
    (*DEVICE_DATA->device)->Release(DEVICE_DATA->device);
    
    return 0;
}
//-------------------------------------------------------------------------


//==========================================================================
// MAC_BULKTRANSFER
// IO control call to start piperead/pipewrite
//
//==========================================================================
long mac_bulktransfer(flidev_t dev, int ep, void *buf, long *tlen)
{
    if(ep & USB_DIR_IN)
    {
        mac_usb_piperead(dev, buf, *tlen, DEVICE_DATA->epReadBulk, DEVICE->io_timeout);
    }
    else
    {
        mac_usb_pipewrite(dev, buf, *tlen, DEVICE_DATA->epWrite, DEVICE->io_timeout);
    }
    return 0;
}


//==========================================================================
// MAC_BULKREAD
// IO control call to read
//
//==========================================================================
long mac_bulkread(flidev_t dev, void *buf, long *rlen)
{
    mac_usb_piperead(dev, buf, *rlen, DEVICE_DATA->epRead, DEVICE->io_timeout);
    return 0;
}
//-------------------------------------------------------------------------


//==========================================================================
// MAC_BULKWRITE
// IO control call to write
//
//==========================================================================
long mac_bulkwrite(flidev_t dev, void *buf, long *wlen)
{
    mac_usb_pipewrite(dev, buf, *wlen, DEVICE_DATA->epWrite, DEVICE->io_timeout);
    return 0;
}

//==========================================================================
// PIPEREAD
// Function to Read input calls ReadPipeTO
//
//
//==========================================================================
ssize_t mac_usb_piperead(flidev_t dev, void *buf, size_t size, unsigned pipe, unsigned timeout)
{
    CHKDEVICE(dev);
    
	IOReturn ioret;
    
	if(pipe <= 0 || pipe > DEVICE_DATA->interfaceNumEndpoints)
    {
		debug(FLIDEBUG_FAIL, "mac_usb_piperead: invalid pipe number (%u of %u)", pipe, DEVICE_DATA->interfaceNumEndpoints);
		return -EINVAL;
	}
    
	ioret = (*DEVICE_DATA->interface)->ReadPipeTO(DEVICE_DATA->interface, pipe, buf, (UInt32*) &size, 0, timeout);
	if(ioret)
    {
		debug(FLIDEBUG_FAIL, "mac_usb_piperead: read error: %x, size: %d", ioret, (int)size);
              
		if(ioret != kIOReturnSuccess)
        {
            debug(FLIDEBUG_FAIL, "mac_usb_piperead: aborted");
			ioret = (*DEVICE_DATA->interface)->ClearPipeStallBothEnds(DEVICE_DATA->interface, pipe);
			//ioret = (*DEVICE_DATA->interface)->ClearPipeStall(DEVICE_DATA->interface, true);
            if(ioret)
            {
                debug(FLIDEBUG_FAIL, "mac_usb_piperead: Pipe Stalled: %x", ioret);
            }
		}
        
		return -EINVAL;
	}
    
	return size;
}
//-------------------------------------------------------------------------


//==========================================================================
// PIPEWRITE
// Function to write data to device calls WritePipeTo
//
//==========================================================================
ssize_t mac_usb_pipewrite(flidev_t dev, void *buf, size_t size, unsigned pipe, unsigned timeout)
{
    CHKDEVICE(dev);
    
	IOReturn ioret;    
    
	int piperef = DEVICE_DATA->epWrite;
    
	if(piperef <= 0 || piperef > DEVICE_DATA->interfaceNumEndpoints)
    {
		debug(FLIDEBUG_FAIL, "mac_usb_pipewrite: invalid pipe number (%u of %u)", piperef, DEVICE_DATA->interfaceNumEndpoints);
		return -EINVAL;
	}
    
	ioret = (*DEVICE_DATA->interface)->WritePipeTO(DEVICE_DATA->interface, piperef, buf, size, timeout, 0); 
	if(ioret)
    {
		debug(FLIDEBUG_FAIL, "mac_usb_pipewrite: write error %x", ioret);
		return -EINVAL;
	}
    
	return size;
}
//-------------------------------------------------------------------------


//==========================================================================
// MAC_FLI_LOCK
// Function to lock access to device
//
//==========================================================================
long mac_fli_lock(flidev_t dev)
{
    
    fli_unixio_t *io = DEVICE->io_data;
    
    if(io == NULL)
    {
        return -ENODEV;
    }
    
    if(!flock(io->fd, LOCK_EX))
    {
        return -errno;
    }
    
    return 0;
}
//-------------------------------------------------------------------------


//==========================================================================
// MAC_FLI_UNLOCK
// Function to unlock access to device
//
//==========================================================================
long mac_fli_unlock(flidev_t dev)
{
    fli_unixio_t *io = DEVICE->io_data;
    if(io == NULL)
    {
        return -ENODEV;
    }
    
    if(!flock(io->fd, LOCK_UN))
    {
        return -errno;
    }
    
    return 0;
}
//-------------------------------------------------------------------------
