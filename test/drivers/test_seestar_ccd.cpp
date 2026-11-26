/*
    Regression tests for the Seestar CCD driver (and the Alpaca CCD driver it is built on):
    - A frame is saved with 16 bits per pixel. The image code always fills a 16-bit buffer, but the chip stayed at
      its initial 8 bits until a client selected a capture format, so every byte was saved as a pixel.
    - The frame has the camera's size, 1080 x 1920, with every image line intact. The ImageBytes dimensions follow
      the Alpaca specification (Dimension1 = X, Dimension2 = Y, Y fastest); swapping them cut the lines apart.
    - Properties the camera does not implement (Alpaca error 0x400) are not reported as errors.

    The driver talks to a fake Alpaca camera on 127.0.0.1. Its replies are copied from a real Seestar S50 (camera
    driver "Camera V3" 1.3.1-1, Alpaca server 1.3.1-1), including the ImageBytes metadata of a real frame and the
    "not implemented" replies. The pixel values are synthetic: each pixel encodes its own X and Y, so a transposed or
    cut frame shows.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include <httplib.h>
#ifdef _USE_SYSTEM_JSONLIB
#include <nlohmann/json.hpp>
#else
#include <indijson.hpp>
#endif

#include "indilogger.h"
#include "drivers/ccd/indi_seestar_ccd.h"

namespace
{

const char *DEVICE_NAME = "Mock Seestar CCD";
const uint32_t WIDTH  = 1080;   // CameraXSize of the S50
const uint32_t HEIGHT = 1920;   // CameraYSize of the S50

// Synthetic pixel value at (x, y): x in the high byte, y in the low byte.
uint16_t pixel(uint32_t x, uint32_t y)
{
    return static_cast<uint16_t>(((x & 0xFF) << 8) | (y & 0xFF));
}

class FakeSeestarCamera
{
    public:
        FakeSeestarCamera()
        {
            // GET replies of a real S50 camera (device 0) while connected.
            m_Values =
            {
                {"connected", true}, {"cameraxsize", WIDTH}, {"cameraysize", HEIGHT}, {"pixelsizex", 2.9}, {"pixelsizey", 2.9},
                {"sensortype", 2}, {"maxadu", 65535}, {"bayeroffsetx", 1}, {"bayeroffsety", 0}, {"gain", 0}, {"gainmin", 0},
                {"gainmax", 400}, {"gains", nlohmann::json::array()}, {"offset", 0}, {"offsetmin", 0}, {"offsetmax", 0},
                {"exposuremin", 3e-05}, {"exposuremax", 2000}, {"exposureresolution", 1e-06}, {"canabortexposure", true},
                {"canstopexposure", true}, {"canpulseguide", false}, {"cansetccdtemperature", false},
                {"cangetcoolerpower", false}, {"hasshutter", false}, {"canasymmetricbin", false}, {"canfastreadout", false},
                {"maxbinx", 1}, {"maxbiny", 1}, {"binx", 1}, {"biny", 1}, {"startx", 0}, {"starty", 0}, {"numx", WIDTH},
                {"numy", HEIGHT}, {"sensorname", ""}, {"description", "Seestar S50_a544ee7b Telephoto Camera"},
                {"driverinfo", "Camera V3"}, {"driverversion", "1.3.1-1"}, {"name", "Seestar S50_a544ee7b Telephoto Camera"},
                {"interfaceversion", 3}, {"camerastate", 0}, {"readoutmodes", nlohmann::json::array({"0"})},
                {"readoutmode", 0}, {"fullwellcapacity", 52428.8}, {"electronsperadu", 0.8}, {"ccdtemperature", 21.125},
                {"ispulseguiding", false}
            };
            // Error 0x400 replies of the same camera.
            m_NotImplemented =
            {
                {"offsets", "Property read Offsets is not implemented in this driver."},
                {"coolerpower", "Property read CoolerPower get not Implemented is not implemented in this driver."},
                {"cooleron", "Property read CoolerOn is not implemented in this driver."},
                {"fastreadout", "Property read FastReadout is not implemented in this driver."},
                {"percentcompleted", "Property read PercentCompleted get not implemented is not implemented in this driver."},
                {"lastexposureduration", "Property read LastExposureDuration get not implemented is not implemented in this driver."}
            };

            m_Server.Get(R"(/api/v1/camera/0/(\w+))", [this](const httplib::Request & req, httplib::Response & res)
            {
                get(req, res);
            });
            m_Server.Put(R"(/api/v1/camera/0/(\w+))", [this](const httplib::Request & req, httplib::Response & res)
            {
                put(req, res);
            });
            m_Port = m_Server.bind_to_any_port("127.0.0.1");
            m_Thread = std::thread([this]()
            {
                m_Server.listen_after_bind();
            });
            m_Server.wait_until_ready();
        }

        ~FakeSeestarCamera()
        {
            m_Server.stop();
            if (m_Thread.joinable())
                m_Thread.join();
        }

        int port() const
        {
            return m_Port;
        }

    private:
        static nlohmann::json reply(int transaction, int error = 0, const std::string &message = "")
        {
            return {{"ClientTransactionID", transaction}, {"ServerTransactionID", 1}, {"ErrorNumber", error}, {"ErrorMessage", message}};
        }

        static int transaction(const httplib::Request &req)
        {
            return req.has_param("ClientTransactionID") ? std::atoi(req.get_param_value("ClientTransactionID").c_str()) : 0;
        }

        void get(const httplib::Request &req, httplib::Response &res)
        {
            const std::string property = req.matches[1];
            std::lock_guard<std::mutex> lock(m_Mutex);

            if (property == "imagearray")
            {
                imageBytes(req, res);
                return;
            }

            nlohmann::json body = reply(transaction(req));
            if (property == "imageready")
                body["Value"] = m_ImageReady;
            else if (m_NotImplemented.count(property))
            {
                body = reply(transaction(req), 0x400, m_NotImplemented[property]);
                body["Value"] = 0;
            }
            else if (m_Values.count(property))
                body["Value"] = m_Values[property];
            else
                body = reply(transaction(req), 0x400, "Property " + property + " is not implemented in this driver.");
            res.set_content(body.dump(), "application/json; charset=utf-8");
        }

        void put(const httplib::Request &req, httplib::Response &res)
        {
            const std::string property = req.matches[1];
            std::lock_guard<std::mutex> lock(m_Mutex);
            if (property == "startexposure")
                m_ImageReady = true;
            res.set_content(reply(transaction(req)).dump(), "application/json; charset=utf-8");
        }

        // ImageBytes metadata of a real S50 frame: version 1, DataStart 44, ImageElementType Int32 (2),
        // TransmissionElementType UInt16 (8), rank 2, Dimension1 = X = 1080, Dimension2 = Y = 1920.
        void imageBytes(const httplib::Request &req, httplib::Response &res)
        {
            int32_t metadata[11] = {1, 0, transaction(req), 1, 44, 2, 8, 2, static_cast<int32_t>(WIDTH), static_cast<int32_t>(HEIGHT), 0};
            std::string body(reinterpret_cast<const char *>(metadata), sizeof(metadata));
            std::vector<uint16_t> data(WIDTH * HEIGHT);
            // Alpaca order: the last index (Y) varies fastest.
            for (uint32_t x = 0; x < WIDTH; x++)
                for (uint32_t y = 0; y < HEIGHT; y++)
                    data[x * HEIGHT + y] = pixel(x, y);
            body.append(reinterpret_cast<const char *>(data.data()), data.size() * sizeof(uint16_t));
            res.set_content(body, "application/imagebytes");
        }

        httplib::Server m_Server;
        std::thread m_Thread;
        int m_Port {-1};
        std::mutex m_Mutex;
        bool m_ImageReady {false};
        std::map<std::string, nlohmann::json> m_Values;
        std::map<std::string, std::string> m_NotImplemented;
};

class SeestarUnderTest : public SeestarCCD
{
    public:
        SeestarUnderTest()
        {
            setDeviceName(DEVICE_NAME);
            // As at a client's first getProperties; this also runs initProperties().
            ISGetProperties(nullptr);
        }

        bool connectTo(int port, const std::string &uploadDir)
        {
            std::string host = "127.0.0.1", portText = std::to_string(port);
            char hostName[] = "HOST", portName[] = "PORT";
            char *texts[2] = {&host[0], &portText[0]};
            char *names[2] = {hostName, portName};
            ISNewText(getDeviceName(), "SERVER_ADDRESS", texts, names, 2);

            // Save frames to a private folder instead of sending them to a client.
            ISState modes[3] = {ISS_OFF, ISS_ON, ISS_OFF};
            char client[] = "UPLOAD_CLIENT", local[] = "UPLOAD_LOCAL", both[] = "UPLOAD_BOTH";
            char *modeNames[3] = {client, local, both};
            ISNewSwitch(getDeviceName(), "UPLOAD_MODE", modes, modeNames, 3);
            std::string dir = uploadDir, prefix = "frame_XXX";
            char dirName[] = "UPLOAD_DIR", prefixName[] = "UPLOAD_PREFIX";
            char *uploadTexts[2] = {&dir[0], &prefix[0]};
            char *uploadNames[2] = {dirName, prefixName};
            ISNewText(getDeviceName(), "UPLOAD_SETTINGS", uploadTexts, uploadNames, 2);

            ISState connection[2] = {ISS_ON, ISS_OFF};
            char connect[] = "CONNECT", disconnect[] = "DISCONNECT";
            char *connectionNames[2] = {connect, disconnect};
            ISNewSwitch(getDeviceName(), "CONNECTION", connection, connectionNames, 2);
            return isConnected();
        }

        void disconnect()
        {
            ISState connection[2] = {ISS_OFF, ISS_ON};
            char connect[] = "CONNECT", disconnect[] = "DISCONNECT";
            char *connectionNames[2] = {connect, disconnect};
            ISNewSwitch(getDeviceName(), "CONNECTION", connection, connectionNames, 2);
        }

        // Expose as a client does it and wait (at most 10 s) for the frame.
        bool expose(double seconds)
        {
            double values[1] = {seconds};
            char name[] = "CCD_EXPOSURE_VALUE";
            char *names[1] = {name};
            if (!ISNewNumber(getDeviceName(), "CCD_EXPOSURE", values, names, 1))
                return false;
            std::unique_lock<std::mutex> lock(m_FrameMutex);
            return m_FrameDone.wait_for(lock, std::chrono::seconds(10), [this]()
            {
                return m_Complete;
            });
        }

        // What the chip held when the frame was completed.
        int bpp {0};
        int width {0};
        int height {0};
        std::vector<uint16_t> frame;

    protected:
        bool ExposureComplete(INDI::CCDChip *chip) override
        {
            {
                std::lock_guard<std::mutex> lock(m_FrameMutex);
                bpp    = chip->getBPP();
                width  = chip->getSubW();
                height = chip->getSubH();
                const uint16_t *buffer = reinterpret_cast<const uint16_t *>(chip->getFrameBuffer());
                frame.assign(buffer, buffer + static_cast<size_t>(width) * height);
            }
            return SeestarCCD::ExposureComplete(chip);
        }

        // The base class saves the frame in a thread of its own and calls this when it is done, so the FITS file
        // is complete and the thread no longer needs the driver.
        void UploadComplete(INDI::CCDChip *) override
        {
            {
                std::lock_guard<std::mutex> lock(m_FrameMutex);
                m_Complete = true;
            }
            m_FrameDone.notify_all();
        }

    private:
        std::mutex m_FrameMutex;
        std::condition_variable m_FrameDone;
        bool m_Complete {false};
};

// Captures what the driver publishes on stdout.
class Published
{
    public:
        Published()
        {
            fflush(stdout);
            m_File = tmpfile();
            m_SavedStdout = dup(STDOUT_FILENO);
            dup2(fileno(m_File), STDOUT_FILENO);
        }

        ~Published()
        {
            restore();
            if (m_File)
                fclose(m_File);
        }

        // The driver's messages, e.g. "[ERROR] ...".
        std::vector<std::string> messages()
        {
            restore();
            std::string text;
            rewind(m_File);
            char chunk[4096];
            size_t n;
            while ((n = fread(chunk, 1, sizeof(chunk), m_File)) > 0)
                text.append(chunk, n);
            std::vector<std::string> messages;
            std::regex message("message=['\"]([^'\"]*)['\"]");
            for (auto it = std::sregex_iterator(text.begin(), text.end(), message); it != std::sregex_iterator(); ++it)
                messages.push_back((*it)[1]);
            return messages;
        }

    private:
        void restore()
        {
            if (m_SavedStdout < 0)
                return;
            fflush(stdout);
            dup2(m_SavedStdout, STDOUT_FILENO);
            close(m_SavedStdout);
            m_SavedStdout = -1;
        }

        FILE *m_File {nullptr};
        int m_SavedStdout {-1};
};

// Value of a FITS header keyword in the first 2880-byte block.
std::string fitsKeyword(const std::string &path, const std::string &keyword)
{
    std::ifstream file(path, std::ios::binary);
    std::string header(2880, '\0');
    file.read(&header[0], header.size());
    for (size_t i = 0; i + 80 <= header.size(); i += 80)
    {
        std::string card = header.substr(i, 80);
        if (card.compare(0, keyword.size(), keyword) == 0 && card[keyword.size()] == ' ')
        {
            std::string value = card.substr(10, 20);
            value.erase(0, value.find_first_not_of(' '));
            return value.substr(0, value.find(' '));
        }
    }
    return "";
}

std::string makeTempDir()
{
    char dirTemplate[] = "/tmp/test_seestar_ccd_XXXXXX";
    const char *dir = mkdtemp(dirTemplate);
    return dir ? dir : "";
}

void removeDir(const std::string &dir)
{
    std::remove((dir + "/frame_001.fits").c_str());
    std::remove(dir.c_str());
}

}

// A frame from the camera is a 16-bit frame of 1080 x 1920 with intact lines: along every saved row X counts up, and
// Y is the same within the row and counts up from row to row.
TEST(SeestarCCDTest, frameIs16BitWithTheCameraSizeAndIntactLines)
{
    FakeSeestarCamera camera;
    std::string dir = makeTempDir();
    ASSERT_FALSE(dir.empty());
    SeestarUnderTest driver;

    Published published;
    bool connected = driver.connectTo(camera.port(), dir);
    bool exposed = connected && driver.expose(1);
    driver.disconnect();
    published.messages();

    ASSERT_TRUE(connected);
    ASSERT_TRUE(exposed) << "No frame within 10 s";
    EXPECT_EQ(driver.bpp, 16) << "The 16-bit frame buffer was saved as 8-bit";
    EXPECT_EQ(driver.width, static_cast<int>(WIDTH));
    EXPECT_EQ(driver.height, static_cast<int>(HEIGHT));

    const std::string fits = dir + "/frame_001.fits";
    EXPECT_EQ(fitsKeyword(fits, "BITPIX"), "16");
    EXPECT_EQ(fitsKeyword(fits, "NAXIS1"), std::to_string(WIDTH));
    EXPECT_EQ(fitsKeyword(fits, "NAXIS2"), std::to_string(HEIGHT));

    if (driver.frame.size() == static_cast<size_t>(WIDTH) * HEIGHT && driver.width == static_cast<int>(WIDTH))
    {
        int brokenRows = 0;
        for (uint32_t row = 0; row < HEIGHT; row++)
        {
            const uint16_t first = driver.frame[row * WIDTH];
            bool intact = true;
            for (uint32_t column = 0; column < WIDTH && intact; column++)
            {
                const uint16_t value = driver.frame[row * WIDTH + column];
                intact = (value >> 8) == (column & 0xFF) && (value & 0xFF) == (first & 0xFF);
            }
            if (!intact)
                brokenRows++;
        }
        EXPECT_EQ(brokenRows, 0) << "Rows whose X does not count up along the row or whose Y changes within the row";

        // Y of neighbouring rows differs by one (the frame may be stored top-down or bottom-up).
        const int step = ((driver.frame[WIDTH] & 0xFF) - (driver.frame[0] & 0xFF) + 256) % 256;
        EXPECT_TRUE(step == 1 || step == 255) << "Y changes by " << step << " from one row to the next";
    }
    removeDir(dir);
}

// Properties the camera does not implement (Offsets, PercentCompleted, LastExposureDuration, ...) are optional and
// must not show up as errors in the client.
TEST(SeestarCCDTest, notImplementedPropertiesAreNotErrors)
{
    FakeSeestarCamera camera;
    std::string dir = makeTempDir();
    ASSERT_FALSE(dir.empty());
    SeestarUnderTest driver;

    Published published;
    bool connected = driver.connectTo(camera.port(), dir);
    bool exposed = connected && driver.expose(1);
    driver.disconnect();
    std::vector<std::string> messages = published.messages();

    ASSERT_TRUE(connected);
    ASSERT_TRUE(exposed);
    for (const auto &message : messages)
        EXPECT_EQ(message.find("[ERROR]"), std::string::npos) << message;
    removeDir(dir);
}

int main(int argc, char **argv)
{
    // Never touch the user's real ~/.indi.
    char homeTemplate[] = "/tmp/test_seestar_ccd_home_XXXXXX";
    const char *home = mkdtemp(homeTemplate);
    if (home)
        setenv("HOME", home, 1);
    // Never read or write the user's real driver configuration.
    const std::string configFile = "/tmp/test_seestar_ccd_" + std::to_string(getpid()) + "_config.xml";
    setenv("INDICONFIG", configFile.c_str(), 1);
    INDI::Logger::getInstance().configure("", INDI::Logger::file_off,
                                          INDI::Logger::DBG_ERROR, INDI::Logger::DBG_ERROR);
    ::testing::InitGoogleTest(&argc, argv);
    int rc = RUN_ALL_TESTS();
    std::remove(configFile.c_str());
    std::remove((configFile + ".default").c_str());
    if (home)
    {
        std::remove((std::string(home) + "/.indi").c_str());
        std::remove(home);
    }
    return rc;
}
