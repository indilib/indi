/*
    Copyright (C) 2014 by geehalel <geehalel@gmail.com>

    V4L2 Record 

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

#include "v4l2_record.h"
#include "ser_recorder.h"

V4L2_Recorder::V4L2_Recorder() {
}

V4L2_Recorder::~V4L2_Recorder() {
}

const char *V4L2_Recorder::getName() {
  return name;
}

V4L2_Record::V4L2_Record() {
  recorder_list.push_back(new SER_Recorder());
  default_recorder=recorder_list.at(0);
}

V4L2_Record::~V4L2_Record() {
  std::vector<V4L2_Recorder *>::iterator it;
  for ( it=recorder_list.begin() ; it != recorder_list.end(); it++ ) {
    delete(*it);
  }
  recorder_list.clear();
}

std::vector<V4L2_Recorder *> V4L2_Record::getRecorderList() {
  return recorder_list;
}

V4L2_Recorder *V4L2_Record::getRecorder() {
  return current_recorder;
}
V4L2_Recorder *V4L2_Record::getDefaultRecorder() {
  return default_recorder;
};
void V4L2_Record::setRecorder(V4L2_Recorder *recorder) {
  current_recorder=recorder;
};
