#pragma once
#include <indiccd.h>

// Configuration parameters for the sky renderer.
// All fields have sensible defaults; update via setConfig() when INDI properties change.
struct RenderConfig
{
    int   maxVal        = 65000;    // ADU ceiling
    float limitingMag   = 11.5f;    // magnitude that gives 1 ADU/s at reference aperture
    float saturationMag = 2.0f;     // magnitude that saturates in 1 s
    float seeing        = 3.5f;     // atmospheric FWHM (arcsec)
    float skyGlow       = 19.5f;    // sky background brightness (magnitudes)
};

// SkyRenderer renders a synthetic sky scene into an INDI::CCDChip frame buffer.
// It has no INDI::CCD base class and can be used standalone in tests.
// Both CCDSim and GuideSim own one as a member and delegate DrawCcdFrame work to it.
class SkyRenderer
{
    public:
        SkyRenderer() = default;
        explicit SkyRenderer(const RenderConfig &cfg) : m_Cfg(cfg) {}

        void setConfig(const RenderConfig &cfg)
        {
            m_Cfg = cfg;
        }
        const RenderConfig &getConfig() const
        {
            return m_Cfg;
        }

        int maxPix() const
        {
            return m_MaxPix;
        }
        int minPix() const
        {
            return m_MinPix;
        }

        // Render a complete sky scene into chip's frame buffer.
        // Returns the number of stars drawn, 0 if the field is empty or renderStars is false,
        // or -1 if the GSC catalog tool could not be launched.
        // ra_j2000_deg: field center RA in J2000 degrees [0..360).
        // dec_j2000_deg: field center Dec in J2000 degrees [-90..90].
        // rotation_deg: camera position angle from North (degrees).
        // renderStars: if false, skip GSC catalog lookup and star rendering (use for flat frames).
        // minSearchRadiusArcmin: if > 0, GSC search radius is clamped to at least this value.
        int renderFrame(INDI::CCDChip *chip,
                        double ra_j2000_deg,
                        double dec_j2000_deg,
                        double focal_length_mm,
                        double rotation_deg,
                        float  exposure_s,
                        bool   renderStars = true,
                        double minSearchRadiusArcmin = 0);

        float imageScaleX() const
        {
            return m_ImageScaleX;
        }
        float imageScaleY() const
        {
            return m_ImageScaleY;
        }
        void setImageScale(float scaleX, float scaleY)
        {
            m_ImageScaleX = scaleX;
            m_ImageScaleY = scaleY;
        }

        int drawImageStar(INDI::CCDChip *, float mag, float x, float y, float exp_s);

        void applyReadoutNoise(INDI::CCDChip *chip, int bias, int maxNoise);

    private:
        RenderConfig m_Cfg;
        int   m_MaxPix      {0};
        int   m_MinPix      {65000};
        float m_ImageScaleX {1.0f};
        float m_ImageScaleY {1.0f};

        double flux(double mag) const;
        int    addToPixel(INDI::CCDChip *, int x, int y, int val);
        void   drawSkyGlow(INDI::CCDChip *, float exp_s);
};
