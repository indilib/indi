#include "indixisf.h"
#include <cstring>
#include <QByteArray>
#include "libxisf.h"

namespace INDI
{

class XISFWrapperImpl : public XISFWrapper
{
    QByteArray m_filedata;
    std::string m_error;
public:

bool writeImage(const XISFImageParam &params, const std::vector<FITSRecord> &fitsKeywords, void *pixelData) override
{
    try
    {
        LibXISF::Image image;
        LibXISF::XISFWriter xisfWriter;

        for (auto &keyword : fitsKeywords)
        {
            image.addFITSKeyword({keyword.key().c_str(), keyword.valueString().c_str(), keyword.comment().c_str()});
            QVariant value = keyword.valueString().c_str();
            image.addFITSKeywordAsProperty(keyword.key().c_str(), value);
        }

        image.setGeometry(params.width, params.height, params.channelCount);
        switch(params.bpp)
        {
            case 8:  image.setSampleFormat(LibXISF::Image::UInt8); break;
            case 16: image.setSampleFormat(LibXISF::Image::UInt16); break;
            case 32: image.setSampleFormat(LibXISF::Image::UInt32); break;
            default: m_error = "Unsupported bits per pixel value"; return false;
        }

        switch(params.frameType)
        {
            case CCDChip::LIGHT_FRAME: image.setImageType(LibXISF::Image::Light); break;
            case CCDChip::BIAS_FRAME:  image.setImageType(LibXISF::Image::Bias); break;
            case CCDChip::DARK_FRAME:  image.setImageType(LibXISF::Image::Dark); break;
            case CCDChip::FLAT_FRAME:  image.setImageType(LibXISF::Image::Flat); break;
        }

        if (params.compress)
        {
            image.setCompression(LibXISF::DataBlock::LZ4);
            image.setByteshuffling(params.bpp / 8);
        }

        if (params.bayer)
            image.setColorFilterArray({2, 2, params.bayerPattern.c_str()});

        if (params.channelCount == 3)
        {
            image.setColorSpace(LibXISF::Image::RGB);
        }

        std::memcpy(image.imageData(), pixelData, image.imageDataSize());
        xisfWriter.writeImage(image);

        m_filedata.clear();
        xisfWriter.save(m_filedata);
    }
    catch (LibXISF::Error &error)
    {
        m_error = error.what();
        return false;
    }
    return true;
}

const void* fileData() const override
{
    return (void*)m_filedata.data();
}

size_t fileDataSize() const override
{
    return m_filedata.size();
}

const char* error() const override
{
    return m_error.c_str();
}

};

}

extern "C" INDI::XISFWrapper *allocXISFWrapper()
{
    return new INDI::XISFWrapperImpl;
}

extern "C" void freeXISFWrapper(INDI::XISFWrapper *ptr)
{
    delete ptr;
}
