/*******************************************************************************
 Copyright(c) 2016 CloudMakers, s. r. o.. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
 *******************************************************************************/

#ifndef SCOPESCRIPT_H
#define SCOPESCRIPT_H

#include "indibase/inditelescope.h"
#include "indicontroller.h"

class ScopeScript : public INDI::Telescope
{
  public:
    ScopeScript();
    virtual ~ScopeScript();

    virtual const char *getDefaultName();
    virtual bool initProperties();
    virtual bool saveConfigItems(FILE *fp);

    void ISGetProperties(const char *dev);
    bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n);
    virtual bool Connect();
    virtual bool Disconnect();
    virtual bool Handshake();

  protected:
    virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command);
    virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command);
    virtual bool Abort();

    bool ReadScopeStatus();
    bool Goto(double, double);
    bool Sync(double ra, double dec);
    bool Park();
    bool UnPark();

  private:
    bool RunScript(int script, ...);

    ITextVectorProperty ScriptsTP;
    IText ScriptsT[15];
};

#endif // SCOPESCRIPT_H
