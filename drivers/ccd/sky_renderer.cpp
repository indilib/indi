#include "sky_renderer.h"
#include "indicom.h"
#include "locale_compat.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

double SkyRenderer::flux(double mag) const
{
    if (m_Cfg.limitingMag == m_Cfg.saturationMag)
        return 1.0;
    double const z = m_Cfg.limitingMag;
    double const k = 2.5 * log10(m_Cfg.maxVal) / (m_Cfg.limitingMag - m_Cfg.saturationMag);
    return pow(10.0, (z - mag) * k / 2.5);
}

int SkyRenderer::addToPixel(INDI::CCDChip *chip, int x, int y, int val)
{
    int const nwidth  = chip->getSubW();
    int const nheight = chip->getSubH();

    x -= chip->getSubX();
    y -= chip->getSubY();

    if (x < 0 || x >= nwidth || y < 0 || y >= nheight)
        return 0;

    auto *pt = reinterpret_cast<uint16_t *>(chip->getFrameBuffer());
    pt += y * nwidth + x;

    int newval = static_cast<int>(pt[0]) + val;
    if (newval > m_Cfg.maxVal)
        newval = m_Cfg.maxVal;
    if (newval > m_MaxPix)
        m_MaxPix = newval;
    if (newval < m_MinPix)
        m_MinPix = newval;
    pt[0] = static_cast<uint16_t>(newval);

    return 1;
}

int SkyRenderer::drawImageStar(INDI::CCDChip *chip, float mag, float x, float y, float exp_s)
{
    int drew = 0;

    int const subX = chip->getSubX();
    int const subY = chip->getSubY();
    int const subW = chip->getSubW() + subX;
    int const subH = chip->getSubH() + subY;

    if (x < subX || x > subW || y < subY || y > subH)
        return 0;

    float const totalFlux = static_cast<float>(flux(mag) * exp_s);

    // Compute position-dependent seeing to simulate sensor tilt.
    // Instead of a global seeing, compute per-star seeing by shifting the
    // effective focus position based on star location on the sensor.
    // This causes each tile's V-curve minimum to land at a different focuser
    // position — which is exactly what the Aberration Inspector detects as tilt.
    float effectiveSeeing = m_Cfg.seeing;
    if (m_Cfg.tiltLR != 0.0f || m_Cfg.tiltTB != 0.0f)
    {
        float const cx = chip->getXRes() / 2.0f;
        float const cy = chip->getYRes() / 2.0f;
        float const dx = (x - cx) / cx;  // normalized [-1, +1]
        float const dy = (y - cy) / cy;
        // tiltLR/tiltTB in arcsec: directly added to seeing at that position.
        // This shifts where the V-curve minimum occurs for edge tiles vs center.
        float const tiltOffset = m_Cfg.tiltLR * dx + m_Cfg.tiltTB * dy;
        effectiveSeeing += tiltOffset;
        if (effectiveSeeing < 0.5f)
            effectiveSeeing = 0.5f;  // Floor to prevent degenerate PSF
    }

    int const boxsizey = static_cast<int>(3.0f * effectiveSeeing / m_ImageScaleY) + 1;

    float const sigma    = effectiveSeeing / (2.0f * std::sqrt(2.0f * std::log(2.0f)));
    float const norm     = 1.0f / (sigma * std::sqrt(2.0f * float(M_PI)));
    float const inv2sig2 = 1.0f / (2.0f * sigma * sigma);

    for (int sy = -boxsizey; sy <= boxsizey; sy++)
    {
        for (int sx = -boxsizey; sx <= boxsizey; sx++)
        {
            float const dc2 = sx * sx * m_ImageScaleX * m_ImageScaleX
                              + sy * sy * m_ImageScaleY * m_ImageScaleY;
            float const fa  = norm * std::exp(-dc2 * inv2sig2);
            int const fp = static_cast<int>(fa * totalFlux);
            if (fp > 0)
            {
                if (addToPixel(chip,
                               static_cast<int>(x) + sx,
                               static_cast<int>(y) + sy, fp) != 0)
                    drew = 1;
            }
        }
    }
    return drew;
}

void SkyRenderer::applyReadoutNoise(INDI::CCDChip *chip, int bias, int maxNoise)
{
    if (maxNoise <= 0)
        return;

    int const nx = chip->getSubW();
    int const ny = chip->getSubH();
    auto *buf = reinterpret_cast<uint16_t *>(chip->getFrameBuffer());

    for (int y = 0; y < ny; y++)
        for (int x = 0; x < nx; x++)
        {
            int newval = static_cast<int>(buf[y * nx + x]) + bias + (random() % maxNoise);
            if (newval > m_Cfg.maxVal)
                newval = m_Cfg.maxVal;
            buf[y * nx + x] = static_cast<uint16_t>(newval);
        }
}

