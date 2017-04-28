/*******************************************************************************
  Copyright(c) 2010, 2011 Gerry Rozema. All rights reserved.

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

#ifndef INDI_FILTERWHEEL_H
#define INDI_FILTERWHEEL_H

#include "defaultdevice.h"
#include "indicontroller.h"
#include "indifilterinterface.h"

/**
 * \class INDI::FilterWheel
   \brief Class to provide general functionality of a filter wheel device.

   Developers need to subclass INDI::FilterWheel to implement any driver for filter wheels within INDI.

\author Gerry Rozema, Jasem Mutlaq
\see INDI::FilterInterface
*/
class INDI::FilterWheel: public INDI::DefaultDevice, public INDI::FilterInterface
{
    protected:

        FilterWheel();
        virtual ~FilterWheel();

    public:

        /** \struct FilterConnection
            \brief Holds the connection mode of the Filter.
        */
        enum
        {
            CONNECTION_NONE   = 1 << 0,                 /** Do not use any connection plugin */
            CONNECTION_SERIAL = 1 << 1,                 /** For regular serial and bluetooth connections */
            CONNECTION_TCP    = 1 << 2                  /** For Wired and WiFI connections */
        } FilterConnection;

        virtual bool initProperties();
        virtual bool updateProperties();
        virtual void ISGetProperties (const char * dev);
        virtual bool ISSnoopDevice (XMLEle * root);
        virtual bool ISNewSwitch (const char * dev, const char * name, ISState * states, char * names[], int n);
        virtual bool ISNewNumber (const char * dev, const char * name, double values[], char * names[], int n);
        virtual bool ISNewText (const char * dev, const char * name, char * texts[], char * names[], int n);

        static void joystickHelper(const char * joystick_n, double mag, double angle, void * context);
        static void buttonHelper(const char * button_n, ISState state, void * context);

        /**
         * @brief setFilterConnection Set Filter connection mode. Child class should call this in the constructor before INDI::Filter registers
         * any connection interfaces
         * @param value ORed combination of FilterConnection values.
         */
        void setFilterConnection(const uint8_t &value);

        /**
         * @return Get current Filter connection mode
         */
        uint8_t getFilterConnection() const;

    protected:

        virtual bool saveConfigItems(FILE * fp);
        virtual int QueryFilter();
        virtual bool SelectFilter(int);
        virtual bool SetFilterNames();
        virtual bool GetFilterNames(const char * groupName);

        /** \brief perform handshake with device to check communication */
        virtual bool Handshake();

        void processJoystick(const char * joystick_n, double mag, double angle);
        void processButton(const char * button_n, ISState state);

        INDI::Controller * controller;

        Connection::Serial * serialConnection=NULL;
        Connection::TCP * tcpConnection=NULL;

        // For Serial & TCP connections
        int PortFD=-1;

    private:
        bool callHandshake();
        uint8_t filterConnection = CONNECTION_NONE;

};

#endif // INDI::FilterWheel_H
