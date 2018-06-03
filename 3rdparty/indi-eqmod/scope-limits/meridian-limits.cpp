/* Copyright 2018 starrybird (star48b@gmail.com) */
/* This file is part of the Skywatcher Protocol INDI driver.

    The Skywatcher Protocol INDI driver is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    The Skywatcher Protocol INDI driver is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with the Skywatcher Protocol INDI driver.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "meridian-limits.h"

#include "../eqmod.h"

#include <indicom.h>
#include <cstring>
#include <wordexp.h>


MeridianLimits::MeridianLimits(INDI::Telescope *t, Skywatcher *m)
{
    telescope = t;
    mount = m;
    raMotorEncoderEast = 0;
    raMotorEncoderWest = 0;
    strcpy(errorline, "Bad number format line     ");
    sline = errorline + 23;
    MeridianInitialized = false;
}

MeridianLimits::~MeridianLimits()
{

}

const char *MeridianLimits::getDeviceName()
{
    return telescope->getDeviceName();
}

void MeridianLimits::Reset()
{
    raMotorEncoderEast = 0;
    raMotorEncoderWest = 0;
}

void MeridianLimits::Init()
{
    if (!MeridianInitialized)
    {
        char *res = LoadDataFile(IUFindText(MeridianLimitsDataFileTP, "MERIDIAN_LIMITS_FILENAME")->text);

        if (res)
        {
            LOGF_WARN("Can not load MeridianLimits Data File %s: %s",
                      IUFindText(MeridianLimitsDataFileTP, "MERIDIAN_LIMITS_FILENAME")->text, res);
        }
        else
        {
            LOGF_INFO("MeridianLimits: Data loaded from file %s",
                      IUFindText(MeridianLimitsDataFileTP, "MERIDIAN_LIMITS_FILENAME")->text);
        }
    }

    MeridianInitialized = true;
}

bool MeridianLimits::initProperties()
{
    /* Load properties from the skeleton file */

    telescope->buildSkeleton("indi_eqmod_meridian_limits_sk.xml");

    MeridianLimitsDataFileTP      = telescope->getText("MERIDIAN_LIMITS_DATA_FILE");
    MeridianLimitsDataFitsBP      = telescope->getBLOB("MERIDIAN_LIMITS_DATA_FITS");
    MeridianLimitsStepNP          = telescope->getNumber("MERIDIAN_LIMITS_STEP");
    MeridianLimitsSetCurrentSP    = telescope->getSwitch("MERIDIAN_LIMITS_SET_CURRENT");
    MeridianLimitsFileOperationSP = telescope->getSwitch("MERIDIAN_LIMITS_FILE_OPERATION");
    MeridianLimitsOnLimitSP       = telescope->getSwitch("MERIDIAN_LIMITS_ON_LIMIT");

    return true;
}

void MeridianLimits::ISGetProperties()
{
    if (telescope->isConnected())
    {
        telescope->defineText(MeridianLimitsDataFileTP);
        telescope->defineBLOB(MeridianLimitsDataFitsBP);
        telescope->defineNumber(MeridianLimitsStepNP);
        telescope->defineSwitch(MeridianLimitsSetCurrentSP);
        telescope->defineSwitch(MeridianLimitsFileOperationSP);
        telescope->defineSwitch(MeridianLimitsOnLimitSP);
    }
}

bool MeridianLimits::updateProperties()
{
    //IDLog("MeridianLimits update properties connected = %d.\n",(telescope->isConnected()?1:0) );
    if (telescope->isConnected())
    {
        telescope->defineText(MeridianLimitsDataFileTP);
        telescope->defineBLOB(MeridianLimitsDataFitsBP);
        telescope->defineNumber(MeridianLimitsStepNP);
        telescope->defineSwitch(MeridianLimitsSetCurrentSP);
        telescope->defineSwitch(MeridianLimitsFileOperationSP);
        telescope->defineSwitch(MeridianLimitsOnLimitSP);

        Init();
    }
    else if (MeridianLimitsDataFileTP)
    {
        telescope->deleteProperty(MeridianLimitsDataFileTP->name);
        telescope->deleteProperty(MeridianLimitsDataFitsBP->name);
        telescope->deleteProperty(MeridianLimitsStepNP->name);
        telescope->deleteProperty(MeridianLimitsSetCurrentSP->name);
        telescope->deleteProperty(MeridianLimitsFileOperationSP->name);
        telescope->deleteProperty(MeridianLimitsOnLimitSP->name);
    }

    return true;
}

