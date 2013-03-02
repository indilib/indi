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
#include <set>

#include "htm.h"
#include <lilxml.h>

// to get access to lat/long data
#include <inditelescope.h>

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
  typedef struct Distance {
    HtmID htmID;
    double value;
  } Distance;
  typedef enum PointFilter {
    None, SameQuadrant
  } PointFilter;
  PointSet(INDI::Telescope *);
  void AddPoint(AlignData aligndata, struct ln_lnlat_posn *pos);
  Point *getPoint(HtmID htmid);
  void Init();
  void Reset();
  char *LoadDataFile(const char *filename);
  char *WriteDataFile(const char *filename);
  std::set<Distance, bool (*)(Distance, Distance)> *ComputeDistances(double alt, double az, PointFilter filter);
  double lat, lon, alt;
  double range24(double r);
  double range360(double r);
  void AltAzFromRaDec(double ra, double dec, double lst, double *alt, double *az, struct ln_lnlat_posn *pos);
  void RaDecFromAltAz(double alt, double az, double jd, double *ra, double *dec, struct ln_lnlat_posn *pos) ;
 protected:
 private:

  XMLEle *PointSetXmlRoot;
  std::map<HtmID, Point> *PointSetMap;

  // to get access to lat/long data
  INDI::Telescope *telescope;
  // from align data file
  struct ln_lnlat_posn *lnalignpos; 

};
#endif
