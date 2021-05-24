/*******************************************************************************
 ScopeDome Dome INDI Driver

 Copyright(c) 2017-2021 Jarno Paananen. All rights reserved.

 based on:

 ScopeDome Windows ASCOM driver version 5.1.30

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

#include "scopedome_sim.h"

bool ScopeDomeSim::detect()
{
    return true;
}

int ScopeDomeSim::updateState()
{
    return 0;
}

uint32_t ScopeDomeSim::getStatus()
{
    return 0;
}

// Abstract versions
ISState ScopeDomeSim::getInputState(AbstractInput input)
{
    (void)input;
    return ISS_OFF;
#if 0
    DigitalIO channel;
    switch(input)
    {
        case HOME:
            channel = IN_HOME;
            break;
        case OPEN1:
            channel = IN_OPEN1;
            break;
        case CLOSED1:
            channel = IN_CLOSED1;
            break;
        case OPEN2:
            channel = IN_OPEN2;
            break;
        case CLOSED2:
            channel = IN_CLOSED1;
            break;
        case ROTARY_LINK:
            channel = IN_ROT_LINK;
            break;
        default:
            LOG_ERROR("invalid input");
            return ISS_OFF;
    }
    return getInputState(channel);
#endif
}

int ScopeDomeSim::setOutputState(AbstractOutput output, ISState onOff)
{
    (void)output;
    (void)onOff;
    return 0;
#if 0
    DigitalIO channel;
    switch(output)
    {
        case RESET:
            channel = OUT_RELAY1;
            break;
        case CW:
            channel = OUT_CW;
            break;
        case CCW:
            channel = OUT_CCW;
            break;
        default:
            LOG_ERROR("invalid output");
            return ISS_OFF;
    }
    return setOutputState(channel, onOff);
#endif
}

void ScopeDomeSim::getFirmwareVersions(double &main, double &rotary)
{
    main = 3.7;
    rotary = 3.7;
}

uint32_t ScopeDomeSim::getStepsPerRevolution()
{
    return STEPS_PER_REVOLUTION;
}

int ScopeDomeSim::getRotationCounter()
{
    return 0;
}

int ScopeDomeSim::getRotationCounterExt()
{
    return 0;
}

bool ScopeDomeSim::isCalibrationNeeded()
{
    return false;
}

void ScopeDomeSim::abort()
{
}

void ScopeDomeSim::calibrate()
{
}

void ScopeDomeSim::findHome()
{
}

void ScopeDomeSim::controlShutter(ShutterOperation operation)
{
    (void) operation;
}

void ScopeDomeSim::resetCounter()
{
}

void ScopeDomeSim::move(int steps)
{
    (void) steps;
}

size_t ScopeDomeSim::getNumberOfSensors()
{
    return 11;
}

ScopeDomeCard::SensorInfo ScopeDomeSim::getSensorInfo(size_t index)
{
    ScopeDomeCard::SensorInfo info;
    switch(index)
    {
        case 0:
            info.propName = "LINK_STRENGTH";
            info.label = "Shutter link strength";
            info.format = "%3.0f";
            info.minValue = 0;
            info.maxValue = 100;
            break;
        case 1:
            info.propName = "SHUTTER_POWER";
            info.label = "Shutter internal power";
            info.format = "%2.2f";
            info.minValue = 0;
            info.maxValue = 100;
            break;
        case 2:
            info.propName = "SHUTTER_BATTERY";
            info.label = "Shutter battery power";
            info.format = "%2.2f";
            info.minValue = 0;
            info.maxValue = 100;
            break;
        case 3:
            info.propName = "CARD_POWER";
            info.label = "Card internal power";
            info.format = "%2.2f";
            info.minValue = 0;
            info.maxValue = 100;
            break;
        case 4:
            info.propName = "CARD_BATTERY";
            info.label = "Card battery power";
            info.format = "%2.2f";
            info.minValue = 0;
            info.maxValue = 100;
            break;
        case 5:
            info.propName = "TEMP_DOME_IN";
            info.label = "Temperature in dome";
            info.format = "%2.2f";
            info.minValue = -100;
            info.maxValue = 100;
            break;
        case 6:
            info.propName = "TEMP_DOME_OUT";
            info.label = "Temperature outside dome";
            info.format = "%2.2f";
            info.minValue = -100;
            info.maxValue = 100;
            break;
        case 7:
            info.propName = "TEMP_DOME_HUMIDITY";
            info.label = "Temperature humidity sensor";
            info.format = "%2.2f";
            info.minValue = -100;
            info.maxValue = 100;
            break;
        case 8:
            info.propName = "HUMIDITY";
            info.label = "Humidity";
            info.format = "%3.2f";
            info.minValue = 0;
            info.maxValue = 100;
            break;
        case 9:
            info.propName = "PRESSURE";
            info.label = "Pressure";
            info.format = "%4.1f";
            info.minValue = 0;
            info.maxValue = 2000;
            break;
        case 10:
            info.propName = "DEW_POINT";
            info.label = "Dew point";
            info.format = "%2.2f";
            info.minValue = -100;
            info.maxValue = 100;
            break;
        default:
            LOG_ERROR("invalid sensor index");
            break;
    }
    return info;
}

double ScopeDomeSim::getSensorValue(size_t index)
{
    double value = 0;

    switch(index)
    {
        case 0:
            break;
        case 1:
            break;
        case 2:
            break;
        case 3:
            break;
        case 4:
            break;
        case 5:
            break;
        case 6:
            break;
        case 7:
            break;
        case 8:
            break;
        case 9:
            break;
        case 10:
            break;
        default:
            LOG_ERROR("invalid sensor index");
            break;
    }
    return value;
}

size_t ScopeDomeSim::getNumberOfRelays()
{
    return 0;
}

ScopeDomeCard::RelayInfo ScopeDomeSim::getRelayInfo(size_t index)
{
    (void) index;
    ScopeDomeCard::RelayInfo info;
    return info;
}

ISState ScopeDomeSim::getRelayState(size_t index)
{
    (void) index;
    return ISS_OFF;
}

void ScopeDomeSim::setRelayState(size_t index, ISState state)
{
    (void) index;
    (void) state;
}

size_t ScopeDomeSim::getNumberOfInputs()
{
    return 0;
}

ScopeDomeCard::InputInfo ScopeDomeSim::getInputInfo(size_t index)
{
    (void) index;
    ScopeDomeCard::InputInfo info;
    return info;
}

ISState ScopeDomeSim::getInputValue(size_t index)
{
    (void) index;
    return ISS_OFF;
}

void ScopeDomeSim::setHomeSensorPolarity(HomeSensorPolarity polarity)
{
    (void)polarity;
}
