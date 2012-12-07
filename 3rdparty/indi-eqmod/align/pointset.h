/* Copyright 2012 Geehalel (geehalel AT gmail DOT com) */
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

#ifndef POINTSET_H
#define POINTSET_H

#include <map>

#include "htm.h"
#include <lilxml.h>

typedef struct AlignData {
  double lst;
  double jd;
  double targetRA, targetDEC;
  double telescopeRA, telescopeDEC;
} AlignData;

class PointSet 
{

 public:
  typedef struct Point {
    HtmID htmID;
    HtmName htmname;
    double celestialALT, celestialAZ, telescopeALT, telescopeAZ;
    AlignData aligndata;
  } Point;

  void AddPoint(AlignData aligndata);
  void Init();
  void Reset();
  char *LoadDataFile(const char *filename);

 protected:
 private:

  XMLEle *PointSetXmlRoot;
  std::map<HtmID, Point> *PointSetMap;
  double lat, lon, alt;
 
  double range24(double r);

};
#endif
