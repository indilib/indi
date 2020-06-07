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
#include "libastro.h"

#include <fitsio.h>

#include <libnova/julian_day.h>
#include <libnova/ln_types.h>
#include <libnova/precession.h>

#include <regex>

#include <dirent.h>
#include <cerrno>
#include <locale.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <zlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

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

Interface::Interface(INDI::DefaultDevice *dev, Type type, const char *name, const char *label) : m_Device(dev),
    m_Name(name), m_Label(label), m_Type(type)
{
    char activatestrname[MAXINDINAME];
    char activatestrlabel[MAXINDILABEL];
    sprintf(activatestrname, "DSP_ACTIVATE_%s", m_Name);
    sprintf(activatestrlabel, "Activate %s", m_Label);
    IUFillSwitch(&ActivateS[0], "DSP_ACTIVATE_ON", "Activate", ISState::ISS_OFF);
    IUFillSwitch(&ActivateS[1], "DSP_ACTIVATE_OFF", "Deactivate", ISState::ISS_ON);
    IUFillSwitchVector(&ActivateSP, ActivateS, 2, getDeviceName(), activatestrname, activatestrlabel, DSP_TAB, IP_RW,
                       ISR_1OFMANY, 60, IPS_IDLE);

    IUFillBLOB(&FitsB, m_Name, m_Label, "");
    IUFillBLOBVector(&FitsBP, &FitsB, 1, getDeviceName(), m_Name, m_Label, DSP_TAB, IP_RO, 60, IPS_IDLE);
    BufferSizes = nullptr;
    BufferSizesQty = 0;
    strncpy (FitsB.format, ".fits", MAXINDIFORMAT);
}

Interface::~Interface()
{
}

const char *Interface::getDeviceName()
{
    return m_Device->getDeviceName();
}

void Interface::ISGetProperties(const char *dev)
{
    INDI_UNUSED(dev);
    if (m_Device->isConnected())
    {
        m_Device->defineSwitch(&ActivateSP);
    }
    else
    {
        m_Device->deleteProperty(ActivateSP.name);
        PluginActive = false;
        Deactivated();
    }
}

bool Interface::updateProperties()
{
    if (m_Device->isConnected())
    {
        m_Device->defineSwitch(&ActivateSP);
    }
    else
    {
        m_Device->deleteProperty(ActivateSP.name);
        PluginActive = false;
        Deactivated();
    }
    return true;
}

bool Interface::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(!strcmp(dev, getDeviceName()) && !strcmp(name, ActivateSP.name))
    {
        IUUpdateSwitch(&ActivateSP, states, names, n);
        if(ActivateSP.sp[0].s == ISS_ON)
        {
            PluginActive = true;
            Activated();
        }
        else
        {
            PluginActive = false;
            Deactivated();
        }
        IDSetSwitch(&ActivateSP, nullptr);
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

bool Interface::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
                          char *names[], int n)
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

uint8_t* Interface::Callback(uint8_t* buf, uint32_t ndims, int* dims, int bits_per_sample)
{
    INDI_UNUSED(buf);
    INDI_UNUSED(ndims);
    INDI_UNUSED(dims);
    INDI_UNUSED(bits_per_sample);
    DEBUG(INDI::Logger::DBG_WARNING, "Interface::Callback -  Should never get here");
    return nullptr;
}

bool Interface::processBLOB(uint8_t* buf, uint32_t ndims, int* dims, int bits_per_sample)
{
    if(PluginActive)
    {
        bool sendCapture = (m_Device->getSwitch("UPLOAD_MODE")->sp[0].s == ISS_ON
                            || m_Device->getSwitch("UPLOAD_MODE")->sp[2].s == ISS_ON);
        bool saveCapture = (m_Device->getSwitch("UPLOAD_MODE")->sp[1].s == ISS_ON
                            || m_Device->getSwitch("UPLOAD_MODE")->sp[2].s == ISS_ON);

        if (sendCapture || saveCapture)
        {
            setSizes(ndims, dims);
            setBPS(bits_per_sample);
            uint8_t* buffer = Callback(buf, ndims, dims, bits_per_sample);
            if (buffer)
            {
                LOGF_INFO("%s processing done. Creating file..", m_Label);
                if (!strcmp(FitsB.format, ".fits"))
                {
                    sendFITS(buffer, sendCapture, saveCapture);
                }
                else
                {
                    long len = 1;
                    uint32_t i;
                    for (len = 1, i = 0; i < BufferSizesQty; len *= BufferSizes[i++]);
                    len *= getBPS() / 8;
                    uploadFile(buffer, len, sendCapture, saveCapture, FitsB.format);
                }

                if (sendCapture)
                    IDSetBLOB(&FitsBP, nullptr);
            }
            free(buffer);
        }
    }
    return true;
}

void Interface::Activated()
{
    m_Device->defineBLOB(&FitsBP);
}

void Interface::Deactivated()
{
    m_Device->deleteProperty(FitsBP.name);
}

