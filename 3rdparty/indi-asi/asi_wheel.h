/*
 ASI Filter Wheel INDI Driver

 Copyright (c) Rumen G.Bogdanovski
 All Rights Reserved.

 This program is free software; you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by the Free
 Software Foundation; either version 2 of the License, or (at your option)
 any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 more details.

 You should have received a copy of the GNU General Public License along with
 this program; if not, write to the Free Software Foundation, Inc., 59
 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

 The full GNU General Public License is included in this distribution in the
 file called LICENSE.
 */
#ifndef __ASI_WHEEL_H
#define __ASI_WHEEL_H

#define NAME_MAX 100

#include <indifilterwheel.h>

class ASIWHEEL: public INDI::FilterWheel {
	private:
		int fw_id;
		int fw_index;
		int slot_num;

	public:
		ASIWHEEL(int index, EFW_INFO info, bool enumerate);
		~ASIWHEEL();

		void debugTriggered(bool enable);
		void simulationTriggered(bool enable);

		bool Connect();
		bool Disconnect();
		const char *getDefaultName();

		bool initProperties();

		void ISGetProperties(const char *dev);

		int QueryFilter();
		bool SelectFilter(int);
		void TimerHit();
		virtual bool SetFilterNames() { return true; }
		bool GetFilterNames(const char *);

		char name[NAME_MAX];
};

#endif // __ASI_WHEEL_H
