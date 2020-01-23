/*******************************************************************************
  Copyright(c) 2017 Jasem Mutlaq. All rights reserved.

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

#include "dspinterface.h"

#include "defaultdevice.h"
#include "indiccd.h"
#include "indisensorinterface.h"
#include "indilogger.h"
#include "locale_compat.h"
#include "indicom.h"

#include <fitsio.h>

#include <libnova/julian_day.h>
#include <libnova/ln_types.h>
#include <libnova/precession.h>

#include <regex>

#include <dirent.h>
#include <cerrno>
#include <locale.h>
#include <cstdlib>
#include <zlib.h>
#include <sys/stat.h>

// Create dir recursively
static int _det_mkdir(const char *dir, mode_t mode)
{
    char tmp[PATH_MAX];
    char *p = nullptr;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", dir);
    len = strlen(tmp);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = 0;
    for (p = tmp + 1; *p; p++)
        if (*p == '/')
        {
            *p = 0;
            if (mkdir(tmp, mode) == -1 && errno != EEXIST)
                return -1;
            *p = '/';
        }
    if (mkdir(tmp, mode) == -1 && errno != EEXIST)
        return -1;

    return 0;
}

static std::string regex_replace_compat(const std::string &input, const std::string &pattern, const std::string &replace)
{
    std::stringstream s;
    std::regex_replace(std::ostreambuf_iterator<char>(s), input.begin(), input.end(), std::regex(pattern), replace);
    return s.str();
}

namespace DSP
{
const char *DSP_TAB = "Signal Processing";

Interface::Interface(INDI::DefaultDevice *dev, Type type, const char *name, const char *label) : m_Device(dev), m_Name(name), m_Label(label), m_Type(type)
{
    strncpy (processedFileExtension, "fits", MAXINDIFORMAT);
    IUFillSwitch(&ActivateS[0], "DSP_ACTIVATE_ON", "Activate", ISState::ISS_OFF);
    IUFillSwitch(&ActivateS[1], "DSP_ACTIVATE_OFF", "Deactivate", ISState::ISS_ON);
    IUFillSwitchVector(&ActivateSP, ActivateS, 2, dev->getDeviceName(), "DSP_ACTIVATE", "Activate", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    IUFillBLOB(&FitsB, "DATA", "DSP Data Blob", "");
    IUFillBLOBVector(&FitsBP, &FitsB, 1, getDeviceName(), "DSP", "Processed Data", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    // Snoop properties of interest
    IDSnoopDevice(ActiveDeviceT[0].text, "EQUATORIAL_EOD_COORD");
    IDSnoopDevice(ActiveDeviceT[1].text, "TELESCOPE_INFO");
    IDSnoopDevice(ActiveDeviceT[2].text, "FILTER_SLOT");
    IDSnoopDevice(ActiveDeviceT[2].text, "FILTER_NAME");
    IDSnoopDevice(ActiveDeviceT[3].text, "SKY_QUALITY");
}

Interface::~Interface()
{
}

const char *Interface::getDeviceName()
{
    return getDeviceName();
}

void Interface::ISGetProperties(const char *dev)
{
    if(!strcmp(dev, getDeviceName())) {
        m_Device->defineSwitch(&ActivateSP);
        m_Device->defineBLOB(&FitsBP);
    }
}

bool Interface::updateProperties()
{
    if (m_Device->isConnected()) {
        m_Device->defineSwitch(&ActivateSP);
        m_Device->defineBLOB(&FitsBP);
    } else {
        m_Device->deleteProperty(ActivateSP.name);
        m_Device->deleteProperty(FitsBP.name);
    }
    return true;
}

bool Interface::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(!strcmp(dev, getDeviceName())) {
        for (int i = 0; i < n; i++) {
            if(!strcmp(name, ActivateSP.name)) {
                const char *name = names[i];

                if (!strcmp(name, "DSP_ACTIVATE_ON") && states[i] == ISS_ON) {
                        m_Device->defineBLOB(&FitsBP);
                        Activated();
                }
                if (!strcmp(name, "DSP_ACTIVATE_OFF") && states[i] == ISS_ON) {
                    m_Device->deleteProperty(FitsBP.name);
                    Deactivated();
                }
            }
        }
    }
    return false;
}

bool Interface::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(values);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
    return false;
}

bool Interface::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(texts);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
    return false;
}

bool Interface::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
    return false;
}

bool Interface::ISSnoopDevice(XMLEle *root)
{
    XMLEle *ep           = nullptr;
    const char *propName = findXMLAttValu(root, "name");

    if (!strcmp(propName, "EQUATORIAL_EOD_COORD"))
    {
        const char *name = findXMLAttValu(ep, "name");

        if (!strcmp(name, "RA"))
        {
            primaryAperture = atof(pcdataXMLEle(ep));
        }
        else if (!strcmp(name, "DEC"))
        {
            primaryAperture = atof(pcdataXMLEle(ep));
        }
    }
    else if (!strcmp(propName, "TELESCOPE_INFO"))
    {
        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
        {
            const char *name = findXMLAttValu(ep, "name");

            if (!strcmp(name, "TELESCOPE_APERTURE"))
            {
                primaryAperture = atof(pcdataXMLEle(ep));
            }
            else if (!strcmp(name, "TELESCOPE_FOCAL_LENGTH"))
            {
                primaryFocalLength = atof(pcdataXMLEle(ep));
            }
        }
    }
    else if (!strcmp(propName, "SKY_QUALITY"))
    {
        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
        {
            const char *name = findXMLAttValu(ep, "name");

            if (!strcmp(name, "SKY_BRIGHTNESS"))
            {
                MPSAS = atof(pcdataXMLEle(ep));
                break;
            }
        }
    }

    return true;
}

uint8_t* Interface::Callback(unsigned char* buf, int ndims, int* dims, int bits_per_sample)
{
    INDI_UNUSED(buf);
    INDI_UNUSED(ndims);
    INDI_UNUSED(dims);
    INDI_UNUSED(bits_per_sample);
    return nullptr;
}

bool Interface::processBLOB(unsigned char* buf, int ndims, int* dims, int bits_per_sample)
{
    if(ActivateSP.sp[0].s == ISState::ISS_ON) {
        // Run async
        std::thread(&Interface::processBLOBPrivate, this, buf, ndims, dims, bits_per_sample).detach();
    }
    return true;
}

bool Interface::processBLOBPrivate(unsigned char* buf, int ndims, int* dims, int bits_per_sample)
{
    uint8_t* buffer = Callback(buf, ndims, dims, bits_per_sample);
    bool sendCapture = (m_Device->getSwitch("UPLOAD_MODE")->sp[0].s == ISS_ON || m_Device->getSwitch("UPLOAD_MODE")->sp[2].s == ISS_ON);
    bool saveCapture = (m_Device->getSwitch("UPLOAD_MODE")->sp[1].s == ISS_ON || m_Device->getSwitch("UPLOAD_MODE")->sp[2].s == ISS_ON);

    if (sendCapture || saveCapture)
    {
        void* blob = nullptr;
        int len = 1;
        for (int d = 0; d < BufferSizesQty; d++)
            len *= BufferSizes[d];
        if (!strcmp(FitsB.format, "fits"))
        {
            blob = sendFITS(buffer, len * 8 / abs(BPS), BPS);
        }
        else
        {
            uploadFile(buffer, static_cast<size_t>(len), sendCapture, saveCapture, processedFileExtension);
        }

        if (sendCapture)
            IDSetBLOB(&FitsBP, nullptr);
        if(blob != nullptr)
            free(blob);
    }

    return true;
}

void Interface::Activated()
{
}

void Interface::Deactivated()
{
}

bool Interface::saveConfigItems(FILE *fp)
{
    INDI_UNUSED(fp);
    return true;
}

void Interface::addFITSKeywords(fitsfile *fptr, uint8_t* buf, int len)
{
    INDI_UNUSED(buf);
    INDI_UNUSED(len);
    int status = 0;
    char exp_start[32];

    char *orig = setlocale(LC_NUMERIC, "C");

    char fitsString[MAXINDIDEVICE];

    // Telescope
    strncpy(fitsString, m_Device->getText("ACTIVE_DEVICES")->tp[0].text, MAXINDIDEVICE);
    fits_update_key_s(fptr, TSTRING, "TELESCOP", fitsString, "Telescope name", &status);

    // Observer
    strncpy(fitsString, m_Device->getText("FITS_HEADER")->tp[0].text, MAXINDIDEVICE);
    fits_update_key_s(fptr, TSTRING, "OBSERVER", fitsString, "Observer name", &status);

    // Object
    strncpy(fitsString, m_Device->getText("FITS_HEADER")->tp[1].text, MAXINDIDEVICE);
    fits_update_key_s(fptr, TSTRING, "OBJECT", fitsString, "Object name", &status);

#ifdef WITH_MINMAX
    if (getNAxis() == 2)
    {
        double min_val, max_val;
        getMinMax(&min_val, &max_val, buf, len, getBPS());

        fits_update_key_s(fptr, TDOUBLE, "DATAMIN", &min_val, "Minimum value", &status);
        fits_update_key_s(fptr, TDOUBLE, "DATAMAX", &max_val, "Maximum value", &status);
    }
#endif

    fits_update_key_s(fptr, TDOUBLE, "FOCALLEN", &primaryFocalLength, "Focal Length (mm)", &status);
    fits_update_key_s(fptr, TDOUBLE, "MPSAS", &MPSAS, "Sky Quality (mag per arcsec^2)", &status);

    INumberVectorProperty *nv = m_Device->getNumber("GEOGRAPHIC_COORDS");
    if(nv != nullptr)
    {
        double Lat = nv->np[0].value;
        double Lon = nv->np[1].value;
        double El = nv->np[2].value;

        char lat_str[MAXINDIFORMAT];
        char lon_str[MAXINDIFORMAT];
        char el_str[MAXINDIFORMAT];
        fs_sexa(lat_str, Lat, 2, 360000);
        fs_sexa(lat_str, Lon, 2, 360000);
        snprintf(el_str, MAXINDIFORMAT, "%lf", El);
        fits_update_key_s(fptr, TSTRING, "LATITUDE", lat_str, "Location Latitude", &status);
        fits_update_key_s(fptr, TSTRING, "LONGITUDE", lon_str, "Location Longitude", &status);
        fits_update_key_s(fptr, TSTRING, "ELEVATION", el_str, "Location Elevation", &status);
    }

    nv = m_Device->getNumber("EQUATORIAL_EOD_COORDS");
    if(nv != nullptr)
    {
        double RA = nv->np[0].value;
        double Dec = nv->np[1].value;

        ln_equ_posn epochPos { 0, 0 }, J2000Pos { 0, 0 };
        epochPos.ra  = RA * 15.0;
        epochPos.dec = Dec;

        // Convert from JNow to J2000
        //TODO use exp_start instead of julian from system
        ln_get_equ_prec2(&epochPos, ln_get_julian_from_sys(), JD2000, &J2000Pos);

        double raJ2000  = J2000Pos.ra / 15.0;
        double decJ2000 = J2000Pos.dec;
        char ra_str[32], de_str[32];

        fs_sexa(ra_str, raJ2000, 2, 360000);
        fs_sexa(de_str, decJ2000, 2, 360000);

        char *raPtr = ra_str, *dePtr = de_str;
        while (*raPtr != '\0')
        {
            if (*raPtr == ':')
                *raPtr = ' ';
            raPtr++;
        }
        while (*dePtr != '\0')
        {
            if (*dePtr == ':')
                *dePtr = ' ';
            dePtr++;
        }

        fits_update_key_s(fptr, TSTRING, "OBJCTRA", ra_str, "Object RA", &status);
        fits_update_key_s(fptr, TSTRING, "OBJCTDEC", de_str, "Object DEC", &status);

        int epoch = 2000;

        //fits_update_key_s(fptr, TINT, "EPOCH", &epoch, "Epoch", &status);
        fits_update_key_s(fptr, TINT, "EQUINOX", &epoch, "Equinox", &status);
    }

    fits_update_key_s(fptr, TSTRING, "DATE-OBS", exp_start, "UTC start date of observation", &status);
    fits_write_comment(fptr, "Generated by INDI", &status);

    setlocale(LC_NUMERIC, orig);
}

void Interface::fits_update_key_s(fitsfile *fptr, int type, std::string name, void *p, std::string explanation,
                                 int *status)
{
    // this function is for removing warnings about deprecated string conversion to char* (from arg 5)
    fits_update_key(fptr, type, name.c_str(), p, const_cast<char *>(explanation.c_str()), status);
}

void* Interface::sendFITS(uint8_t *buf, int len, int bitspersample)
{
    bool sendCapture = (m_Device->getSwitch("UPLOAD_MODE")->sp[0].s == ISS_ON || m_Device->getSwitch("UPLOAD_MODE")->sp[2].s == ISS_ON);
    bool saveCapture = (m_Device->getSwitch("UPLOAD_MODE")->sp[1].s == ISS_ON || m_Device->getSwitch("UPLOAD_MODE")->sp[2].s == ISS_ON);
    fitsfile *fptr = nullptr;
    void *memptr;
    size_t memsize;
    int img_type  = 0;
    int byte_type = 0;
    int status    = 0;
    long naxis    = 2;
    long naxes[2] = {0};
    int nelements = 0;
    std::string bit_depth;
    char error_status[MAXINDINAME];
    switch (bitspersample)
    {
        case 8:
            byte_type = TBYTE;
            img_type  = BYTE_IMG;
            bit_depth = "8 bits per sample";
            break;

        case 16:
            byte_type = TUSHORT;
            img_type  = USHORT_IMG;
            bit_depth = "16 bits per sample";
            break;

        case 32:
            byte_type = TLONG;
            img_type  = LONG_IMG;
            bit_depth = "32 bits per sample";
            break;

        case 64:
            byte_type = TLONGLONG;
            img_type  = LONGLONG_IMG;
            bit_depth = "64 bits double per sample";
            break;

        case -32:
            byte_type = TFLOAT;
            img_type  = FLOAT_IMG;
            bit_depth = "32 bits double per sample";
            break;

        case -64:
            byte_type = TDOUBLE;
            img_type  = DOUBLE_IMG;
            bit_depth = "64 bits double per sample";
            break;

        default:
            DEBUGF(INDI::Logger::DBG_ERROR, "Unsupported bits per sample value %d", bitspersample);
            return nullptr;
    }
    naxes[0] = len;
    naxes[0] = naxes[0] < 1 ? 1 : naxes[0];
    naxes[1] = 1;
    nelements = static_cast<int>(naxes[0]);

    /*DEBUGF(INDI::Logger::DBG_DEBUG, "Exposure complete. Image Depth: %s. Width: %d Height: %d nelements: %d", bit_depth.c_str(), naxes[0],
            naxes[1], nelements);*/

    //  Now we have to send fits format data to the client
    memsize = 5760;
    memptr  = malloc(memsize);
    if (!memptr)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Error: failed to allocate memory: %lu", static_cast<unsigned long>(memsize));
    }

    fits_create_memfile(&fptr, &memptr, &memsize, 2880, realloc, &status);

    if (status)
    {
        fits_report_error(stderr, status); /* print out any error messages */
        fits_get_errstatus(status, error_status);
        DEBUGF(INDI::Logger::DBG_ERROR, "FITS Error: %s", error_status);
        if(memptr != nullptr)
            free(memptr);
        return nullptr;
    }

    fits_create_img(fptr, img_type, naxis, naxes, &status);

    if (status)
    {
        fits_report_error(stderr, status); /* print out any error messages */
        fits_get_errstatus(status, error_status);
        DEBUGF(INDI::Logger::DBG_ERROR, "FITS Error: %s", error_status);
        if(memptr != nullptr)
            free(memptr);
        return nullptr;
    }

    addFITSKeywords(fptr, buf, len);

    fits_write_img(fptr, byte_type, 1, nelements, buf, &status);

    if (status)
    {
        fits_report_error(stderr, status); /* print out any error messages */
        fits_get_errstatus(status, error_status);
        DEBUGF(INDI::Logger::DBG_ERROR, "FITS Error: %s", error_status);
        if(memptr != nullptr)
            free(memptr);
        return nullptr;
    }

    fits_close_file(fptr, &status);

    uploadFile(memptr, memsize, sendCapture, saveCapture, processedFileExtension);

    return memptr;
}

