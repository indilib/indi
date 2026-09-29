// SPDX-FileCopyrightText: 2026 Matteo Beretta
// SPDX-License-Identifier: LGPL-2.1-or-later

// Wheelly - the plot of the magnet sweep, drawn by hand.
//
// Why by hand and not with a library: the driver has no external
// dependencies, and by the criterion written in INDI's CONTRIBUTING that is
// exactly what lets it live in the main repository instead of indi-3rdparty.
// Adding Qt, cairo or libpng to draw a diagnostic plot would be a
// disproportionate price. Everything needed is in here: a palette canvas, a
// 5x7 font, and a PNG writer that does not compress (deflate "stored"
// blocks), so not even zlib.
//
// The PNG ends up in an INDI BLOB. KStars, for a device that is not a camera,
// saves it in the FITS folder and - since the extension is of a format Qt can
// read - opens it in its ImageViewer window. See firmware.md, section 5.1.

#ifndef WHEELLY_PLOT_H
#define WHEELLY_PLOT_H

#include <string>
#include <vector>

namespace wheelly
{

// A sample of the sweep: where the wheel stood, and how strongly the sensor
// saw the magnet at that moment.
struct Sample
{
    double angle;        // degrees, 0..360
    double magnitude;    // AS5600 counts
};

// The labels of the plot, given by the driver: the drawer stays a pure
// function, testable on its own - and the font has only unaccented capitals, so the labels must be chosen
// accordingly.
struct PlotLabels
{
    std::string title;
    std::string x_axis;
    std::string y_axis;
    std::string samples;
    std::string span;
    std::string minimum;
    std::string maximum;
};

// The two decisions of the plot that can be checked without looking at it,
// public for firmware/test/test_plot.cpp:
// the vertical window for data between minimum and maximum - a margin, and
// at least 200 counts, so a ripple of 34 does not read as a collapse;
void plot_window(double minimum, double maximum, double &low, double &high);
// the points the trace joins: sorted by angle, and the samples taken at the
// same angle (the wheel resting at a slot) merged into one, at their mean.
std::vector<Sample> plot_points(const std::vector<Sample> &samples);

// Draws the sweep and returns the bytes of a PNG. `slot_angles` are the
// calibration angles, marked on the plot: that is where the wheel stops, and
// that is where it matters to know whether the magnet is seen well.
std::string sweep_png(const std::vector<Sample> &samples,
                      const std::vector<double> &slot_angles,
                      const PlotLabels &labels);

}  // namespace wheelly

#endif  // WHEELLY_PLOT_H