bool MeridianLimits::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device
    if (strcmp(dev, telescope->getDeviceName()) == 0)
    {
        if (MeridianLimitsStepNP && strcmp(name, MeridianLimitsStepNP->name) == 0)
        {
            unsigned long encoderPierEast = (unsigned long)(values[0]);
            unsigned long encoderPierWest = (unsigned long)(values[1]);

            if (encoderPierEast > encoderPierWest)
            {
                LOG_WARN("Encoder for pier side EAST is LARGER than encoder setting for pier side WEST !");
                MeridianLimitsStepNP->s = IPS_ALERT;
                IDSetNumber(MeridianLimitsStepNP, NULL);
                return false;
            }

            if (IUUpdateNumber(MeridianLimitsStepNP, values, names, n) != 0)
            {
                LOG_WARN("Update encoder failed !");
                MeridianLimitsStepNP->s = IPS_ALERT;
                IDSetNumber(MeridianLimitsStepNP, NULL);
                return false;
            }

            MeridianLimitsStepNP->s = IPS_OK;

            raMotorEncoderEast = encoderPierEast;
            raMotorEncoderWest = encoderPierWest;

            IDSetNumber(MeridianLimitsStepNP, NULL);
            LOG_INFO("Meridian limit encoder has been updated.");

            return true;
        }
    }

    return false;
}

