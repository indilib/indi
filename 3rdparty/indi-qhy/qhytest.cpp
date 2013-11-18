/*
 * QHY CCD INDI Driver
 *
 * Copyright (c) 2013 CloudMakers, s. r. o. All Rights Reserved.
 *
 * The code is provided by CloudMakers and contributors "AS IS",
 * without warranty of any kind.
 */

#include <iostream>
#include <stdlib.h>
#include <unistd.h>

#include "qhygeneric.h"

using namespace std;

#define W 20
#define H 20

int main() {

	QHYDevice::makeRules();

  cout << endl << "---------------------------------- version " << VERSION_MAJOR << "." << VERSION_MINOR << endl;

  QHYDevice *cameras[10];
	int count = QHYDevice::list(cameras, 10);
	
	for (int i=0; i< count; i++) {
	  QHYDevice *camera = cameras[i];
		cout << "---------------------------------- testing " << camera->getName() << endl;
		if (camera->open()) {
      cout << "is OSC:         " << (camera->isOSC() ? "YES" : "NO") << endl;
      cout << "has cooler:     " << (camera->hasCooler() ? "YES" : "NO") << endl;
      cout << "has shutter:    " << (camera->hasShutter() ? "YES" : "NO") << endl;
      cout << "has guide port: " << (camera->hasGuidePort() ? "YES" : "NO") << endl;
      cout << "---------------------------------- reset " << endl;
		  camera->reset();
      cout << "---------------------------------- get parameters " << endl;
			unsigned pixelCountX, pixelCountY;
			float pixelSizeX, pixelSizeY;
			unsigned bitsPerPixel;
			unsigned maxBinX, maxBinY;
			if (camera->getParameters(&pixelCountX, &pixelCountY, &pixelSizeX, &pixelSizeY, &bitsPerPixel, &maxBinX, &maxBinY)) {
				cout << "pixel count: " << pixelCountX << " x " << pixelCountY << endl;
				cout << "pixel size:  " << pixelSizeX << " x " << pixelSizeY << endl;
				cout << "bits/pixel:  " << bitsPerPixel << endl;
        cout << "max binning: " << maxBinX << " x " << maxBinY << endl;
			} else
				cout << "getParameters() failed!" << endl;
      cout << "---------------------------------- get temperature " << endl;
			float ccdTemp1, ccdTemp2;
			if (camera->getCCDTemp(&ccdTemp1))
				cout << "CCD temp:    " << ccdTemp1 << endl;
			else
				cout << "getCCDTemp() failed!" << endl;
      cout << "---------------------------------- test cooling " << endl;
			if (camera->hasCooler()) {
				cout << "cooler off, fan off..." << endl;
				camera->setCooler(0, false);
				usleep(3*1000*1000);
				cout << "cooler on, fan on..." << endl;
				camera->setCooler(255, true);
				while (ccdTemp2 > ccdTemp1 - 2) {
					usleep(3*1000*1000);
					if (camera->getCCDTemp(&ccdTemp2)) 
						cout << "CCD temp:    " << ccdTemp2 << endl;
					else {
						cout << "getCCDTemp() failed!" << endl;
						break;
					}
				}
				cout << "cooler off, fan off..." << endl;
				camera->setCooler(0, false);
			}
      cout << "---------------------------------- test guiding " << endl;
			if (camera->hasGuidePort()) {
				if (camera->guidePulse(GUIDE_WEST, 100)) {
					cout << "guide west..." << endl;
					usleep(200*1000);
				}
				else
					cout << "guide west failed!" << endl;
				if (camera->guidePulse(GUIDE_EAST, 100)) {
					cout << "guide east..." << endl;
					usleep(200*1000);
				} else
					cout << "guide east failed!" << endl;
				if (camera->guidePulse(GUIDE_NORTH, 100)) {
					cout << "guide north..." << endl;
					usleep(200*1000);
				} else
					cout << "guide north failed!" << endl;
				if (camera->guidePulse(GUIDE_SOUTH, 100)) {
					cout << "guide south..." << endl;
					usleep(200*1000);
				} else
					cout << "guide south failed!" << endl;
			}
      cout << "---------------------------------- set frame parameters " << endl;
			camera->setParameters(0, 0, W, H, 90);
      cout << "---------------------------------- start exposure " << endl;
      camera->StartExposure(1.0);
      cout << "---------------------------------- read image " << endl;
      usleep(1000*1000);
      if (bitsPerPixel == 8) {
        unsigned char pixels[W*H];
        camera->readExposure(pixels);
          for (int j=0; j<H; j++) {
            for (int i=0; i<W; i++)
            cout << (int)pixels[j*W+i] << " ";
          cout << endl;
        }
      } else {
        unsigned short pixels[W*H];
        camera->readExposure(pixels);
          for (int j=0; j<H; j++) {
            for (int i=0; i<W; i++)
            cout << pixels[j*W+i] << " ";
          cout << endl;
        }
      }
      cout << endl;
      cout << "---------------------------------- done " << endl;
			camera->close();
		} else
			cout << "open() failed!" << endl;
	}
	cout << "---------------------------------- test done" << endl;
}
