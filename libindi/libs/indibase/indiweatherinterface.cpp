/*
    Weather Interface
    Copyright (C) 2018 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/

#include "indiweatherinterface.h"

#include "indilogger.h"

#include <cstring>

namespace INDI
{

WeatherInterface::WeatherInterface(DefaultDevice *defaultDevice) : m_defaultDevice(defaultDevice)
{
}

WeatherInterface::~WeatherInterface()
{
    for (int i = 0; i < ParametersNP.nnp; i++)
    {
        free(ParametersN[i].aux0);
        free(ParametersN[i].aux1);
        free(ParametersRangeNP[i].np);
    }

    free(ParametersN);
    free(ParametersRangeNP);
    free(critialParametersL);
}

void WeatherInterface::initProperties(const char *statusGroup, const char *paramsGroup)
{
    m_ParametersGroup = paramsGroup;

    // Parameters
    IUFillNumberVector(&ParametersNP, nullptr, 0, m_defaultDevice->getDeviceName(), "WEATHER_PARAMETERS", "Parameters", paramsGroup,
                       IP_RO, 60, IPS_OK);

    // Weather Status
    IUFillLightVector(&critialParametersLP, nullptr, 0, m_defaultDevice->getDeviceName(), "WEATHER_STATUS", "Status", statusGroup,
                      IPS_IDLE);
}

bool WeatherInterface::updateProperties()
{
    if (m_defaultDevice->isConnected())
    {
            if (critialParametersL)
                m_defaultDevice->defineLight(&critialParametersLP);

            if (ParametersN)
                m_defaultDevice->defineNumber(&ParametersNP);

            if (ParametersRangeNP)
            {
                for (int i = 0; i < nRanges; i++)
                    m_defaultDevice->defineNumber(&ParametersRangeNP[i]);
            }
            }
        else
        {
            if (critialParametersL)
                m_defaultDevice->deleteProperty(critialParametersLP.name);

            if (ParametersN)
                m_defaultDevice->deleteProperty(ParametersNP.name);

            if (ParametersRangeNP)
            {
                for (int i = 0; i < nRanges; i++)
                    m_defaultDevice->deleteProperty(ParametersRangeNP[i].name);
            }

        }

    return true;
}

bool WeatherInterface::processNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    INDI_UNUSED(dev);

    // Parameter ranges
    for (int i = 0; i < nRanges; i++)
    {
        if (!strcmp(name, ParametersRangeNP[i].name))
        {
            IUUpdateNumber(&ParametersRangeNP[i], values, names, n);

            ParametersN[i].min               = ParametersRangeNP[i].np[0].value;
            ParametersN[i].max               = ParametersRangeNP[i].np[1].value;
            *(static_cast<double *>(ParametersN[i].aux0)) = ParametersRangeNP[i].np[2].value;

            syncCriticalParameters();

            ParametersRangeNP[i].s = IPS_OK;
            IDSetNumber(&ParametersRangeNP[i], nullptr);

            return true;
        }
    }

    return false;
}

IPState WeatherInterface::updateWeather()
{
    DEBUGDEVICE(m_defaultDevice->getDeviceName(),Logger::DBG_ERROR,
          "updateWeather() must be implemented in Weather device child class to update GEOGRAPHIC_COORD properties.");
    return IPS_ALERT;
}

void WeatherInterface::addParameter(std::string name, std::string label, double numMinOk, double numMaxOk, double percWarning)
{
    DEBUGFDEVICE(m_defaultDevice->getDeviceName(),Logger::DBG_DEBUG, "Parameter %s is added. Ok (%g,%g,%g) ", name.c_str(), numMinOk,
           numMaxOk, percWarning);

    ParametersN = (ParametersN == nullptr) ? static_cast<INumber *>(malloc(sizeof(INumber))) :
                                             static_cast<INumber *>(realloc(ParametersN, (ParametersNP.nnp + 1) * sizeof(INumber)));

    double *warn = static_cast<double *>(malloc(sizeof(double)));

    *warn = percWarning;

    IUFillNumber(&ParametersN[ParametersNP.nnp], name.c_str(), label.c_str(), "%4.2f", numMinOk, numMaxOk, 0, 0);

    ParametersN[ParametersNP.nnp].aux0 = warn;

    ParametersNP.np = ParametersN;
    ParametersNP.nnp++;

    createParameterRange(name, label);
}

void WeatherInterface::setParameterValue(std::string name, double value)
{
    for (int i = 0; i < ParametersNP.nnp; i++)
    {
        if (!strcmp(ParametersN[i].name, name.c_str()))
        {
            ParametersN[i].value = value;
            return;
        }
    }
}

bool WeatherInterface::setCriticalParameter(std::string param)
{
    for (int i = 0; i < ParametersNP.nnp; i++)
    {
        if (!strcmp(ParametersN[i].name, param.c_str()))
        {
            critialParametersL =
                (critialParametersL == nullptr) ?
                    static_cast<ILight *>(malloc(sizeof(ILight))) :
                    static_cast<ILight *>(realloc(critialParametersL, (critialParametersLP.nlp + 1) * sizeof(ILight)));

            IUFillLight(&critialParametersL[critialParametersLP.nlp], param.c_str(), ParametersN[i].label, IPS_IDLE);

            critialParametersLP.lp = critialParametersL;

            critialParametersLP.nlp++;

            return true;
        }
    }

    DEBUGFDEVICE(m_defaultDevice->getDeviceName(),Logger::DBG_WARNING, "Unable to find parameter %s in list of existing parameters!", param.c_str());
    return false;
}

void WeatherInterface::syncCriticalParameters()
{
    if (critialParametersL == nullptr)
        return;

    critialParametersLP.s = IPS_IDLE;

    for (int i = 0; i < critialParametersLP.nlp; i++)
    {
        for (int j = 0; j < ParametersNP.nnp; j++)
        {
            if (!strcmp(critialParametersL[i].name, ParametersN[j].name))
            {
                double warn = *(static_cast<double *>(ParametersN[j].aux0));

                double rangeWarn = (ParametersN[j].max - ParametersN[j].min) * (warn /100);

                if ((ParametersN[j].value < ParametersN[j].min) || (ParametersN[j].value > ParametersN[j].max))
                {
                    critialParametersL[i].s = IPS_ALERT;
                    DEBUGFDEVICE(m_defaultDevice->getDeviceName(),Logger::DBG_WARNING, "Caution: Parameter %s value (%g) is in the danger zone!",
                           ParametersN[j].label, ParametersN[j].value);
                }
                //else if (ParametersN[j].value < (ParametersN[j].min + rangeWarn) || ParametersN[j].value > (ParametersN[j].max - rangeWarn))
                // FIXME This is a hack to prevent warnings parameters which minimum values are zero (e.g. Wind)
                else if (   ((ParametersN[j].value < (ParametersN[j].min + rangeWarn)) && ParametersN[j].min != 0)
                         || ((ParametersN[j].value > (ParametersN[j].max - rangeWarn)) && ParametersN[j].max != 0))
                {
                    critialParametersL[i].s = IPS_BUSY;
                    DEBUGFDEVICE(m_defaultDevice->getDeviceName(),Logger::DBG_WARNING, "Warning: Parameter %s value (%g) is in the warning zone!",
                           ParametersN[j].label, ParametersN[j].value);
                }
                else
                {
                    critialParametersL[i].s = IPS_OK;
                }
                break;
            }
        }

        // The overall state is the worst individual state.
        if (critialParametersL[i].s > critialParametersLP.s)
            critialParametersLP.s = critialParametersL[i].s;
    }

    IDSetLight(&critialParametersLP, nullptr);
}

void WeatherInterface::createParameterRange(std::string name, std::string label)
{
    ParametersRangeNP =
        (ParametersRangeNP == nullptr) ?
            static_cast<INumberVectorProperty *>(malloc(sizeof(INumberVectorProperty))) :
            static_cast<INumberVectorProperty *>(realloc(ParametersRangeNP, (nRanges + 1) * sizeof(INumberVectorProperty)));

    INumber *rangesN = static_cast<INumber *>(malloc(sizeof(INumber) * 3));

    IUFillNumber(&rangesN[0], "MIN_OK", "OK range min", "%4.2f", -1e6, 1e6, 0, ParametersN[nRanges].min);
    IUFillNumber(&rangesN[1], "MAX_OK", "OK range max", "%4.2f", -1e6, 1e6, 0, ParametersN[nRanges].max);
    IUFillNumber(&rangesN[2], "PERC_WARN", "% for Warning", "%g", 0, 100, 1, *(static_cast<double *>(ParametersN[nRanges].aux0)));

    char propName[MAXINDINAME];
    char propLabel[MAXINDILABEL];
    snprintf(propName, MAXINDINAME, "%s", name.c_str());
    snprintf(propLabel, MAXINDILABEL, "%s", label.c_str());

    IUFillNumberVector(&ParametersRangeNP[nRanges], rangesN, 3, m_defaultDevice->getDeviceName(), propName, propLabel, m_ParametersGroup.c_str(),
                       IP_RW, 60, IPS_IDLE);

    nRanges++;
}


}
