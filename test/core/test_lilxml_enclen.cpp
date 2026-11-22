/*******************************************************************************
 Copyright(c) 2026 Jasem Mutlaq. All rights reserved.

 Regression tests for the oneBLOB "enclen" handling in the lilxml BLOB
 fast path (GHSA-j235-2w8x-gp3x, Finding 1).

 A client-supplied enclen on a <oneBLOB> was trusted without validation: it
 could shrink the pcdata buffer below the payload already held (heap overflow
 in growString), overflow when the base64 newline slack was added, or be
 negative/non-numeric (over-size realloc or uninitialised length, both ending
 in moremem()'s exit(1), which tears down indiserver and its drivers). The
 parser is reachable unauthenticated on the default TCP port 7624.

 These tests feed a <oneBLOB> split across two parseXMLChunk() calls (so the
 second chunk enters the enclen fast path) with hostile enclen values, and
 assert the parser neither crashes nor corrupts/truncates the payload. Built
 under AddressSanitizer they also catch the out-of-bounds access directly.

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
#include <cstring>
#include <string>

#include "lilxml.h"

// Parse a two-chunk oneBLOB message and return the completed root element (or
// nullptr). The caller owns the returned element. The oneBLOB carries the
// supplied enclen attribute and `payload` as its (base64) pcdata, with the
// split placed inside the payload so the second chunk exercises the enclen
// fast path in parseXMLChunk().
static XMLEle *parseSplitBlob(const std::string &enclen, const std::string &payload)
{
    LilXML *lp = newLilXML();
    char err[XML_ERROR_SIZE];

    const std::string head = "<newBLOBVector device='d' name='n'><oneBLOB name='a' size='1' "
                             "format='.x' enclen='" + enclen + "'>";
    const std::string tail = "</oneBLOB></newBLOBVector>";

    const size_t half = payload.size() / 2;
    std::string chunk1 = head + payload.substr(0, half);
    std::string chunk2 = payload.substr(half) + tail;

    XMLEle *root = nullptr;

    // First chunk: nothing completes yet, but it leaves the oneBLOB mid-content.
    XMLEle **nodes = parseXMLChunk(lp, chunk1.data(), static_cast<int>(chunk1.size()), err);
    if (nodes)
    {
        for (int i = 0; nodes[i]; i++)
            delXMLEle(nodes[i]);
        free(nodes);
    }

    // Second chunk: this is where enclen is consulted; the root closes here.
    nodes = parseXMLChunk(lp, chunk2.data(), static_cast<int>(chunk2.size()), err);
    if (nodes)
    {
        for (int i = 0; nodes[i]; i++)
        {
            if (root == nullptr)
                root = nodes[i];      // keep the first completed node for the caller
            else
                delXMLEle(nodes[i]);
        }
        free(nodes);
    }

    delLilXML(lp);
    return root;
}

// After parsing, the oneBLOB pcdata must be exactly the payload we sent -
// not shrunk, truncated, or corrupted - whatever enclen claimed.
static void expectPayloadIntact(const std::string &enclen, const std::string &payload)
{
    XMLEle *root = parseSplitBlob(enclen, payload);
    ASSERT_NE(root, nullptr) << "parser failed to complete for enclen='" << enclen << "'";

    XMLEle *ep = nextXMLEle(root, 1);
    ASSERT_NE(ep, nullptr);
    ASSERT_STREQ(tagXMLEle(ep), "oneBLOB");

    EXPECT_EQ(static_cast<size_t>(pcdatalenXMLEle(ep)), payload.size())
            << "pcdata length wrong for enclen='" << enclen << "'";
    EXPECT_EQ(std::memcmp(pcdataXMLEle(ep), payload.data(), payload.size()), 0)
            << "pcdata corrupted for enclen='" << enclen << "'";

    delXMLEle(root);
}

// A payload large enough that the hostile-enclen realloc lands well below the
// live length, so a shrink corrupts the next growString write.
static std::string bigPayload()
{
    return std::string(40000, 'A');
}

TEST(CORE_LILXML_ENCLEN, ShrinkingEnclenDoesNotCorrupt)
{
    // enclen far smaller than the payload already buffered.
    expectPayloadIntact("4", bigPayload());
}

TEST(CORE_LILXML_ENCLEN, ZeroEnclenDoesNotCorrupt)
{
    expectPayloadIntact("0", bigPayload());
}

TEST(CORE_LILXML_ENCLEN, NegativeEnclenDoesNotAbort)
{
    // Pre-fix this reached moremem() with a near-SIZE_MAX request -> exit(1).
    expectPayloadIntact("-1", bigPayload());
    expectPayloadIntact("-100", bigPayload());
}

TEST(CORE_LILXML_ENCLEN, NonNumericEnclenDoesNotCorrupt)
{
    // Pre-fix blen was left uninitialised by the failed sscanf.
    expectPayloadIntact("abc", bigPayload());
    expectPayloadIntact("", bigPayload());
}

TEST(CORE_LILXML_ENCLEN, OverflowingEnclenDoesNotCorrupt)
{
    // blen += (blen / 72) + 1 overflows int at INT_MAX.
    expectPayloadIntact("2147483647", bigPayload());
}

TEST(CORE_LILXML_ENCLEN, HonestEnclenStillParses)
{
    // A valid, matching enclen (the optimisation path this code exists for)
    // must still complete and deliver the payload intact. Unlike the hostile
    // cases above, this delivers the header in one read and the payload plus
    // closing tags together in the next - the realistic shape for a BLOB that
    // arrives as a single TCP segment after its opening tag, and the shape
    // the enclen fast path is meant to optimise for.
    const int payloadLen = 1000;
    const std::string payload(payloadLen, 'A');
    const std::string head = "<newBLOBVector device='d' name='n'><oneBLOB name='a' size='1' "
                             "format='.x' enclen='" + std::to_string(payloadLen) + "'>";
    const std::string rest = payload + "</oneBLOB></newBLOBVector>";

    LilXML *lp = newLilXML();
    char err[XML_ERROR_SIZE];

    XMLEle **nodes = parseXMLChunk(lp, const_cast<char *>(head.data()),
                                    static_cast<int>(head.size()), err);
    if (nodes)
    {
        for (int i = 0; nodes[i]; i++)
            delXMLEle(nodes[i]);
        free(nodes);
    }

    nodes = parseXMLChunk(lp, const_cast<char *>(rest.data()), static_cast<int>(rest.size()), err);
    ASSERT_NE(nodes, nullptr);
    ASSERT_NE(nodes[0], nullptr) << "parser failed to complete for a valid, matching enclen";

    XMLEle *ep = nextXMLEle(nodes[0], 1);
    ASSERT_NE(ep, nullptr);
    EXPECT_EQ(pcdatalenXMLEle(ep), payloadLen);
    EXPECT_EQ(std::memcmp(pcdataXMLEle(ep), payload.data(), payloadLen), 0);

    for (int i = 0; nodes[i]; i++)
        delXMLEle(nodes[i]);
    free(nodes);
    delLilXML(lp);
}
