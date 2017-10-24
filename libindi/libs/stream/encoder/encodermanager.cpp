/*
    Copyright (C) 2017 by Jasem Mutlaq <mutlaqja@ikarustech.com>

    Encoder Manager

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

#include "encodermanager.h"
#include "raw_encoder.h"

namespace INDI
{

EncoderManager::EncoderManager()
{
    encoder_list.push_back(new RAWEncoder());
    default_encoder = encoder_list.at(0);
}

EncoderManager::~EncoderManager()
{
    std::vector<EncoderManager *>::iterator it;
    for (it = encoder_list.begin(); it != encoder_list.end(); it++)
    {
        delete (*it);
    }
    encoder_list.clear();
}

std::vector<EncoderManager *> EncoderManager::getEncoderList()
{
    return encoder_list;
}

EncoderManager *EncoderManager::getEncoder()
{
    return current_encoder;
}

EncoderManager *EncoderManager::getDefaultRecorder()
{
    return default_encoder;
}

void EncoderManager::setEncoder(EncoderManager *encoder)
{
    current_encoder = encoder;
}

}
