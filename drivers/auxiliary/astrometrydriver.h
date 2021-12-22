/*******************************************************************************
  Copyright(c) 2017 Jasem Mutlaq. All rights reserved.

  INDI Astrometry.net Driver

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

#pragma once

#include "defaultdevice.h"

#include <pthread.h>

/**
 * @brief The AstrometryDriver class is an INDI driver frontend for astrometry.net
 *
 * There are two supported methods to solve an image:
 *
 * 1. Upload an image using the BLOB property.
 * 2. Listen to uploaded BLOBs as emitted from a CCD driver. Set the CCD driver name to listen to in Options.
 *
 * The solver settings should be set before running the solver in order to ensure correct and timely response from astrometry.net
 * It is assumed that astrometry.net is property set-up in the same machine the driver is running along with the appropiate index files.
 *
 * If the solver is successfull, the driver sets the solver results which include:
 * + Pixel Scale (arcsec/pixel).
 * + Orientation (E or W) degrees.
 * + Center RA (J2000) Hours.
 * + Center DE (J2000) Degrees.
 * + Parity
 *
 * @author Jasem Mutlaq
 */
class AstrometryDriver : public INDI::DefaultDevice
{
  public:
    enum
    {
        ASTROMETRY_SETTINGS_BINARY,
        ASTROMETRY_SETTINGS_OPTIONS
    };

    enum
    {
        ASTROMETRY_RESULTS_PIXSCALE,
        ASTROMETRY_RESULTS_ORIENTATION,
        ASTROMETRY_RESULTS_RA,
        ASTROMETRY_RESULTS_DE,
        ASTROMETRY_RESULTS_PARITY
    };

    AstrometryDriver();
    ~AstrometryDriver() = default;

    virtual void ISGetProperties(const char *dev) override;
    virtual bool initProperties() override;
    virtual bool updateProperties() override;

    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                           char *formats[], char *names[], int n) override;
    virtual bool ISSnoopDevice(XMLEle *root) override;

    static void *runSolverHelper(void *context);

  protected:
    //  Generic indi device entries
    bool Connect() override;
    bool Disconnect() override;
    const char *getDefaultName() override;

    virtual bool saveConfigItems(FILE *fp) override;

    // Astrometry

    // Enable/Disable solver
    ISwitch SolverS[2];
    ISwitchVectorProperty SolverSP;
    enum { SOLVER_ENABLE, SOLVER_DISABLE};

    // Solver Settings
    IText SolverSettingsT[2] {};
    ITextVectorProperty SolverSettingsTP;

    // Solver Results
    INumber SolverResultN[5];
    INumberVectorProperty SolverResultNP;

    ITextVectorProperty ActiveDeviceTP;
    IText ActiveDeviceT[1] {};

    IBLOBVectorProperty SolverDataBP;
    IBLOB SolverDataB[1];

    IBLOB CCDDataB[1];
    IBLOBVectorProperty CCDDataBP;

  private:
    // Run solver thread
    void runSolver();

    /**
     * @brief processBLOB Read blob FITS. Uncompress if necessary, write to temporary file, and run
     * solver against it.
     * @param data raw data FITS buffer
     * @param size size of FITS data
     * @param len size of raw data. If no compression is used then len = size. If compression is used,
     * then len is the compressed buffer size and size is the uncompressed final valid data size.
     * @return True if blob buffer was processed correctly and solver started, false otherwise.
     */
    bool processBLOB(uint8_t *data, uint32_t size, uint32_t len);

    // Thread for listenINDI()
    pthread_t solverThread;
    pthread_mutex_t lock;
};
