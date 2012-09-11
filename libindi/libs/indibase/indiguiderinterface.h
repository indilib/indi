/*
    Guider Interface
    Copyright (C) 2011 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#ifndef GUIDERINTERFACE_H
#define GUIDERINTERFACE_H

#include "indibase.h"

/**
 * \class INDI::GuiderInterface
   \brief Provides interface to implement guider (ST4) port functionality.

   initFilterProperties() must be called before any other function to initilize the guider properties.
\author Jasem Mutlaq
*/
class INDI::GuiderInterface
{

public:

    /** \brief Guide north for ms milliseconds
        \return True if OK, false otherwise
    */
    virtual bool GuideNorth(float ms) = 0;

    /** \brief Guide south for ms milliseconds
        \return True if OK, false otherwise
    */
    virtual bool GuideSouth(float ms) = 0;

    /** \brief Guide east for ms milliseconds
        \return True if OK, false otherwise
    */
    virtual bool GuideEast(float ms) = 0;

    /** \brief Guide west for ms milliseconds
        \return True if OK, false otherwise
    */
    virtual bool GuideWest(float ms) = 0;

protected:

    GuiderInterface();
    ~GuiderInterface();

    /** \brief Initilize guider properties. It is recommended to call this function within initProperties() of your primary device
        \param deviceName Name of the primary device
        \param groupName Group or tab name to be used to define guider properties.
    */
    void initGuiderProperties(const char *deviceName, const char* groupName);

    void processGuiderProperties(const char *name, double values[], char *names[], int n);

    INumber GuideNS[2];
    INumberVectorProperty GuideNSP;
    INumber GuideEW[2];
    INumberVectorProperty GuideEWP;
};

#endif // GUIDERINTERFACE_H