bool Interface::saveConfigItems(FILE *fp)
{
    INDI_UNUSED(fp);
    return true;
}

void Interface::addFITSKeywords(fitsfile *fptr)
{
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
        //ln_get_equ_prec2(&epochPos, ln_get_julian_from_sys(), JD2000, &J2000Pos);
        LibAstro::ObservedToJ2000(&epochPos, ln_get_julian_from_sys(), &J2000Pos);

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

dsp_stream_p Interface::loadFITS(char* buffer, int len)
{
    dsp_stream_p loaded_stream = dsp_stream_new();
    long ndims;
    long bits_per_sample;
    int status;
    off_t offset;
    off_t head;
    off_t end;
    fitsfile *fptr;
    void* buf;
    char error_status[MAXINDINAME];
    char comment[MAXINDINAME];
    char filename[MAXINDIMESSAGE];
    sprintf(filename, "/tmp/%s_%s_%08X.fits", m_Label, getDeviceName(), rand());
    int fd = creat(filename, 0600);
    if(fd >= 0) {
        int written = write(fd, buffer, len);
        if(written != len)
            return nullptr;
        close(fd);
    }
    fits_open_file(&fptr, filename, 0, &status);
    if(status != 0)
        goto load_err;
    fits_read_key_lng(fptr, "BITPIX", &bits_per_sample, comment, &status);
    if(status != 0)
        goto load_err;
    fits_read_key_lng(fptr, "NAXIS", &ndims, comment, &status);
    if(status != 0)
        goto load_err;
    for (int d = 1; d <= ndims; d++) {
        char query[MAXINDINAME];
        long value;
        sprintf(query, "NAXIS%d", d);
        fits_read_key_lng(fptr, query, &value, comment, &status);
        if(status != 0)
            goto load_err;
        dsp_stream_add_dim(loaded_stream, value);
    }
    dsp_stream_alloc_buffer(loaded_stream, loaded_stream->len);
    fits_get_hduoff(fptr, &head, &offset, &end, &status);
    buf = static_cast<void*>(&buffer[offset]);
    switch (bits_per_sample)
    {
        case 8:
            dsp_buffer_copy((static_cast<uint8_t *>(buf)), loaded_stream->buf, loaded_stream->len);
            goto err_free;
        case 16:
            dsp_buffer_copy((static_cast<uint16_t *>(buf)), loaded_stream->buf, loaded_stream->len);
            goto err_free;
        case 32:
            dsp_buffer_copy((static_cast<uint32_t *>(buf)), loaded_stream->buf, loaded_stream->len);
            goto err_free;
        case 64:
            dsp_buffer_copy((static_cast<unsigned long *>(buf)), loaded_stream->buf, loaded_stream->len);
            goto err_free;
        case -32:
            dsp_buffer_copy((static_cast<float *>(buf)), loaded_stream->buf, loaded_stream->len);
            goto err_free;
        case -64:
            dsp_buffer_copy((static_cast<double *>(buf)), loaded_stream->buf, loaded_stream->len);
            goto dsp_err;
        default:
        break;
    }
load_err:
    fits_report_error(stderr, status); /* print out any error messages */
    fits_get_errstatus(status, error_status);
    LOGF_ERROR("FITS Error: %s", error_status);
dsp_err:
    //Destroy the dsp stream
    dsp_stream_free_buffer(loaded_stream);
    dsp_stream_free(loaded_stream);
    return nullptr;
err_free:
    fits_close_file(fptr, &status);
    unlink(filename);
    return loaded_stream;
}

bool Interface::sendFITS(uint8_t *buf, bool sendCapture, bool saveCapture)
{
    int img_type  = USHORT_IMG;
    int byte_type = TUSHORT;
    std::string bit_depth = "16 bits per sample";
    switch (getBPS())
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
            byte_type = TULONG;
            img_type  = ULONG_IMG;
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
            DEBUGF(INDI::Logger::DBG_ERROR, "Unsupported bits per sample value %d", getBPS());
            return false;
    }

    fitsfile *fptr = nullptr;
    void *memptr;
    size_t memsize;
    int status    = 0;
    int naxis    = static_cast<int>(BufferSizesQty);
    long *naxes = static_cast<long*>(malloc(sizeof(long) * BufferSizesQty));
    long nelements = 0;

    for (uint32_t i = 0, nelements = 1; i < BufferSizesQty; nelements *= static_cast<long>(BufferSizes[i++]))
        naxes[i] = BufferSizes[i];
    char error_status[MAXINDINAME];

    //  Now we have to send fits format data to the client
    memsize = 5760;
    memptr  = malloc(memsize);
    if (!memptr)
    {
        LOGF_ERROR("Error: failed to allocate memory: %lu", memsize);
        return false;
    }

    fits_create_memfile(&fptr, &memptr, &memsize, 2880, realloc, &status);

    if (status)
    {
        fits_report_error(stderr, status); /* print out any error messages */
        fits_get_errstatus(status, error_status);
        fits_close_file(fptr, &status);
        free(memptr);
        LOGF_ERROR("FITS Error: %s", error_status);
        return false;
    }

    fits_create_img(fptr, img_type, naxis, naxes, &status);

    if (status)
    {
        fits_report_error(stderr, status); /* print out any error messages */
        fits_get_errstatus(status, error_status);
        fits_close_file(fptr, &status);
        free(memptr);
        LOGF_ERROR("FITS Error: %s", error_status);
        return false;
    }

    addFITSKeywords(fptr);

    fits_write_img(fptr, byte_type, 1, nelements, buf, &status);

    if (status)
    {
        fits_report_error(stderr, status); /* print out any error messages */
        fits_get_errstatus(status, error_status);
        fits_close_file(fptr, &status);
        free(memptr);
        LOGF_ERROR("FITS Error: %s", error_status);
        return false;
    }

    fits_close_file(fptr, &status);

    uploadFile(memptr, memsize, sendCapture, saveCapture, "fits");
    free(memptr);
    return true;
}

