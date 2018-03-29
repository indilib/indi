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
#ifndef _UNLOCK_QUEUE_H
#define _UNLOCK_QUEUE_H

class UnlockImageQueue {
public:
    UnlockImageQueue();
    virtual ~UnlockImageQueue();
    
    bool Initialize(int nSize);
    unsigned int Put(const unsigned char *pBuffer, unsigned int nLen);
    unsigned int Get(unsigned char *pBuffer, unsigned int nLen);
    inline void Clean() {
        m_nIn = 0;
        m_nOut = 0;
    }
    inline unsigned int GetDataLen() const { 
    	return  m_nIn - m_nOut; 
    }
    
private:
    inline bool is_power_of_2(unsigned long n) { 
    	return (n != 0 && ((n & (n - 1)) == 0)); 
    };
    
    inline unsigned long roundup_power_of_two(unsigned long val);
private:
    unsigned char *m_pBuffer;	/* the buffer holding the data */
    unsigned int   m_nSize;		/* the size of the allocated buffer */
    unsigned int   m_nIn;		/* data is added at offset (in % size) */
    unsigned int   m_nOut;     	/* data is extracted from offset (out % size) */
};

#endif