bool Interface::uploadFile(const void *fitsData, size_t totalBytes, bool sendCapture, bool saveCapture, const char* format)
{

    DEBUGF(INDI::Logger::DBG_DEBUG, "Uploading file. Ext: %s, Size: %d, sendCapture? %s, saveCapture? %s",
           format, totalBytes, sendCapture ? "Yes" : "No", saveCapture ? "Yes" : "No");

    FitsB.blob    = const_cast<void *>(fitsData);
    FitsB.bloblen = static_cast<int>(totalBytes);
    snprintf(FitsB.format, MAXINDIBLOBFMT, ".%s", format);
    if (saveCapture)
    {

        FILE *fp = nullptr;

        std::string prefix = m_Device->getText("UPLOAD_SETTINGS")->tp[1].text;
        int maxIndex       = getFileIndex(m_Device->getText("UPLOAD_SETTINGS")->tp[0].text, m_Device->getText("UPLOAD_SETTINGS")->tp[1].text,
                                          format);

        if (maxIndex < 0)
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "Error iterating directory %s. %s", m_Device->getText("UPLOAD_SETTINGS")->tp[0].text,
                   strerror(errno));
            return false;
        }

        if (maxIndex > 0)
        {
            char ts[32];
            struct tm *tp;
            time_t t;
            time(&t);
            tp = localtime(&t);
            strftime(ts, sizeof(ts), "%Y-%m-%dT%H-%M-%S", tp);
            std::string filets(ts);
            prefix = std::regex_replace(prefix, std::regex("ISO8601"), filets);

            char indexString[8];
            snprintf(indexString, 8, "%03d", maxIndex);
            std::string prefixIndex = indexString;
            //prefix.replace(prefix.find("XXX"), std::string::npos, prefixIndex);
            prefix = std::regex_replace(prefix, std::regex("XXX"), prefixIndex);
        }

        snprintf(processedFileName, MAXINDINAME, "%s/%s_%s%s", m_Device->getText("UPLOAD_SETTINGS")->tp[0].text, prefix.c_str(), m_Name, format);

        fp = fopen(processedFileName, "w");
        if (fp == nullptr)
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "Unable to save image file (%s). %s", processedFileName, strerror(errno));
            return false;
        }

        size_t n = 0;
        for (size_t nr = 0; nr < static_cast<size_t>(FitsB.bloblen); nr += n)
            n = fwrite((static_cast<char *>(FitsB.blob) + nr), 1, static_cast<size_t>(FitsB.bloblen - nr), fp);

        fclose(fp);
    }

    FitsB.size = static_cast<int>(totalBytes);
    FitsBP.s   = IPS_OK;

    DEBUG(INDI::Logger::DBG_DEBUG, "Upload complete");

    return true;
}