bool Interface::uploadFile(const void *fitsData, size_t totalBytes, bool sendCapture, bool saveCapture, const char* format)
{

    DEBUGF(INDI::Logger::DBG_DEBUG, "Uploading file. Ext: %s, Size: %d, sendCapture? %s, saveCapture? %s",
           format, totalBytes, sendCapture ? "Yes" : "No", saveCapture ? "Yes" : "No");

    FitsB.blob    = const_cast<void *>(fitsData);
    FitsB.bloblen = static_cast<int>(totalBytes);
    if (saveCapture)
    {

        FILE *fp = nullptr;

        std::string prefix = m_Device->getText("UPLOAD_SETTINGS")->tp[1].text;

        int maxIndex = getFileIndex(m_Device->getText("UPLOAD_SETTINGS")->tp[0].text, prefix.c_str(),
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

        snprintf(processedFileName, MAXINDINAME, "%s/%s_%s.%s", m_Device->getText("UPLOAD_SETTINGS")->tp[0].text, prefix.c_str(),
                 m_Name, format);

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
        LOGF_INFO("File saved in %s.", processedFileName);
    }

    FitsB.size = totalBytes;
    FitsBP.s   = IPS_OK;

    if (sendCapture)
    {

        auto start = std::chrono::high_resolution_clock::now();
        IDSetBLOB(&FitsBP, nullptr);
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = end - start;
        LOGF_DEBUG("BLOB transfer took %g seconds", diff.count());
    }


    DEBUG(INDI::Logger::DBG_DEBUG, "Upload complete");

    return true;
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

void Interface::setStream(void *buf, uint32_t dims, int *sizes, int bits_per_sample)
{
    //Create the dsp stream
    stream = dsp_stream_new();
    for(uint32_t dim = 0; dim < dims; dim++)
        dsp_stream_add_dim(stream, sizes[dim]);
    dsp_stream_alloc_buffer(stream, stream->len);
    switch (bits_per_sample)
    {
        case 8:
            dsp_buffer_copy((static_cast<uint8_t *>(buf)), stream->buf, stream->len);
            break;
        case 16:
            dsp_buffer_copy((static_cast<uint16_t *>(buf)), stream->buf, stream->len);
            break;
        case 32:
            dsp_buffer_copy((static_cast<uint32_t *>(buf)), stream->buf, stream->len);
            break;
        case 64:
            dsp_buffer_copy((static_cast<unsigned long *>(buf)), stream->buf, stream->len);
            break;
        case -32:
            dsp_buffer_copy((static_cast<float *>(buf)), stream->buf, stream->len);
            break;
        case -64:
            dsp_buffer_copy((static_cast<double *>(buf)), stream->buf, stream->len);
            break;
        default:
            dsp_stream_free_buffer(stream);
            //Destroy the dsp stream
            dsp_stream_free(stream);
    }
}

uint8_t* Interface::getStream()
{
    void *buffer = malloc(stream->len * getBPS() / 8);
    switch (getBPS())
    {
        case 8:
            dsp_buffer_copy(stream->buf, (static_cast<uint8_t *>(buffer)), stream->len);
            break;
        case 16:
            dsp_buffer_copy(stream->buf, (static_cast<uint16_t *>(buffer)), stream->len);
            break;
        case 32:
            dsp_buffer_copy(stream->buf, (static_cast<uint32_t *>(buffer)), stream->len);
            break;
        case 64:
            dsp_buffer_copy(stream->buf, (static_cast<unsigned long *>(buffer)), stream->len);
            break;
        case -32:
            dsp_buffer_copy(stream->buf, (static_cast<float *>(buffer)), stream->len);
            break;
        case -64:
            dsp_buffer_copy(stream->buf, (static_cast<double *>(buffer)), stream->len);
            break;
        default:
            free (buffer);
            break;
    }
    //Destroy the dsp stream
    dsp_stream_free_buffer(stream);
    dsp_stream_free(stream);
    return static_cast<uint8_t *>(buffer);
}
}
