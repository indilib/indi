/*
    Abstract Interface

    Define INDI Driver Interface Requirements

    Copyright (C) 2023 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#pragma once

#include "defaultdevice.h"

/**
 * \class AbstractInterface
   \brief Provides interface to implement hardware class functionality that can be embedded in standard INDI drives.

   A concrete interface is expected to subclass AbstractInterface and override functions as needed.
\author Jasem Mutlaq
*/
namespace INDI
{

class AbstractInterface
{
    protected:
        explicit AbstractInterface(DefaultDevice *parent) : m_DefaultDevice(parent) {}
        virtual ~AbstractInterface();

        /** \brief Initilize light box properties. It is recommended to call this function within initProperties() of your primary device
                \param group Group or tab name to be used to define the properties.
            */
        virtual void initProperties(const char *group) = 0;

        /** \brief Define or delete light properties depending on connection */
        virtual bool updateProperties() = 0;

        /** \brief get properties */
        virtual void ISGetProperties(const char *) {}

        /** \brief Process switch properties */
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
        {
            INDI_UNUSED(dev);
            INDI_UNUSED(name);
            INDI_UNUSED(states);
            INDI_UNUSED(names);
            INDI_UNUSED(n);
            return false;
        }

        /** \brief Process number properties */
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
        {
            INDI_UNUSED(dev);
            INDI_UNUSED(name);
            INDI_UNUSED(values);
            INDI_UNUSED(names);
            INDI_UNUSED(n);
            return false;
        }

        /** \brief Process text properties */
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
        {
            INDI_UNUSED(dev);
            INDI_UNUSED(name);
            INDI_UNUSED(texts);
            INDI_UNUSED(names);
            INDI_UNUSED(n);
            return false;
        };

        /** \brief Process blob properties */
        virtual bool ISSnoopDevice(XMLEle *)
        {
            return false;
        }

        virtual bool saveConfigItems(FILE *)
        {
            return false;
        }

        DefaultDevice *m_DefaultDevice {nullptr};
};
}
