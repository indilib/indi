// SPDX-FileCopyrightText: 2026 Matteo Beretta
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "plot.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace wheelly
{

namespace
{

// ---------------------------------------------------------------- the font
//
// Five dots by seven, written by rows and not by columns on purpose: that way
// a glyph reads by looking at it, and a wrong one shows without testing it.
// There are only capitals, digits and a few signs: the labels of the plot are
// chosen accordingly, and no accented letters go in them.

struct Glyph
{
    char sign;
    uint8_t row[7];
};

const Glyph ALPHABET[] =
{
    {' ', {0, 0, 0, 0, 0, 0, 0}},
    {'(', {0b00010, 0b00100, 0b01000, 0b01000, 0b01000, 0b00100, 0b00010}},
    {')', {0b01000, 0b00100, 0b00010, 0b00010, 0b00010, 0b00100, 0b01000}},
    {'+', {0b00000, 0b00100, 0b00100, 0b11111, 0b00100, 0b00100, 0b00000}},
    {'-', {0b00000, 0b00000, 0b00000, 0b11111, 0b00000, 0b00000, 0b00000}},
    {'.', {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b01100, 0b01100}},
    {'/', {0b00001, 0b00010, 0b00010, 0b00100, 0b01000, 0b01000, 0b10000}},
    {'0', {0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110}},
    {'1', {0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110}},
    {'2', {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111}},
    {'3', {0b11111, 0b00010, 0b00100, 0b00010, 0b00001, 0b10001, 0b01110}},
    {'4', {0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010}},
    {'5', {0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110}},
    {'6', {0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110}},
    {'7', {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000}},
    {'8', {0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110}},
    {'9', {0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b01100}},
    {':', {0b00000, 0b01100, 0b01100, 0b00000, 0b01100, 0b01100, 0b00000}},
    {'%', {0b11000, 0b11001, 0b00010, 0b00100, 0b01000, 0b10011, 0b00011}},
    {'A', {0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}},
    {'B', {0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110}},
    {'C', {0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110}},
    {'D', {0b11100, 0b10010, 0b10001, 0b10001, 0b10001, 0b10010, 0b11100}},
    {'E', {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111}},
    {'F', {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000}},
    {'G', {0b01110, 0b10001, 0b10000, 0b10111, 0b10001, 0b10001, 0b01111}},
    {'H', {0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}},
    {'I', {0b01110, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110}},
    {'J', {0b00111, 0b00010, 0b00010, 0b00010, 0b00010, 0b10010, 0b01100}},
    {'K', {0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001}},
    {'L', {0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111}},
    {'M', {0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001}},
    {'N', {0b10001, 0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001}},
    {'O', {0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}},
    {'P', {0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000}},
    {'Q', {0b01110, 0b10001, 0b10001, 0b10001, 0b10101, 0b10010, 0b01101}},
    {'R', {0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001}},
    {'S', {0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110}},
    {'T', {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100}},
    {'U', {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}},
    {'V', {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100}},
    {'W', {0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b11011, 0b10001}},
    {'X', {0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001}},
    {'Y', {0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100}},
    {'Z', {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b11111}},
};

const Glyph *glyph(char c)
{
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    for (const Glyph &g : ALPHABET)
        if (g.sign == c) return &g;
    return nullptr;   // what we cannot draw becomes a space
}

// -------------------------------------------------------------- the canvas

// The colours, by name instead of by number: a plot in which the trace and the
// grid get swapped by mistake is a mess that shows only by looking at it.
enum Colour : uint8_t
{
    BACKGROUND = 0, GRID = 1, AXES = 2, TEXT = 3, TRACE = 4, SLOT = 5, DIM = 6
};

const uint8_t PALETTE[][3] =
{
    // Solid black, not a very dark grey: the window KStars opens it in has a
    // WHITE background - it inverts the palette on purpose, see firmware.md -
    // and next to that white an "almost black" background reads as a washed-out
    // smudge instead of as the background of the plot.
    { 0x00, 0x00, 0x00 },   // BACKGROUND
    { 0x2b, 0x31, 0x3a },   // GRID
    { 0x5a, 0x63, 0x70 },   // AXES
    { 0xc8, 0xce, 0xd6 },   // TEXT
    { 0x4a, 0xa3, 0xff },   // TRACE
    { 0xff, 0xb0, 0x00 },   // SLOT: the calibration angles
    { 0x7a, 0x84, 0x90 },   // DIM: the surrounding labels
};
const int COLOUR_COUNT = (int)(sizeof(PALETTE) / sizeof(PALETTE[0]));

class Canvas
{
    public:
        Canvas(int width, int height)
            : m_w(width), m_h(height), m_dots((size_t)width * height, BACKGROUND) {}

        int width() const
        {
            return m_w;
        }
        int height() const
        {
            return m_h;
        }
        const std::vector<uint8_t> &dots() const
        {
            return m_dots;
        }

        void dot(int x, int y, uint8_t colour)
        {
            if (x < 0 || y < 0 || x >= m_w || y >= m_h) return;
            m_dots[(size_t)y * m_w + x] = colour;
        }

        void disc(int x, int y, int radius, uint8_t colour)
        {
            for (int dy = -radius; dy <= radius; dy++)
                for (int dx = -radius; dx <= radius; dx++)
                    if (dx * dx + dy * dy <= radius * radius) dot(x + dx, y + dy, colour);
        }

        // Bresenham, the usual one.
        void line(int x1, int y1, int x2, int y2, uint8_t colour, int dash = 0)
        {
            const int dx = std::abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
            const int dy = -std::abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
            int error = dx + dy, count = 0;
            for (;;)
            {
                if (dash == 0 || (count / dash) % 2 == 0)
                    dot(x1, y1, colour);
                count++;
                if (x1 == x2 && y1 == y2) break;
                const int e2 = 2 * error;
                if (e2 >= dy)
                {
                    error += dy;
                    x1 += sx;
                }
                if (e2 <= dx)
                {
                    error += dx;
                    y1 += sy;
                }
            }
        }

        // Writes starting from the top left corner. `scale` multiplies the
        // dots of the font: at 1 the text is tiny, at 2 it reads from afar.
        void text(int x, int y, const std::string &s, uint8_t colour, int scale = 1)
        {
            int pen = x;
            for (char c : s)
            {
                const Glyph *g = glyph(c);
                if (g != nullptr)
                {
                    for (int row = 0; row < 7; row++)
                        for (int column = 0; column < 5; column++)
                            if (g->row[row] & (1 << (4 - column)))
                                for (int sy = 0; sy < scale; sy++)
                                    for (int sx = 0; sx < scale; sx++)
                                        dot(pen + column * scale + sx,
                                            y + row * scale + sy, colour);
                }
                pen += 6 * scale;
            }
        }

        static int text_width(const std::string &s, int scale = 1)
        {
            return (int)s.size() * 6 * scale;
        }

    private:
        int m_w, m_h;
        std::vector<uint8_t> m_dots;
};

// ------------------------------------------------------------------ the PNG

void be32(std::string &out, uint32_t value)
{
    out += (char)((value >> 24) & 0xff);
    out += (char)((value >> 16) & 0xff);
    out += (char)((value >> 8) & 0xff);
    out += (char)(value & 0xff);
}

uint32_t crc32_of(const std::string &data)
{
    static uint32_t table[256];
    static bool ready = false;
    if (!ready)
    {
        for (uint32_t i = 0; i < 256; i++)
        {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        ready = true;
    }
    uint32_t crc = 0xFFFFFFFFu;
    for (char b : data) crc = table[(crc ^ (uint8_t)b) & 0xff] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

void add_chunk(std::string &out, const char *type, const std::string &data)
{
    be32(out, (uint32_t)data.size());
    const std::string body = std::string(type) + data;
    out += body;
    be32(out, crc32_of(body));
}

// A zlib stream made only of "uncompressed" blocks. It is valid zlib in every
// respect - PNG demands it - and costs some twenty lines instead of a
// dependency. The price is a file as big as the dots it holds: for a
// diagnostic plot asked for once in a while that is perfectly fine.
std::string zlib_uncompressed(const std::string &raw)
{
    std::string z;
    z += (char)0x78;              // CMF: deflate, 32k window
    z += (char)0x01;              // FLG: no dictionary, and (0x7801 % 31) == 0
    size_t i = 0;
    do
    {
        const size_t count = std::min<size_t>(65535, raw.size() - i);
        const bool last = (i + count >= raw.size());
        z += (char)(last ? 1 : 0);
        z += (char)(count & 0xff);
        z += (char)((count >> 8) & 0xff);
        const uint16_t negated = (uint16_t)~(uint16_t)count;
        z += (char)(negated & 0xff);
        z += (char)((negated >> 8) & 0xff);
        z.append(raw, i, count);
        i += count;
    }
    while (i < raw.size());

    uint32_t a = 1, b = 0;
    for (char c : raw)
    {
        a = (a + (uint8_t)c) % 65521;
        b = (b + a) % 65521;
    }
    be32(z, (b << 16) | a);
    return z;
}

std::string to_png(const Canvas &canvas)
{
    std::string out("\x89PNG\r\n\x1a\n", 8);

    std::string ihdr;
    be32(ihdr, (uint32_t)canvas.width());
    be32(ihdr, (uint32_t)canvas.height());
    ihdr += (char)8;    // eight bits per dot
    ihdr += (char)3;    // palette
    ihdr += (char)0;    // compression: deflate, the only one PNG allows
    ihdr += (char)0;    // filters: the standard ones
    ihdr += (char)0;    // no interlacing
    add_chunk(out, "IHDR", ihdr);

    std::string plte;
    for (int i = 0; i < COLOUR_COUNT; i++)
    {
        plte += (char)PALETTE[i][0];
        plte += (char)PALETTE[i][1];
        plte += (char)PALETTE[i][2];
    }
    add_chunk(out, "PLTE", plte);

    std::string raw;
    raw.reserve((size_t)canvas.height() * (canvas.width() + 1));
    for (int y = 0; y < canvas.height(); y++)
    {
        raw += (char)0;      // row filter: none
        raw.append((const char *)&canvas.dots()[(size_t)y * canvas.width()],
                   (size_t)canvas.width());
    }
    add_chunk(out, "IDAT", zlib_uncompressed(raw));
    add_chunk(out, "IEND", std::string());
    return out;
}

std::string whole_number(double v)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.0f", v);
    return buffer;
}

std::string number_1(double v)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.1f", v);
    return buffer;
}

}  // namespace

// ----------------------------------------------------------------- the plot

void plot_window(double minimum, double maximum, double &low, double &high)
{
    const double span = maximum - minimum;
    low = minimum - std::max(span * 0.10, 1.0);
    high = maximum + std::max(span * 0.10, 1.0);
    // A MINIMUM WINDOW. Stretched to the data alone, a
    // sweep of the reference wheel - magnitude 390..424, 34 counts - fills the
    // whole height and reads as a collapse of the field, when it is 8 % of
    // it. The window is at least MIN_WINDOW counts, centred on the data, and
    // never below zero: a small ripple then looks small, and a real drop of
    // hundreds still fills the plot.
    const double MIN_WINDOW = 200.0;
    if (high - low < MIN_WINDOW)
    {
        const double centre = (high + low) / 2.0;
        low = centre - MIN_WINDOW / 2.0;
        high = centre + MIN_WINDOW / 2.0;
    }
    if (low < 0.0)
    {
        high -= low;
        low = 0.0;
    }
}

std::vector<Sample> plot_points(const std::vector<Sample> &samples)
{
    const double SAME_ANGLE_DEG = 0.1;
    std::vector<Sample> sorted = samples;
    std::sort(sorted.begin(), sorted.end(),
              [](const Sample & a, const Sample & b)
    {
        return a.angle < b.angle;
    });
    std::vector<Sample> points;
    size_t in_point = 0;
    for (const Sample &c : sorted)
    {
        if (!points.empty() && c.angle - points.back().angle <= SAME_ANGLE_DEG)
        {
            // running mean; the point keeps the angle of its first sample, so
            // a slow creep does not chain a whole arc into one point
            in_point++;
            points.back().magnitude += (c.magnitude - points.back().magnitude) / (double)in_point;
        }
        else
        {
            points.push_back(c);
            in_point = 1;
        }
    }
    return points;
}


std::string sweep_png(const std::vector<Sample> &samples,
                      const std::vector<double> &slot_angles,
                      const PlotLabels &labels)
{
    const int W = 640, H = 420;
    const int LEFT = 62, RIGHT = 620, TOP = 52, BOTTOM = 350;
    Canvas canvas(W, H);

    // Vertical scale. A ten per cent margin is kept above and below, otherwise
    // the trace scrapes along the edge; and if the samples are all equal - a
    // silent sensor, say - a window is opened anyway, instead of dividing by
    // zero.
    double minimum = 0, maximum = 0;
    bool first = true;
    for (const Sample &c : samples)
    {
        if (first)
        {
            minimum = maximum = c.magnitude;
            first = false;
        }
        minimum = std::min(minimum, c.magnitude);
        maximum = std::max(maximum, c.magnitude);
    }
    const double span = maximum - minimum;
    double low = 0, high = 0;
    plot_window(minimum, maximum, low, high);

    const auto x_of = [&](double angle)
    {
        return LEFT + (int)((RIGHT - LEFT) * (angle / 360.0) + 0.5);
    };
    const auto y_of = [&](double mag)
    {
        return BOTTOM - (int)((BOTTOM - TOP) * ((mag - low) / (high - low)) + 0.5);
    };

    canvas.text(LEFT, 18, labels.title, TEXT, 2);

    // grid: every 45 degrees horizontally, five lines vertically
    for (int g = 0; g <= 360; g += 45)
    {
        const int x = x_of(g);
        canvas.line(x, TOP, x, BOTTOM, GRID);
        const std::string label = whole_number(g);
        canvas.text(x - Canvas::text_width(label) / 2, BOTTOM + 8,
                    label, DIM);
    }
    for (int i = 0; i <= 4; i++)
    {
        const double value = low + (high - low) * i / 4.0;
        const int y = y_of(value);
        canvas.line(LEFT, y, RIGHT, y, GRID);
        const std::string label = whole_number(value);
        canvas.text(LEFT - 8 - Canvas::text_width(label), y - 3,
                    label, DIM);
    }

    canvas.line(LEFT, TOP, LEFT, BOTTOM, AXES);
    canvas.line(LEFT, BOTTOM, RIGHT, BOTTOM, AXES);

    // The calibration angles: that is where the wheel really stops. They are
    // drawn AFTER the axes on purpose: a slot calibrated at zero degrees falls
    // exactly on the vertical axis, and between the data and the frame the
    // data must win.
    for (size_t i = 0; i < slot_angles.size(); i++)
    {
        const int x = x_of(slot_angles[i]);
        canvas.line(x, TOP, x, BOTTOM, SLOT, 3);
        const std::string number = whole_number((double)(i + 1));
        canvas.text(x - Canvas::text_width(number) / 2, TOP - 12, number, SLOT);
    }

    // The trace. The samples are sorted by angle, because the turn can pass
    // through zero and a polyline going backwards would cross the whole plot;
    // and two samples are joined only if they are close, otherwise the jump
    // across the gap reads as data that is not there.
    //
    // Samples taken AT THE SAME ANGLE become one point, at their mean.
    // The wheel rests at every slot for a while, and every
    // 'status' there is a sample at the same angle with the magnitude's own
    // noise: sorted by angle and joined, they would draw vertical lines at
    // the slots, which read as the field jumping where the wheel stands still.
    // "The same angle" is within SAME_ANGLE_DEG, about one AS5600 count.
    const std::vector<Sample> points = plot_points(samples);
    for (size_t i = 0; i < points.size(); i++)
    {
        const int x = x_of(points[i].angle), y = y_of(points[i].magnitude);
        if (i > 0 && points[i].angle - points[i - 1].angle < 25.0)
        {
            canvas.line(x_of(points[i - 1].angle), y_of(points[i - 1].magnitude),
                        x, y, TRACE);
        }
        canvas.disc(x, y, 2, TRACE);
    }

    // the summary, below: the numbers people actually look at
    const double percent = maximum > 0 ? span / maximum * 100.0 : 0.0;
    const std::string line1 = labels.samples + ": " + whole_number((double)samples.size())
                              + "   " + labels.minimum + ": " + whole_number(minimum)
                              + "   " + labels.maximum + ": " + whole_number(maximum);
    const std::string line2 = labels.span + ": " + whole_number(span)
                              + " (" + number_1(percent) + "%)";
    canvas.text(LEFT, BOTTOM + 30, line1, TEXT);
    canvas.text(LEFT, BOTTOM + 46, line2, TEXT, 2);

    // The axis names sit at the ends: the angle's at the bottom right, BELOW
    // the row of tick labels - on the row of tick labels it ended up against
    // the "360" and read "A360LO (GRADI)" - and the magnitude's at the top
    // right,
    // on the title row - not above the axis, where the slot numbers are and
    // they would overlap.
    canvas.text(RIGHT - Canvas::text_width(labels.x_axis), BOTTOM + 30,
                labels.x_axis, DIM);
    canvas.text(RIGHT - Canvas::text_width(labels.y_axis), 22,
                labels.y_axis, DIM);

    return to_png(canvas);
}

}  // namespace wheelly