bool MeridianLimits::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (strcmp(dev, telescope->getDeviceName()) == 0)
    {
        if (MeridianLimitsSetCurrentSP && strcmp(name, MeridianLimitsSetCurrentSP->name) == 0)
        {
            ISwitch *sw;

            IUUpdateSwitch(MeridianLimitsSetCurrentSP, states, names, n);
            sw = IUFindOnSwitch(MeridianLimitsSetCurrentSP);

            if (!strcmp(sw->name, "MERIDIAN_LIMITS_SET_CURRENT_EAST"))
            {
                double values[2];

                const char *names[] = 
                {
                    "MERIDIAN_LIMITS_ENCODER_PIERSIDE_EAST", 
                    "MERIDIAN_LIMITS_ENCODER_PIERSIDE_WEST" 
                };

                unsigned long raEncoder = mount->GetRAEncoder();

                values[0] = IUFindNumber(MeridianLimitsStepNP, names[0])->value;
                values[1] = IUFindNumber(MeridianLimitsStepNP, names[1])->value;

                values[0] = double(raEncoder);

                if (raEncoder > raMotorEncoderWest)
                {
                    values[1] = double(raMotorEncoderWest);
                }

                if (IUUpdateNumber(MeridianLimitsStepNP, values, (char **)names, 2) != 0)
                {
                    LOG_WARN("Update encoder failed !");
                    MeridianLimitsSetCurrentSP->s = IPS_ALERT;
                    IDSetSwitch(MeridianLimitsSetCurrentSP, NULL);
                    return false;
                }

                MeridianLimitsStepNP->s = IPS_OK;
                IDSetNumber(MeridianLimitsStepNP, NULL);
                MeridianLimitsSetCurrentSP->s = IPS_OK;
                IDSetSwitch(MeridianLimitsSetCurrentSP, NULL);

                raMotorEncoderEast = raEncoder;

                if (raEncoder > raMotorEncoderWest)
                {
                    raMotorEncoderWest = raEncoder;
                }

                LOG_INFO("Meridian limit encoder (pier side WEST) has been updated.");

                return true;
            }

            if (!strcmp(sw->name, "MERIDIAN_LIMITS_SET_CURRENT_WEST"))
            {
                double values[2];

                const char *names[] = 
                {
                    "MERIDIAN_LIMITS_ENCODER_PIERSIDE_EAST", 
                    "MERIDIAN_LIMITS_ENCODER_PIERSIDE_WEST" 
                };

                unsigned long raEncoder = mount->GetRAEncoder();

                values[0] = IUFindNumber(MeridianLimitsStepNP, names[0])->value;
                values[1] = IUFindNumber(MeridianLimitsStepNP, names[1])->value;

                values[1] = double(raEncoder);

                if (raEncoder < raMotorEncoderEast)
                {
                    values[0] = double(raMotorEncoderEast);
                }

                if (IUUpdateNumber(MeridianLimitsStepNP, values, (char **)names, 2) != 0)
                {
                    LOG_WARN("Update encoder failed !");
                    MeridianLimitsSetCurrentSP->s = IPS_ALERT;
                    IDSetSwitch(MeridianLimitsSetCurrentSP, NULL);
                    return false;
                }

                MeridianLimitsStepNP->s = IPS_OK;
                IDSetNumber(MeridianLimitsStepNP, NULL);
                MeridianLimitsSetCurrentSP->s = IPS_OK;
                IDSetSwitch(MeridianLimitsSetCurrentSP, NULL);

                raMotorEncoderWest = raEncoder;

                if (raEncoder < raMotorEncoderEast)
                {
                    raMotorEncoderEast = raEncoder;
                }

                LOG_INFO("Meridian limit encoder (pier side EAST) has been updated.");

                return true;
            }
        }

        if (MeridianLimitsFileOperationSP && strcmp(name, MeridianLimitsFileOperationSP->name) == 0)
        {
            ISwitch *sw;

            IUUpdateSwitch(MeridianLimitsFileOperationSP, states, names, n);
            sw = IUFindOnSwitch(MeridianLimitsFileOperationSP);

            if (!strcmp(sw->name, "MERIDIAN_LIMITS_WRITE_FILE"))
            {
                char *res;

                res = WriteDataFile(IUFindText(MeridianLimitsDataFileTP, "MERIDIAN_LIMITS_FILENAME")->text);

                if (res)
                {
                    LOGF_WARN("Can not save MeridianLimits Data to file %s: %s",
                              IUFindText(MeridianLimitsDataFileTP, "MERIDIAN_LIMITS_FILENAME")->text, res);
                    MeridianLimitsFileOperationSP->s = IPS_ALERT;
                }
                else
                {
                    LOGF_INFO("MeridianLimits: Data saved in file %s",
                              IUFindText(MeridianLimitsDataFileTP, "MERIDIAN_LIMITS_FILENAME")->text);
                    MeridianLimitsFileOperationSP->s = IPS_OK;
                }
            }
            else if (!strcmp(sw->name, "MERIDIAN_LIMITS_LOAD_FILE"))
            {
                char *res;

                res = LoadDataFile(IUFindText(MeridianLimitsDataFileTP, "MERIDIAN_LIMITS_FILENAME")->text);

                if (res)
                {
                    LOGF_WARN("Can not load MeridianLimits Data File %s: %s",
                              IUFindText(MeridianLimitsDataFileTP, "MERIDIAN_LIMITS_FILENAME")->text, res);
                    MeridianLimitsFileOperationSP->s = IPS_ALERT;
                }
                else
                {
                    LOGF_INFO("MeridianLimits: Data loaded from file %s",
                              IUFindText(MeridianLimitsDataFileTP, "MERIDIAN_LIMITS_FILENAME")->text);
                    MeridianLimitsFileOperationSP->s = IPS_OK;
                }
            }

            IDSetSwitch(MeridianLimitsFileOperationSP, NULL);

            return true;
        }

        if (MeridianLimitsOnLimitSP && strcmp(name, MeridianLimitsOnLimitSP->name) == 0)
        {
            MeridianLimitsOnLimitSP->s = IPS_OK;
            IUUpdateSwitch(MeridianLimitsOnLimitSP, states, names, n);
            IDSetSwitch(MeridianLimitsOnLimitSP, NULL);
            return true;
        }
    }

    return false;
}

bool MeridianLimits::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (strcmp(dev, telescope->getDeviceName()) == 0)
    {
        if (MeridianLimitsDataFileTP && (strcmp(name, MeridianLimitsDataFileTP->name) == 0))
        {
            IUUpdateText(MeridianLimitsDataFileTP, texts, names, n);
            IDSetText(MeridianLimitsDataFileTP, NULL);

            return true;
        }
    }

    return false;
}

bool MeridianLimits::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                              char *formats[], char *names[], int num)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(num);

    return false;
}

