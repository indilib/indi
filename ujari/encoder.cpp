#if 0
    INDI Ujari Driver - Encoder
    Copyright (C) 2014 Jasem Mutlaq
    
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

#endif

#include <aiousb.h>

#include "encoder.h"
#include "ujari.h"

using namespace AIOUSB;

// 12bit encoder for BEI H25
#define MAX_ENCODER_COUNT   4096
// Wait 200ms between updates
#define MAX_THREAD_WAIT     200000

#define ENCODER_GROUP "Encoders"

extern int DBG_SCOPE_STATUS;
extern int DBG_COMM;
extern int DBG_MOUNT;

pthread_mutex_t dome_encoder_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ra_encoder_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t de_encoder_mutex = PTHREAD_MUTEX_INITIALIZER;

Encoder::Encoder(encoderType type, Ujari* scope)
{
        // Initially, not connected
        connection_status = -1;

        setType(type);
        telescope = scope;

        debug = false;
        simulation = false;
        verbose    = true;

        startupEncoderValue = 0;
        lastEncoderRaw=0;
        lastEncoderValue=0;

        simSpeed=0;

        direction = EN_NONE;

}

Encoder::~Encoder()
{
}

Encoder::encoderType Encoder::getType() const
{
    return type;
}

void Encoder::setType(const encoderType &value)
{
    type = value;

    switch (type)
    {
        case RA_ENCODER:
        type_name = std::string("RA Encoder");
        SLAVE_ADDRESS = 1;
        break;

        case DEC_ENCODER:
        type_name = std::string("DEC Encoder");
        SLAVE_ADDRESS = 2;
        break;

        case DOME_ENCODER:
        type_name = std::string("Dome Encoder");
        SLAVE_ADDRESS = 3;
        break;

    }

}

/****************************************************************
**
**
*****************************************************************/
bool Encoder::initProperties()
{
    IUFillNumber(&encoderSettingsN[EN_HOME_POSITION], "HOME_POSITION", "Home Position", "%g", 0, 10000000, 1000, 122800);
    IUFillNumber(&encoderSettingsN[EN_HOME_OFFSET], "HOME_OFFSET", "Home Offset", "%g", 0, 10000000, 1000, 0);
    IUFillNumber(&encoderSettingsN[EN_TOTAL], "TOTAL_COUNT", "Total", "%g", 0, 10000000, 1000, 204800);

    IUFillNumber(&encoderValueN[0], "ENCODER_RAW_VALUE", "Value", "%g", 0, 10000000, 1000, 122800);

    switch (type)
    {
        case RA_ENCODER:
         IUFillNumberVector(&encoderSettingsNP, encoderSettingsN, 3, telescope->getDeviceName(), "RA_SETTINGS", "RA Settings", ENCODER_GROUP, IP_RW, 0, IPS_IDLE);
         IUFillNumberVector(&encoderValueNP, encoderValueN, 1, telescope->getDeviceName(), "RA_VALUES", "RA", ENCODER_GROUP, IP_RO, 0, IPS_IDLE);
        break;

        case DEC_ENCODER:
        IUFillNumberVector(&encoderSettingsNP, encoderSettingsN, 3, telescope->getDeviceName(), "DEC_SETTINGS", "DEC Settings", ENCODER_GROUP, IP_RW, 0, IPS_IDLE);
        IUFillNumberVector(&encoderValueNP, encoderValueN, 1, telescope->getDeviceName(), "DEC_VALUES", "DEC", ENCODER_GROUP, IP_RO, 0, IPS_IDLE);
        break;

        case DOME_ENCODER:
        IUFillNumberVector(&encoderSettingsNP, encoderSettingsN, 3, telescope->getDeviceName(), "DOME_SETTINGS", "Dome Settings", ENCODER_GROUP, IP_RW, 0, IPS_IDLE);
        IUFillNumberVector(&encoderValueNP, encoderValueN, 1, telescope->getDeviceName(), "DOME_VALUES", "Dome", ENCODER_GROUP, IP_RO, 0, IPS_IDLE);
        break;

    }

    return true;

}

/****************************************************************
**
**
*****************************************************************/
bool Encoder::connect()
{
    if (simulation)
    {
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_SESSION, "%s: Simulating connecting to DIO-48 USB.", type_name.c_str());
        encoderSettingsN[EN_HOME_POSITION].value = 122800;
        encoderSettingsN[EN_HOME_OFFSET].value = 0;
        encoderSettingsN[EN_TOTAL].value = 409600;
        encoderValueN[0].value = 122800;
        connection_status = 0;
        return true;
    }


    unsigned long result = AIOUSB_Init();
    if( result != AIOUSB_SUCCESS )
    {
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_ERROR, "%s encoder: Can't initialize AIOUSB USB device.", type_name.c_str());
        return false;
    }

    AIOUSB_Reset( 0 );
    AIOUSB_SetCommTimeout( 0, 1000);

    connection_status = 0;

    return true;
}

