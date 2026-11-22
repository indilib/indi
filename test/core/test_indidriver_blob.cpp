/*******************************************************************************
 Copyright(c) 2026 Jasem Mutlaq. All rights reserved.

 Regression test for the oneBLOB "enclen" handling in driver dispatch
 (GHSA-j235-2w8x-gp3x, Finding 2).

 In dispatch() the decode length started as the measured pcdata length and was
 then replaced by atoi(enclen) whenever the attribute was present, before
 malloc(3 * bloblen / 4) and from64tobits_fast(..., bloblen). enclen is
 attacker-controlled and was not bounded to the pcdata actually present, so a
 large enclen read past the decoded buffer (out-of-bounds read) and a negative
 one drove malloc through assert_mem()'s exit(1), taking the driver offline
 after the server's restart limit.

 This test registers a writable BLOB property, then feeds dispatch() a
 well-formed newBLOBVector whose oneBLOB carries a hostile enclen, and asserts
 the driver decodes exactly the base64 payload present (ISNewBLOB reports its
 true size) rather than trusting enclen. Built under AddressSanitizer it also
 catches the out-of-bounds access directly.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>
#include <unistd.h>

#include "lilxml.h"
#include "indidevapi.h"
#include "indidriver.h"

// dispatch() is the C entry point exercised by indiserver for each message.
extern "C" int dispatch(XMLEle *root, char msg[]);

// ---- captured ISNewBLOB result -------------------------------------------
static int  g_calls    = 0;
static int  g_n        = 0;
static int  g_blobsize = -1;

extern "C"
{
    void ISGetProperties(const char *) {}
    void ISNewText(const char *, const char *, char *[], char *[], int) {}
    void ISNewNumber(const char *, const char *, double[], char *[], int) {}
    void ISNewSwitch(const char *, const char *, ISState[], char *[], int) {}
    void ISSnoopDevice(XMLEle *) {}
    // Normally in indidrivermain.c (the event-loop/main side we deliberately do
    // not link). Only the IDSet* paths use it; dispatch()'s decode never does.
    void waitPingReply(const char *) {}
    void ISNewBLOB(const char *, const char *, int[], int blobsizes[], char *[],
                   char *[], char *[], int n)
    {
        g_calls++;
        g_n = n;
        g_blobsize = (n > 0) ? blobsizes[0] : -1;
    }
}

static const char *DEV  = "TestDev";
static const char *PROP = "TestBLOB";

// Register one writable BLOB property so dispatch() accepts a newBLOBVector
// for it (rosc_find succeeds and the read-only check passes).
static void defineBlobProperty()
{
    static IBLOB blob;
    static IBLOBVectorProperty bvp;
    IUFillBLOB(&blob, "blob", "blob", ".x");
    IUFillBLOBVector(&bvp, &blob, 1, DEV, PROP, "BLOB", "Main", IP_RW, 60, IPS_IDLE);

    // Silence the defBLOB XML the register call prints to stdout.
    fflush(stdout);
    int saved = dup(1);
    FILE *devnull = fopen("/dev/null", "w");
    if (devnull)
        dup2(fileno(devnull), 1);
    IDDefBLOB(&bvp, nullptr);
    fflush(stdout);
    if (devnull)
    {
        dup2(saved, 1);
        fclose(devnull);
    }
    close(saved);
}

// Send one newBLOBVector with the given enclen attribute and base64 pcdata,
// and return the blob size ISNewBLOB was handed (-1 if never called).
static int dispatchBlob(const std::string &enclenAttr, const char *b64)
{
    std::string msg = std::string("<newBLOBVector device='") + DEV + "' name='" + PROP + "'>"
                      "<oneBLOB name='blob' size='3' format='.x'" + enclenAttr + ">" + b64 +
                      "</oneBLOB></newBLOBVector>";

    LilXML *lp = newLilXML();
    char err[XML_ERROR_SIZE];
    XMLEle **nodes = parseXMLChunk(lp, msg.data(), static_cast<int>(msg.size()), err);
    XMLEle *root = (nodes && nodes[0]) ? nodes[0] : nullptr;

    g_calls = 0;
    g_n = 0;
    g_blobsize = -1;

    char dmsg[2048] = {0};
    if (root)
        dispatch(root, dmsg);

    if (nodes)
    {
        for (int i = 0; nodes[i]; i++)
            delXMLEle(nodes[i]);
        free(nodes);
    }
    delLilXML(lp);
    return g_blobsize;
}

// "QUJD" is base64 for "ABC": exactly 3 decoded bytes from 4 base64 chars.
static const char *B64_ABC = "QUJD";

class DriverBlobDispatch : public ::testing::Test
{
    protected:
        static void SetUpTestSuite()
        {
            defineBlobProperty();
        }
};

TEST_F(DriverBlobDispatch, NoEnclenDecodesPcdata)
{
    EXPECT_EQ(dispatchBlob("", B64_ABC), 3);
    EXPECT_EQ(g_calls, 1);
    EXPECT_EQ(g_n, 1);
}

TEST_F(DriverBlobDispatch, MatchingEnclenDecodesPcdata)
{
    EXPECT_EQ(dispatchBlob(" enclen='4'", B64_ABC), 3);
}

TEST_F(DriverBlobDispatch, OversizedEnclenIsBoundedToPcdata)
{
    // Pre-fix: malloc(3*4000/4) then from64tobits_fast reads 4000 base64 chars
    // from a 4-char buffer (out-of-bounds read) and reports a bogus large size.
    // Post-fix: enclen is ignored because it exceeds the pcdata, size is 3.
    EXPECT_EQ(dispatchBlob(" enclen='4000'", B64_ABC), 3);
}

TEST_F(DriverBlobDispatch, NegativeEnclenIsIgnored)
{
    // Pre-fix: malloc(3 * -100 / 4) -> huge request -> assert_mem -> exit(1).
    // Post-fix: enclen is ignored, size is 3.
    EXPECT_EQ(dispatchBlob(" enclen='-100'", B64_ABC), 3);
}

TEST_F(DriverBlobDispatch, OverflowEnclenIsBoundedToPcdata)
{
    EXPECT_EQ(dispatchBlob(" enclen='2147483647'", B64_ABC), 3);
}
