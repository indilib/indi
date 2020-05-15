/*******************************************************************************
 Copyright(c) 2010, 2017 Ilia Platone, Jasem Mutlaq. All rights reserved.


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

#include "defaultdevice.h"
#include "indicorrelator.h"

#include "indicom.h"
#include "locale_compat.h"

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

namespace INDI
{

Correlator::Correlator()
{
    setIntegrationFileExtension("fits");
}

Correlator::~Correlator()
{
}

bool Correlator::initProperties()
{
    // PrimaryCorrelator Info
    IUFillNumber(&CorrelatorSettingsN[CORRELATOR_BASELINE_X], "CORRELATOR_BASELINE_X", "Baseline X size (m)", "%16.12f", 1.0e-12, 1.0e+6, 1.0e-12, 10.0);
    IUFillNumber(&CorrelatorSettingsN[CORRELATOR_BASELINE_X], "CORRELATOR_BASELINE_Y", "Baseline Y size (m)", "%16.12f", 1.0e-12, 1.0e+6, 1.0e-12, 10.0);
    IUFillNumber(&CorrelatorSettingsN[CORRELATOR_BASELINE_X], "CORRELATOR_BASELINE_Z", "Baseline Z size (m)", "%16.12f", 1.0e-12, 1.0e+6, 1.0e-12, 10.0);
    IUFillNumber(&CorrelatorSettingsN[CORRELATOR_WAVELENGTH], "CORRELATOR_WAVELENGTH", "Wavelength (m)", "%7.12f", 3.0e-12, 3.0e+6, 3.0e-12, 350.0e-9);
    IUFillNumber(&CorrelatorSettingsN[CORRELATOR_BANDWIDTH], "CORRELATOR_BANDWIDTH", "Bandwidth (Hz)", "%12.0f", 1.0, 100.0e+9, 1.0, 1.42e+9);
    IUFillNumberVector(&CorrelatorSettingsNP, CorrelatorSettingsN, 5, getDeviceName(), "CORRELATOR_SETTINGS", "Correlator Settings", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    setDriverInterface(CORRELATOR_INTERFACE);

    return SensorInterface::initProperties();
}

void Correlator::ISGetProperties(const char *dev)
{
    return processProperties(dev);
}

bool Correlator::updateProperties()
{
    if (isConnected())
    {
        defineNumber(&CorrelatorSettingsNP);

        if (HasCooler())
            defineNumber(&TemperatureNP);
    }
    else
    {
        deleteProperty(CorrelatorSettingsNP.name);

        if (HasCooler())
            deleteProperty(TemperatureNP.name);
    }
    return SensorInterface::updateProperties();
}

bool Correlator::ISSnoopDevice(XMLEle *root)
{
    return processSnoopDevice(root);
}

bool Correlator::ISNewText(const char *dev, const char *name, char *values[], char *names[], int n)
{
    return processText(dev, name, values, names, n);
}

bool Correlator::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()) && !strcmp(name, CorrelatorSettingsNP.name)) {
        IDSetNumber(&CorrelatorSettingsNP, nullptr);
    }
    return processNumber(dev, name, values, names, n);
}

bool Correlator::ISNewSwitch(const char *dev, const char *name, ISState *values, char *names[], int n)
{
    return processSwitch(dev, name, values, names, n);
}

bool Correlator::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
           char *formats[], char *names[], int n)
{
    return processBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

void Correlator::setBaseline(Baseline bl)
{
    baseline = bl;

    CorrelatorSettingsN[Correlator::CORRELATOR_BASELINE_X].value = baseline.x;
    CorrelatorSettingsN[Correlator::CORRELATOR_BASELINE_Y].value = baseline.y;
    CorrelatorSettingsN[Correlator::CORRELATOR_BASELINE_Z].value = baseline.z;

    IDSetNumber(&CorrelatorSettingsNP, nullptr);
}

void Correlator::setWavelength(double wl)
{
    wavelength = wl;

    CorrelatorSettingsN[Correlator::CORRELATOR_WAVELENGTH].value = wl;

    IDSetNumber(&CorrelatorSettingsNP, nullptr);
}

void Correlator::setBandwidth(double bw)
{
    bandwidth = bw;

    CorrelatorSettingsN[Correlator::CORRELATOR_BANDWIDTH].value = bw;

    IDSetNumber(&CorrelatorSettingsNP, nullptr);
}

void Correlator::SetCorrelatorCapability(uint32_t cap)
{
    SetCapability(cap);
    setDriverInterface(getDriverInterface());
}

Correlator::UVCoordinate Correlator::getUVCoordinates()
{
    UVCoordinate ret;
    double *bl = static_cast<double*>(malloc(sizeof(double)*3));
    double lst = get_local_sidereal_time(Lon);
    double ha = get_local_hour_angle(lst, RA);
    bl[0] = baseline.x;
    bl[1] = baseline.y;
    bl[2] = baseline.z;
    double *uvcoord = interferometry_uv_coords_hadec(ha, Dec, bl, wavelength);
    ret.u = uvcoord[0];
    ret.v = uvcoord[1];
    free(bl);
    free(uvcoord);
    return ret;
}

bool Correlator::StartIntegration(double duration)
{
    INDI_UNUSED(duration);
    DEBUGF(Logger::DBG_WARNING, "Correlator::StartIntegration %4.2f - Not supported", duration);
    return false;
}

void Correlator::setMinMaxStep(const char *property, const char *element, double min, double max, double step,
                                   bool sendToClient)
{
    INumberVectorProperty *vp = nullptr;

    if (!strcmp(property, CorrelatorSettingsNP.name)) {
        vp = &FramedIntegrationNP;

        INumber *np = IUFindNumber(vp, element);
        if (np)
        {
            np->min  = min;
            np->max  = max;
            np->step = step;

            if (sendToClient)
                IUUpdateMinMax(vp);
        }
    }
    INDI::SensorInterface::setMinMaxStep(property, element, min, max, step, sendToClient);
}
}

