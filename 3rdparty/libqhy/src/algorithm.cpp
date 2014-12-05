/*
 QHYCCD SDK
 
 Copyright (c) 2014 QHYCCD.
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

#if 0
void __stdcall Bit16To8_Stretch(unsigned char *InputData16,unsigned char *OutputData8,int imageX,int imageY,unsigned short B,unsigned short W)
{
	int ratio;
	int i,j;
	unsigned long s,k;
	int pixel;
	s=0;
	k=0;
	ratio=(W-B)/256;

    if(ratio == 0)
	{
		ratio = 1;
	}

	for(i=0; i < imageY; i++) 
	{
		for(j=0;j<imageX;j++)
		{
			pixel=InputData16[s]+InputData16[s+1]*256;
			if (pixel>B)
			{
				pixel=(pixel-B)/ratio;
				if (pixel>255) pixel=255;
			}

			else    pixel=0;
		
			if (pixel>255) pixel=255;
			OutputData8[k]=pixel;
			s=s+2;
			k=k+1;
		}
	}
}
#endif