void Interface::getMinMax(double *min, double *max, uint8_t *buf, int len, int bpp)
{
    int ind         = 0, i, j;
    int integrationHeight = 1;
    int integrationWidth  = len;
    double lmin = 0, lmax = 0;

    switch (bpp)
    {
        case 8:
        {
            uint8_t *integrationBuffer = static_cast<uint8_t *>(buf);
            lmin = lmax = integrationBuffer[0];

            for (i = 0; i < integrationHeight; i++)
                for (j = 0; j < integrationWidth; j++)
                {
                    ind = (i * integrationWidth) + j;
                    if (integrationBuffer[ind] < lmin)
                        lmin = integrationBuffer[ind];
                    else if (integrationBuffer[ind] > lmax)
                        lmax = integrationBuffer[ind];
                }
        }
        break;

        case 16:
        {
            uint16_t *integrationBuffer = reinterpret_cast<uint16_t *>(buf);
            lmin = lmax = integrationBuffer[0];

            for (i = 0; i < integrationHeight; i++)
                for (j = 0; j < integrationWidth; j++)
                {
                    ind = (i * integrationWidth) + j;
                    if (integrationBuffer[ind] < lmin)
                        lmin = integrationBuffer[ind];
                    else if (integrationBuffer[ind] > lmax)
                        lmax = integrationBuffer[ind];
                }
        }
        break;

        case 32:
        {
            uint32_t *integrationBuffer = reinterpret_cast<uint32_t *>(buf);
            lmin = lmax = integrationBuffer[0];

            for (i = 0; i < integrationHeight; i++)
                for (j = 0; j < integrationWidth; j++)
                {
                    ind = (i * integrationWidth) + j;
                    if (integrationBuffer[ind] < lmin)
                        lmin = integrationBuffer[ind];
                    else if (integrationBuffer[ind] > lmax)
                        lmax = integrationBuffer[ind];
                }
        }
        break;

        case 64:
        {
            unsigned long *integrationBuffer = reinterpret_cast<unsigned long *>(buf);
            lmin = lmax = integrationBuffer[0];

            for (i = 0; i < integrationHeight; i++)
                for (j = 0; j < integrationWidth; j++)
                {
                    ind = (i * integrationWidth) + j;
                    if (integrationBuffer[ind] < lmin)
                        lmin = integrationBuffer[ind];
                    else if (integrationBuffer[ind] > lmax)
                        lmax = integrationBuffer[ind];
                }
        }
        break;

        case -32:
        {
            double *integrationBuffer = reinterpret_cast<double *>(buf);
            lmin = lmax = integrationBuffer[0];

            for (i = 0; i < integrationHeight; i++)
                for (j = 0; j < integrationWidth; j++)
                {
                    ind = (i * integrationWidth) + j;
                    if (integrationBuffer[ind] < lmin)
                        lmin = integrationBuffer[ind];
                    else if (integrationBuffer[ind] > lmax)
                        lmax = integrationBuffer[ind];
                }
        }
        break;

        case -64:
        {
            double *integrationBuffer = reinterpret_cast<double *>(buf);
            lmin = lmax = integrationBuffer[0];

            for (i = 0; i < integrationHeight; i++)
                for (j = 0; j < integrationWidth; j++)
                {
                    ind = (i * integrationWidth) + j;
                    if (integrationBuffer[ind] < lmin)
                        lmin = integrationBuffer[ind];
                    else if (integrationBuffer[ind] > lmax)
                        lmax = integrationBuffer[ind];
                }
        }
        break;
    }
    *min = lmin;
    *max = lmax;
}

