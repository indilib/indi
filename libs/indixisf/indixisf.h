#include "fitskeyword.h"

#include <vector>
#include <string>
#include "indiccdchip.h"

namespace INDI
{

struct XISFImageParam
{
    int width;
    int height;
    int channelCount;
    int bpp;
    bool compress;
    bool bayer;
    std::string bayerPattern;
    CCDChip::CCD_FRAME frameType;
};

class XISFWrapper
{
public:
    virtual bool writeImage(const XISFImageParam &params, const std::vector<FITSRecord> &fitsKeywords, void *pixelData) = 0;
    virtual const void* fileData() const = 0;
    virtual size_t fileDataSize() const = 0;
    virtual const char* error() const = 0;
    virtual ~XISFWrapper() = default;
};

}

typedef INDI::XISFWrapper* allocXISFWrapperFPTR();
typedef void freeXISFWrapperFPTR(INDI::XISFWrapper *ptr);