void SkyRenderer::drawSkyGlow(INDI::CCDChip *chip, float exp_s)
{
    float const skyflux = static_cast<float>(flux(m_Cfg.skyGlow)) * exp_s;

    int const nwidth  = chip->getSubW();
    int const nheight = chip->getSubH();

    auto *pt = reinterpret_cast<uint16_t *>(chip->getFrameBuffer());

    float const vig = std::min(nwidth, nheight) * m_ImageScaleX;
    float const invVig2 = 1.0f / (vig * vig);

    for (int y = 0; y < nheight; y++)
    {
        float const sy = nheight / 2.0f - y;

        for (int x = 0; x < nwidth; x++)
        {
            float const sx = nwidth / 2.0f - x;

            float const dc2 = sx * sx * m_ImageScaleX * m_ImageScaleX
                              + sy * sy * m_ImageScaleY * m_ImageScaleY;

            float const fa = std::exp(-2.0f * 0.7f * dc2 * invVig2);

            float fp = (pt[0] + skyflux) * fa;

            if (fp > m_Cfg.maxVal) fp = static_cast<float>(m_Cfg.maxVal);
            if (fp < pt[0]) fp = pt[0]; // never darken an already-bright pixel
            if (fp > m_MaxPix) m_MaxPix = static_cast<int>(fp);
            if (fp < m_MinPix) m_MinPix = static_cast<int>(fp);

            pt[0] = static_cast<uint16_t>(fp);
            pt++;
        }
    }
}

int SkyRenderer::renderFrame(
    INDI::CCDChip *chip,
    double ra_j2000_deg,
    double dec_j2000_deg,
    double focal_length_mm,
    double rotation_deg,
    float  exposure_s,
    bool   renderStars,
    double minSearchRadiusArcmin)
{
    m_MaxPix = 0;
    m_MinPix = 65000;

    // Image scale (arcsec/pixel) from focal length and pixel size
    m_ImageScaleX = static_cast<float>((chip->getPixelSizeX() / focal_length_mm) * 206.3);
    m_ImageScaleY = static_cast<float>((chip->getPixelSizeY() / focal_length_mm) * 206.3);

    // Plate matrix: maps standard coords to pixel coords, with camera rotation applied
    double const theta = rotation_deg * (M_PI / 180.0);
    double const pprx  = focal_length_mm / chip->getPixelSizeX() * 1000.0; // pixels per radian, X
    double const ppry  = focal_length_mm / chip->getPixelSizeY() * 1000.0; // pixels per radian, Y

    double const pa =  pprx * std::cos(theta);
    double const pb =  ppry * std::sin(theta);
    double const pd = -pprx * std::sin(theta);
    double const pe =  ppry * std::cos(theta);
    double const pc =  chip->getXRes() / 2.0;  // X center pixel
    double const pf =  chip->getYRes() / 2.0;  // Y center pixel
    double const ccdW = chip->getXRes();

    // Field center in radians
    double const rar  = ra_j2000_deg  * (M_PI / 180.0);
    double const decr = dec_j2000_deg * (M_PI / 180.0);

    // GSC search radius (arcmin): half-diagonal of the chip in arcsec, converted to arcmin
    double radius = std::sqrt(
                        m_ImageScaleX * m_ImageScaleX * chip->getXRes() / 2.0 * chip->getXRes() / 2.0 +
                        m_ImageScaleY * m_ImageScaleY * chip->getYRes() / 2.0 * chip->getYRes() / 2.0) / 60.0;
    if (minSearchRadiusArcmin > 0 && radius < minSearchRadiusArcmin)
        radius = minSearchRadiusArcmin;

    // Cap lookup magnitude for very wide fields to limit GSC processing time
    double lookuplimit = m_Cfg.limitingMag;
    if (radius > 60)
        lookuplimit = 11;

    // Clear the frame buffer
    memset(chip->getFrameBuffer(), 0, chip->getFrameBufferSize());

    int drawn = 0;

    if (renderStars)
    {
        AutoCNumeric locale;
        char gsccmd[250];

        // Handbook of astronomical image processing, eqs. 9.1/9.2 (gnomonic projection)
        snprintf(gsccmd, sizeof(gsccmd),
                 "gsc -c %8.6f %+8.6f -r %4.1f -m 0 %4.2f -n 3000",
                 range360(ra_j2000_deg),
                 rangeDec(dec_j2000_deg),
                 radius,
                 lookuplimit);

        FILE *pp = popen(gsccmd, "r");
        if (pp != nullptr)
        {
            char line[256];

            while (fgets(line, sizeof(line), pp) != nullptr)
            {
                char  id[20], plate[6], ob[6];
                float ra, dec, pose, mag, mage, dist;
                int   band, c, dir;

                int rc = sscanf(line, "%10s %f %f %f %f %f %d %d %4s %2s %f %d",
                                id, &ra, &dec, &pose, &mag, &mage,
                                &band, &c, plate, ob, &dist, &dir);
                if (rc != 12)
                    continue;

                double const srar  = ra  * (M_PI / 180.0);
                double const sdecr = dec * (M_PI / 180.0);

                double const denom = cos(decr) * cos(sdecr) * cos(srar - rar)
                                     + sin(decr) * sin(sdecr);
                if (denom <= 0)
                    continue;

                double const sx = cos(sdecr) * sin(srar - rar) / denom;
                double const sy = (sin(decr) * cos(sdecr) * cos(srar - rar)
                                   - cos(decr) * sin(sdecr)) / denom;

                // Invert horizontally (CW->CCW, origin North)
                double const ccdx = ccdW - (pa * sx + pb * sy + pc);
                double const ccdy =          pd * sx + pe * sy + pf;

                drawn += drawImageStar(chip, mag,
                                       static_cast<float>(ccdx),
                                       static_cast<float>(ccdy),
                                       exposure_s);
            }
            pclose(pp);
        }
        else
        {
            drawn = -1;
        }
    }

    drawSkyGlow(chip, exposure_s);

    return drawn;
}