int Interface::getFileIndex(const char *dir, const char *prefix, const char *ext)
{
    INDI_UNUSED(ext);

    DIR *dpdf = nullptr;
    struct dirent *epdf = nullptr;
    std::vector<std::string> files = std::vector<std::string>();

    std::string prefixIndex = prefix;
    prefixIndex             = regex_replace_compat(prefixIndex, "_ISO8601", "");
    prefixIndex             = regex_replace_compat(prefixIndex, "_XXX", "");

    // Create directory if does not exist
    struct stat st;

    if (stat(dir, &st) == -1)
    {
        DEBUGF(INDI::Logger::DBG_DEBUG, "Creating directory %s...", dir);
        if (_det_mkdir(dir, 0755) == -1)
            DEBUGF(INDI::Logger::DBG_ERROR, "Error creating directory %s (%s)", dir, strerror(errno));
    }

    dpdf = opendir(dir);
    if (dpdf != nullptr)
    {
        while ((epdf = readdir(dpdf)))
        {
            if (strstr(epdf->d_name, prefixIndex.c_str()))
                files.push_back(epdf->d_name);
        }
        closedir(dpdf);
    }
    else
        return -1;

    int maxIndex = 0;

    for (unsigned long i = 0; i < static_cast<unsigned long>(files.size()); i++)
    {
        int index = -1;

        std::string file  = files.at(i);
        std::size_t start = file.find_last_of("_");
        std::size_t end   = file.find_last_of(".");
        if (start != std::string::npos)
        {
            index = atoi(file.substr(start + 1, end).c_str());
            if (index > maxIndex)
                maxIndex = index;
        }
    }

    return (maxIndex + 1);
}
}