/****************************************************************
**
**
*****************************************************************/
void Encoder::disconnect()
{
    connection_status = -1;

    if (simulation)
        return;

}

/****************************************************************
**
**
*****************************************************************/
void Encoder::ISGetProperties()
{

}

/****************************************************************
**
**
*****************************************************************/
bool Encoder::updateProperties(bool connected)
{
    if (connected)
    {
        startupEncoderValue = lastEncoderRaw = readEncoder();

        lastEncoderValue = encoderValueN[0].value = GetEncoderZero();

        telescope->defineNumber(&encoderSettingsNP);
        telescope->defineNumber(&encoderValueNP);

        int err = pthread_create(&encoder_thread, NULL, &Encoder::update_helper, this);
        if (err != 0)
        {
            DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_ERROR, "%s encoder: Can't create encoder thread (%s)", type_name.c_str(), strerror(err));
            return false;
        }
    }
    else
    {
        telescope->deleteProperty(encoderSettingsNP.name);
        telescope->deleteProperty(encoderValueNP.name);

        pthread_join(encoder_thread, NULL);
    }
}

/****************************************************************
**
**
*****************************************************************/
bool Encoder::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    if (!strcmp(encoderSettingsNP.name, name))
    {
        if (IUUpdateNumber(&encoderSettingsNP, values, names, n) < 0)
            encoderSettingsNP.s = IPS_ALERT;
        else
            encoderSettingsNP.s = IPS_OK;

        IDSetNumber(&encoderSettingsNP, NULL);

        return true;
    }

    return false;

}

/****************************************************************
**
**
*****************************************************************/
bool Encoder::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{

    return false;
}

/****************************************************************
**
**
*****************************************************************/
unsigned long Encoder::GetEncoder()  throw (UjariError)
{       
    lock_mutex();
    unsigned long value = static_cast<unsigned long>(encoderValueN[0].value);
    unlock_mutex();

    return value;
}

/****************************************************************
** Zero Encoder Value = Home + (StartUpEncoder - Offset)
** If startupEncoder == Offset, then ZeroEncoder=HomeEncoder
** which is the ideal case
*****************************************************************/
unsigned long Encoder::GetEncoderZero()
{
    return static_cast<unsigned long>(encoderSettingsN[EN_HOME_POSITION].value+(((int)startupEncoderValue)-encoderSettingsN[EN_HOME_OFFSET].value));
}

/****************************************************************
**
**
*****************************************************************/
unsigned long Encoder::GetEncoderTotal()
{
    return static_cast<unsigned long>(encoderSettingsN[EN_TOTAL].value);

}

/****************************************************************
**
**
*****************************************************************/
unsigned long Encoder::GetEncoderHome()
{
    return static_cast<unsigned long>(encoderSettingsN[EN_HOME_POSITION].value);

}

/****************************************************************
**
**
*****************************************************************/
void Encoder::setDirection(encoderDirection dir)
{
    lock_mutex();
    direction = dir;
    unlock_mutex();
}

/****************************************************************
**
**
*****************************************************************/
void Encoder::simulateEncoder(double speed)
{
    lock_mutex();
    simSpeed = speed;
    unlock_mutex();
}

/****************************************************************
**
**
*****************************************************************/
void * Encoder::update_helper(void *context)
{
    ((Encoder *)context)->update();
    return 0;
}

/****************************************************************
**
**
*****************************************************************/
bool Encoder::update()
{
    unsigned long encoderRaw=0;

    while (connection_status != -1)
    {
        lock_mutex();

        encoderRaw = readEncoder();

         // No change, let's return
        if (encoderRaw == lastEncoderRaw)
        {
              unlock_mutex();
              usleep(MAX_THREAD_WAIT);              
              continue;
        }


        encoderValueN[0].value += getEncoderDiff(lastEncoderRaw, encoderRaw);

        lastEncoderRaw = encoderRaw;

        if (lastEncoderValue != encoderValueN[0].value)
        {
            IDSetNumber(&encoderValueNP, NULL);
            lastEncoderValue = encoderValueN[0].value;
        }

        unlock_mutex();
        usleep(MAX_THREAD_WAIT);
    }

    return true;
}

/****************************************************************
**
**
*****************************************************************/
bool Encoder::saveConfigItems(FILE *fp)
{
    IUSaveConfigNumber(fp, &encoderSettingsNP);
    return true;
}