char *MeridianLimits::WriteDataFile(const char *filename)
{
    wordexp_t wexp;
    FILE *fp;
    char lon[10], lat[10];
    INumberVectorProperty *geo = telescope->getNumber("GEOGRAPHIC_COORD");
    INumber *nlon              = IUFindNumber(geo, "LONG");
    INumber *nlat              = IUFindNumber(geo, "LAT");

    if (wordexp(filename, &wexp, 0))
    {
        wordfree(&wexp);
        return (char *)("Badly formed filename");
    }

    if (!(fp = fopen(wexp.we_wordv[0], "w")))
    {
        wordfree(&wexp);
        return strerror(errno);
    }

    setlocale(LC_NUMERIC, "C");

    numberFormat(lon, "%10.6m", nlon->value);
    numberFormat(lat, "%10.6m", nlat->value);
    fprintf(fp, "# Meridian Data for device %s\n", getDeviceName());
    fprintf(fp, "# Location: longitude=%s latitude=%s\n", lon, lat);
    fprintf(fp, "# Created on %s by %s\n", timestamp(), telescope->getDriverName());
    fprintf(fp, "%ld %ld\n", raMotorEncoderEast, raMotorEncoderWest);

    fclose(fp);
    setlocale(LC_NUMERIC, "");

    return NULL;
}

char *MeridianLimits::LoadDataFile(const char *filename)
{
    wordexp_t wexp;
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    int nline = 0, pos = 0;

    INumber *encoderEast = IUFindNumber(MeridianLimitsStepNP, "MERIDIAN_LIMITS_ENCODER_PIERSIDE_EAST");
    INumber *encoderWest = IUFindNumber(MeridianLimitsStepNP, "MERIDIAN_LIMITS_ENCODER_PIERSIDE_WEST");

    if (wordexp(filename, &wexp, 0))
    {
        wordfree(&wexp);
        return (char *)("Badly formed filename");
    }

    if (!(fp = fopen(wexp.we_wordv[0], "r")))
    {
        wordfree(&wexp);
        return strerror(errno);
    }

    wordfree(&wexp);
    Reset();
    setlocale(LC_NUMERIC, "C");

    while ((read = getline(&line, &len, fp)) != -1)
    {
        char *s = line;

        while ((*s == ' ') || (*s == '\t'))
            s++;

        if (*s == '#')
            continue;

        if (sscanf(s, "%ld%n", &raMotorEncoderEast, &pos) != 1)
        {
            fclose(fp);
            snprintf((char *)sline, 4, "%d", nline);
            setlocale(LC_NUMERIC, "");
            return (char *)errorline;
        }

        s += pos;
        while ((*s == ' ') || (*s == '\t'))
            s++;

        if (sscanf(s, "%ld%n", &raMotorEncoderWest, &pos) != 1)
        {
            fclose(fp);
            snprintf((char *)sline, 4, "%d", nline);
            setlocale(LC_NUMERIC, "");
            return (char *)errorline;
        }

        nline++;
        pos = 0;
    }

    encoderEast->value = raMotorEncoderEast;
    encoderWest->value = raMotorEncoderWest;

    MeridianLimitsStepNP->s = IPS_OK;

    IDSetNumber(MeridianLimitsStepNP, NULL);

    if (line)
        free(line);

    fclose(fp);
    setlocale(LC_NUMERIC, "");

    return NULL;
}

bool MeridianLimits::inLimits(unsigned long ra_motor_step)
{
    return (raMotorEncoderEast <= ra_motor_step && ra_motor_step <= raMotorEncoderWest);
}

bool MeridianLimits::checkLimits(unsigned long ra_motor_step, INDI::Telescope::TelescopeStatus status)
{
    bool abortscope = false;
    const char *abortmsg;
    ISwitch *swaborttrack = IUFindSwitch(MeridianLimitsOnLimitSP, "MERIDIAN_LIMITS_ON_LIMIT_TRACK");
    ISwitch *swabortslew  = IUFindSwitch(MeridianLimitsOnLimitSP, "MERIDIAN_LIMITS_ON_LIMIT_SLEW");

    if (!(inLimits(ra_motor_step)))
    {
        abortmsg = "Nothing to abort.";

        if ((status == INDI::Telescope::SCOPE_TRACKING) && (swaborttrack->s == ISS_ON))
        {
            abortmsg   = "Abort Tracking.";
            abortscope = true;
        }

        if ((status == INDI::Telescope::SCOPE_SLEWING) && (swabortslew->s == ISS_ON))
        {
            abortmsg   = "Abort Slewing.";
            abortscope = true;
        }

        LOGF_WARN("Meridian Limits: Scope outside limits. %s", abortmsg);
    }

    return (abortscope);
}
