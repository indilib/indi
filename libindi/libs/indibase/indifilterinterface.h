/*
    Filter Interface
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

#ifndef INDIFILTERINTERFACE_H
#define INDIFILTERINTERFACE_H

#include "indibase.h"

/**
 * \class INDI::FilterInterface
   \brief Provides interface to implement Filter Wheel functionality.

   A filter wheel can be an independent device, or an embedded filter wheel within another device (e.g. CCD camera). Child class must implement all the
   pure virtual functions and call SelectFilterDone(int) when selection of a new filter position is complete in the hardware.

   initFilterProperties() must be called before any other function to initilize the filter properties.
\author Gerry Rozema, Jasem Mutlaq
*/
class INDI::FilterInterface
{

public:

    /** \brief Return current filter position */
    virtual int QueryFilter() = 0;

    /** \brief Select a new filter position
        \return True if operation is successful, false otherwise */
    virtual bool SelectFilter(int position) = 0;

    /** \brief Set filter names as defined by the client for each filter position.
         Filter names should be saved in hardware if possible.
         \return True if successful, false if supported or failed operation */
    virtual bool SetFilterNames() = 0;

    /** \brief Obtains a list of filter names from the hardware and initilizes the FilterNameTP property. The function should check for the number of filters
      available in the filter wheel and build the FilterNameTP property accordingly.
      \return True if successful, false if unsupported or failed operation
      \see QSI CCD implementation of the FilterInterface. QSI CCD is available as a 3rd party INDI driver.
    */
    virtual bool GetFilterNames(const char *deviceName, const char* groupName) = 0;

    /** \brief The child class calls this function when the hardware successfully finished selecting a new filter wheel position
        \param newpos New position of the filter wheel
    */
    void SelectFilterDone(int newpos);


protected:

    FilterInterface();
    ~FilterInterface();

    /** \brief Initilize filter wheel properties. It is recommended to call this function within initProperties() of your primary device
        \param deviceName Name of the primary device
        \param groupName Group or tab name to be used to define filter wheel properties.
    */
    void initFilterProperties(const char *deviceName, const char* groupName);

    INumberVectorProperty *FilterSlotNP;   //  A number vector for filter slot
    INumber FilterSlotN[1];

    ITextVectorProperty *FilterNameTP; //  A text vector that stores out physical port name
    IText *FilterNameT;

    int MinFilter;
    int MaxFilter;
    int CurrentFilter;
    int TargetFilter;
};

#endif // INDIFILTERINTERFACE_H