/****************************************************************
**
**
*****************************************************************/
unsigned long Encoder::readEncoder()
{
    unsigned long encoderRAW;
    int LSBIndex, MSBIndex, LSB, MSB;
    unsigned char LSBMask, MSBMask;

    if (simulation)
    {
        double speed = simSpeed;
        // Make fast speed faster
        if (speed > 1)
            speed *= 5;

        int absEnc = lastEncoderRaw;
        int deltaencoder=0;
        if (speed > 0)
            deltaencoder = ( (speed * (speed > 0.1 ? 200 : 100)) + rand() % 5) * (direction == EN_CW ? 1 : -1);
        absEnc += deltaencoder;
        if (absEnc >= MAX_ENCODER_COUNT)
            absEnc -= MAX_ENCODER_COUNT;
        else if (absEnc < 0)
            absEnc += MAX_ENCODER_COUNT;

        simSpeed=0;

        return (absEnc);
    }

    /* Each port on the DIO-48 is 8-bits. The encoder use 12bit gray code
       # D000-D011 for Dome Encoder
       # D012-D023 for RA Encoder
       # D024-D035 for DEC Encoder
       * Dome: Ports 0 & 1 --> 8 bits of port 0, first 4 bits of port 1
       * RA  : Ports 1 & 2 --> last 4 bits of port 1, 8 bits of port 2
       * DEC : Ports 3 & 4 --> 8 bits of port 3, first 4 bits of port 4
       *  Masks: Dome LSB 0xFF, MSB 0x0F
       *  Masks: RA LSB 0xF0, MSB 0xFF
       *  Masks: DEC LSB 0xFF, MSB, 0x0F
       */
    switch(type)
    {
        case DOME_ENCODER:
            LSBIndex=0;
            MSBIndex=1;
            LSBMask = 0xFF;
            MSBMask = 0x0F;
            break;
        case RA_ENCODER:
            LSBIndex=1;
            MSBIndex=2;
            LSBMask=0xF0;
            MSBMask=0xFF;
            break;
        case DEC_ENCODER:
            LSBIndex=3;
            MSBIndex=4;
            LSBMask = 0xFF;
            MSBMask = 0x0F;
            break;
    }


    DIO_Read8( 0, LSBIndex, &LSB  );

    LSB &= LSBMask;

    DEBUGFDEVICE(telescope->getDeviceName(), DBG_COMM, "LSB Single data was : hex (0x%X), int(%d)", (int)LSB, (int)LSB );

    DIO_Read8( 0, MSBIndex, &MSB  );

    MSB &= MSBMask;

    DEBUGFDEVICE(telescope->getDeviceName(), DBG_COMM, "MSB Single data was : hex(0x%X) int(%d)", (int)MSB, (int)MSB );

    encoderRAW = ((MSB << 8) | LSB);

    return encoderRAW;

}

/****************************************************************
**
**
*****************************************************************/
int Encoder::getEncoderDiff(unsigned long startEncoder, unsigned long endEncoder)
{
    int diff=0;
    bool directionDetection=false;

    // Try to detect direction
    if (direction == EN_NONE)
    {
        direction = EN_CW;
        int CWEncoder = getEncoderDiff(startEncoder, endEncoder);
        direction = EN_CCW;
        int CCWEncoder = getEncoderDiff(startEncoder, endEncoder);
        if (abs(CWEncoder) < abs(CCWEncoder))
            direction = EN_CW;
        else
            direction = EN_CCW;
        directionDetection = true;
    }

    switch (direction)
    {
        case EN_CW:
        if (endEncoder > startEncoder)
           diff = endEncoder - startEncoder;
        else
           diff = endEncoder + MAX_ENCODER_COUNT - startEncoder;
        break;

    case EN_CCW:
        if (startEncoder > endEncoder)
            diff = startEncoder - endEncoder;
        else
            diff = startEncoder + (MAX_ENCODER_COUNT - endEncoder);
        break;
     default:
        break;
    }

    if (diff > MAX_ENCODER_COUNT)
        diff -= MAX_ENCODER_COUNT;
    else if (diff < 0)
        diff += MAX_ENCODER_COUNT;

    int finalDiff = (direction == EN_CW ? diff : diff * -1);

    if (directionDetection)
        direction = EN_NONE;

    return finalDiff;
}

/****************************************************************
**
**
*****************************************************************/
void Encoder::lock_mutex()
{
    switch (type)
    {
        case DOME_ENCODER:
          pthread_mutex_lock( &dome_encoder_mutex );
          break;

       case RA_ENCODER:
            pthread_mutex_lock( &ra_encoder_mutex );
            break;

        case DEC_ENCODER:
            pthread_mutex_lock( &de_encoder_mutex );
            break;
    }
}

/****************************************************************
**
**
*****************************************************************/
void Encoder::unlock_mutex()
{
    switch (type)
    {
        case DOME_ENCODER:
          pthread_mutex_unlock( &dome_encoder_mutex );
          break;

       case RA_ENCODER:
            pthread_mutex_unlock( &ra_encoder_mutex );
            break;

        case DEC_ENCODER:
            pthread_mutex_unlock( &de_encoder_mutex );
            break;
    }
}
