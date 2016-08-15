#if 0
    INDI
    Copyright (C) 2003 Elwood C. Downey

    Complete rewrite of to64frombits() - gives 2x the performance
    of the old implementation (Aug, 2016 by Rumen G.Bogdanovski)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

    Adapted from code written by Eric S. Raymond <esr@snark.thyrsus.com>
#endif

/* Pair of functions to convert to/from base64.
 * Also can be used to build a standalone utility and a loopback test.
 * see http://www.faqs.org/rfcs/rfc3548.html
 */

/** \file base64.c
    \brief Pair of functions to convert to/from base64.

*/

#include <ctype.h>
#include <stdint.h>
#include "base64.h"

static const char base64digits[] =
   "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const char base64lut[] =
"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"
"eAeBeCeDeEeFeGeHeIeJeKeLeMeNeOePeQeReSeTeUeVeWeXeYeZeaebecedeeefegeheiejekelemeneoepeqereseteuevewexeyeze0e1e2e3e4e5e6e7e8e9e+e/"
"fAfBfCfDfEfFfGfHfIfJfKfLfMfNfOfPfQfRfSfTfUfVfWfXfYfZfafbfcfdfefffgfhfifjfkflfmfnfofpfqfrfsftfufvfwfxfyfzf0f1f2f3f4f5f6f7f8f9f+f/"
"gAgBgCgDgEgFgGgHgIgJgKgLgMgNgOgPgQgRgSgTgUgVgWgXgYgZgagbgcgdgegfggghgigjgkglgmgngogpgqgrgsgtgugvgwgxgygzg0g1g2g3g4g5g6g7g8g9g+g/"
"hAhBhChDhEhFhGhHhIhJhKhLhMhNhOhPhQhRhShThUhVhWhXhYhZhahbhchdhehfhghhhihjhkhlhmhnhohphqhrhshthuhvhwhxhyhzh0h1h2h3h4h5h6h7h8h9h+h/"
"iAiBiCiDiEiFiGiHiIiJiKiLiMiNiOiPiQiRiSiTiUiViWiXiYiZiaibicidieifigihiiijikiliminioipiqirisitiuiviwixiyizi0i1i2i3i4i5i6i7i8i9i+i/"
"jAjBjCjDjEjFjGjHjIjJjKjLjMjNjOjPjQjRjSjTjUjVjWjXjYjZjajbjcjdjejfjgjhjijjjkjljmjnjojpjqjrjsjtjujvjwjxjyjzj0j1j2j3j4j5j6j7j8j9j+j/"
"kAkBkCkDkEkFkGkHkIkJkKkLkMkNkOkPkQkRkSkTkUkVkWkXkYkZkakbkckdkekfkgkhkikjkkklkmknkokpkqkrksktkukvkwkxkykzk0k1k2k3k4k5k6k7k8k9k+k/"
"lAlBlClDlElFlGlHlIlJlKlLlMlNlOlPlQlRlSlTlUlVlWlXlYlZlalblcldlelflglhliljlklllmlnlolplqlrlsltlulvlwlxlylzl0l1l2l3l4l5l6l7l8l9l+l/"
"mAmBmCmDmEmFmGmHmImJmKmLmMmNmOmPmQmRmSmTmUmVmWmXmYmZmambmcmdmemfmgmhmimjmkmlmmmnmompmqmrmsmtmumvmwmxmymzm0m1m2m3m4m5m6m7m8m9m+m/"
"nAnBnCnDnEnFnGnHnInJnKnLnMnNnOnPnQnRnSnTnUnVnWnXnYnZnanbncndnenfngnhninjnknlnmnnnonpnqnrnsntnunvnwnxnynzn0n1n2n3n4n5n6n7n8n9n+n/"
"oAoBoCoDoEoFoGoHoIoJoKoLoMoNoOoPoQoRoSoToUoVoWoXoYoZoaobocodoeofogohoiojokolomonooopoqorosotouovowoxoyozo0o1o2o3o4o5o6o7o8o9o+o/"
"pApBpCpDpEpFpGpHpIpJpKpLpMpNpOpPpQpRpSpTpUpVpWpXpYpZpapbpcpdpepfpgphpipjpkplpmpnpopppqprpsptpupvpwpxpypzp0p1p2p3p4p5p6p7p8p9p+p/"
"qAqBqCqDqEqFqGqHqIqJqKqLqMqNqOqPqQqRqSqTqUqVqWqXqYqZqaqbqcqdqeqfqgqhqiqjqkqlqmqnqoqpqqqrqsqtquqvqwqxqyqzq0q1q2q3q4q5q6q7q8q9q+q/"
"rArBrCrDrErFrGrHrIrJrKrLrMrNrOrPrQrRrSrTrUrVrWrXrYrZrarbrcrdrerfrgrhrirjrkrlrmrnrorprqrrrsrtrurvrwrxryrzr0r1r2r3r4r5r6r7r8r9r+r/"
"sAsBsCsDsEsFsGsHsIsJsKsLsMsNsOsPsQsRsSsTsUsVsWsXsYsZsasbscsdsesfsgshsisjskslsmsnsospsqsrssstsusvswsxsyszs0s1s2s3s4s5s6s7s8s9s+s/"
"tAtBtCtDtEtFtGtHtItJtKtLtMtNtOtPtQtRtStTtUtVtWtXtYtZtatbtctdtetftgthtitjtktltmtntotptqtrtstttutvtwtxtytzt0t1t2t3t4t5t6t7t8t9t+t/"
"uAuBuCuDuEuFuGuHuIuJuKuLuMuNuOuPuQuRuSuTuUuVuWuXuYuZuaubucudueufuguhuiujukulumunuoupuqurusutuuuvuwuxuyuzu0u1u2u3u4u5u6u7u8u9u+u/"
"vAvBvCvDvEvFvGvHvIvJvKvLvMvNvOvPvQvRvSvTvUvVvWvXvYvZvavbvcvdvevfvgvhvivjvkvlvmvnvovpvqvrvsvtvuvvvwvxvyvzv0v1v2v3v4v5v6v7v8v9v+v/"
"wAwBwCwDwEwFwGwHwIwJwKwLwMwNwOwPwQwRwSwTwUwVwWwXwYwZwawbwcwdwewfwgwhwiwjwkwlwmwnwowpwqwrwswtwuwvwwwxwywzw0w1w2w3w4w5w6w7w8w9w+w/"
"xAxBxCxDxExFxGxHxIxJxKxLxMxNxOxPxQxRxSxTxUxVxWxXxYxZxaxbxcxdxexfxgxhxixjxkxlxmxnxoxpxqxrxsxtxuxvxwxxxyxzx0x1x2x3x4x5x6x7x8x9x+x/"
"yAyByCyDyEyFyGyHyIyJyKyLyMyNyOyPyQyRySyTyUyVyWyXyYyZyaybycydyeyfygyhyiyjykylymynyoypyqyrysytyuyvywyxyyyzy0y1y2y3y4y5y6y7y8y9y+y/"
"zAzBzCzDzEzFzGzHzIzJzKzLzMzNzOzPzQzRzSzTzUzVzWzXzYzZzazbzczdzezfzgzhzizjzkzlzmznzozpzqzrzsztzuzvzwzxzyzzz0z1z2z3z4z5z6z7z8z9z+z/"
"0A0B0C0D0E0F0G0H0I0J0K0L0M0N0O0P0Q0R0S0T0U0V0W0X0Y0Z0a0b0c0d0e0f0g0h0i0j0k0l0m0n0o0p0q0r0s0t0u0v0w0x0y0z000102030405060708090+0/"
"1A1B1C1D1E1F1G1H1I1J1K1L1M1N1O1P1Q1R1S1T1U1V1W1X1Y1Z1a1b1c1d1e1f1g1h1i1j1k1l1m1n1o1p1q1r1s1t1u1v1w1x1y1z101112131415161718191+1/"
"2A2B2C2D2E2F2G2H2I2J2K2L2M2N2O2P2Q2R2S2T2U2V2W2X2Y2Z2a2b2c2d2e2f2g2h2i2j2k2l2m2n2o2p2q2r2s2t2u2v2w2x2y2z202122232425262728292+2/"
"3A3B3C3D3E3F3G3H3I3J3K3L3M3N3O3P3Q3R3S3T3U3V3W3X3Y3Z3a3b3c3d3e3f3g3h3i3j3k3l3m3n3o3p3q3r3s3t3u3v3w3x3y3z303132333435363738393+3/"
"4A4B4C4D4E4F4G4H4I4J4K4L4M4N4O4P4Q4R4S4T4U4V4W4X4Y4Z4a4b4c4d4e4f4g4h4i4j4k4l4m4n4o4p4q4r4s4t4u4v4w4x4y4z404142434445464748494+4/"
"5A5B5C5D5E5F5G5H5I5J5K5L5M5N5O5P5Q5R5S5T5U5V5W5X5Y5Z5a5b5c5d5e5f5g5h5i5j5k5l5m5n5o5p5q5r5s5t5u5v5w5x5y5z505152535455565758595+5/"
"6A6B6C6D6E6F6G6H6I6J6K6L6M6N6O6P6Q6R6S6T6U6V6W6X6Y6Z6a6b6c6d6e6f6g6h6i6j6k6l6m6n6o6p6q6r6s6t6u6v6w6x6y6z606162636465666768696+6/"
"7A7B7C7D7E7F7G7H7I7J7K7L7M7N7O7P7Q7R7S7T7U7V7W7X7Y7Z7a7b7c7d7e7f7g7h7i7j7k7l7m7n7o7p7q7r7s7t7u7v7w7x7y7z707172737475767778797+7/"
"8A8B8C8D8E8F8G8H8I8J8K8L8M8N8O8P8Q8R8S8T8U8V8W8X8Y8Z8a8b8c8d8e8f8g8h8i8j8k8l8m8n8o8p8q8r8s8t8u8v8w8x8y8z808182838485868788898+8/"
"9A9B9C9D9E9F9G9H9I9J9K9L9M9N9O9P9Q9R9S9T9U9V9W9X9Y9Z9a9b9c9d9e9f9g9h9i9j9k9l9m9n9o9p9q9r9s9t9u9v9w9x9y9z909192939495969798999+9/"
"+A+B+C+D+E+F+G+H+I+J+K+L+M+N+O+P+Q+R+S+T+U+V+W+X+Y+Z+a+b+c+d+e+f+g+h+i+j+k+l+m+n+o+p+q+r+s+t+u+v+w+x+y+z+0+1+2+3+4+5+6+7+8+9+++/"
"/A/B/C/D/E/F/G/H/I/J/K/L/M/N/O/P/Q/R/S/T/U/V/W/X/Y/Z/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q/r/s/t/u/v/w/x/y/z/0/1/2/3/4/5/6/7/8/9/+//"
;

#define BAD     (-1)
static const char base64val[] = {
    BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD,
    BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD,
    BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD, 62, BAD,BAD,BAD, 63,
     52, 53, 54, 55,  56, 57, 58, 59,  60, 61,BAD,BAD, BAD,BAD,BAD,BAD,
    BAD,  0,  1,  2,   3,  4,  5,  6,   7,  8,  9, 10,  11, 12, 13, 14,
     15, 16, 17, 18,  19, 20, 21, 22,  23, 24, 25,BAD, BAD,BAD,BAD,BAD,
    BAD, 26, 27, 28,  29, 30, 31, 32,  33, 34, 35, 36,  37, 38, 39, 40,
     41, 42, 43, 44,  45, 46, 47, 48,  49, 50, 51,BAD, BAD,BAD,BAD,BAD
};
#define DECODE64(c)  (isascii(c) ? base64val[c] : BAD)

/* convert inlen raw bytes at in to base64 string (NUL-terminated) at out. 
 * out size should be at least 4*inlen/3 + 4.
 * return length of out (sans trailing NUL).
 */
int
to64frombits(unsigned char *out, const unsigned char *in, int inlen)
{
	uint16_t* b64lut = (uint16_t*)base64lut;
	int dlen = ((inlen+2)/3)*4; /* 4/3, rounded up */
	uint16_t* wbuf = (uint16_t*)out;

	for(; inlen > 2; inlen -= 3 ) {
		uint32_t n = in[0] << 16 | in[1] << 8 | in[2];

		wbuf[0] = b64lut[ n >> 12 ];
		wbuf[1] = b64lut[ n & 0x00000fff ];

		wbuf += 2;
		in += 3;
	}

	out = (unsigned char*)wbuf;
	if ( inlen > 0 ) {
		int n1 = (*in & 0xfc) >> 2;
		int n2 = (*in & 0x03) << 4;
		if (inlen > 1 ) {
			in++;
			n2 |= (*in & 0xf0) >> 4;
		}
		*out++ = base64digits[n1];
		*out++ = base64digits[n2];
		if (inlen == 2) {  // 2 bytes left to encode
			int n3 = (*in & 0x0f) << 2;
			in++;
			n3 |= (*in & 0xc0) >> 6;
			*out++ = base64digits[n3];
		}
		if (inlen == 1) {  // 1 byte left to encode
			*out++ = '=';
		}
		*out++ = '=';
	}
	*out = 0; // NULL terminate
	return( dlen );
}


/* convert base64 at in to raw bytes out, returning count or <0 on error.
 * base64 may contain any embedded whitespace.
 * out should be at least 3/4 the length of in.
 */
int
from64tobits(char *out, const char *in)
{
    int len = 0;
    register unsigned char digit1, digit2, digit3, digit4;

    do {
	do {digit1 = *in++;} while (isspace(digit1));
        if (DECODE64(digit1) == BAD)
            return(-1);
	do {digit2 = *in++;} while (isspace(digit2));
        if (DECODE64(digit2) == BAD)
            return(-2);
	do {digit3 = *in++;} while (isspace(digit3));
        if (digit3 != '=' && DECODE64(digit3) == BAD)
            return(-3); 
	do {digit4 = *in++;} while (isspace(digit4));
        if (digit4 != '=' && DECODE64(digit4) == BAD)
            return(-4);
        *out++ = (DECODE64(digit1) << 2) | (DECODE64(digit2) >> 4);
        ++len;
        if (digit3 != '=')
        {
            *out++ = ((DECODE64(digit2) << 4) & 0xf0) | (DECODE64(digit3) >> 2);
            ++len;
            if (digit4 != '=')
            {
                *out++ = ((DECODE64(digit3) << 6) & 0xc0) | DECODE64(digit4);
                ++len;
            }
        }
	while (isspace(*in))
	    in++;
    } while (*in && digit4 != '=');

    return (len);
}

#ifdef BASE64_PROGRAM
/* standalone program that converts to/from base64.
 * cc -o base64 -DBASE64_PROGRAM base64.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
usage (char *me)
{
	fprintf (stderr, "Purpose: convert stdin to/from base64 on stdout\n");
	fprintf (stderr, "Usage: %s {-t,-f}\n", me);
	exit (1);
}

int
main (int ac, char *av[])
{
	int to64 = 1;

	/* decide whether to or from base64 */
	if (ac == 2 && strcmp (av[1], "-f") == 0)
	    to64 = 0;
	else if (ac != 1 && (ac != 2 || strcmp (av[1], "-t")))
	    usage (av[0]);

	if (to64) {
	    unsigned char *rawin, *b64;
	    int i, n, nrawin, nb64;

	    /* read raw on stdin until EOF */
	    rawin = malloc(4096);
	    nrawin = 0;
	    while ((n = fread (rawin+nrawin, 1, 4096, stdin)) > 0)
		rawin = realloc (rawin, (nrawin+=n)+4096);

	    /* convert to base64 */
	    b64 = malloc (4*nrawin/3+4);
	    nb64 = to64frombits(b64, rawin, nrawin);

	    /* pretty print */
	    for (i = 0; i < nb64; i += 72)
		printf ("%.*s\n", 72, b64+i);
	} else {
	    unsigned char *raw, *b64;
	    int n, nraw, nb64;

	    /* read base64 on stdin until EOF */
	    b64 = malloc(4096);
	    nb64 = 0;
	    while ((n = fread (b64+nb64, 1, 4096, stdin)) > 0)
		b64 = realloc (b64, (nb64+=n)+4096);
	    b64[nb64] = '\0';

	    /* convert to raw */
	    raw = malloc (3*nb64/4);
	    nraw = from64tobits(raw, b64);
	    if (nraw < 0) {
		fprintf (stderr, "base64 conversion error: %d\n", nraw);
		return (1);
	    }

	    /* write */
	    fwrite (raw, 1, nraw, stdout);
	}

	return (0);
}

#endif

#ifdef LOOPBACK_TEST
/* standalone test that reads binary on stdin, converts to base64 and back,
 * then compares. exit 0 if compares the same else 1
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main (int ac, char *av[])
{
	unsigned char *rawin, *b64, *rawback;
	int n, nrawin, nrawback, nb64;

	/* read raw on stdin until EOF */
	rawin = malloc(4096);
	nrawin = 0;
	while ((n = fread (rawin+nrawin, 1, 4096, stdin)) > 0)
	    rawin = realloc (rawin, (nrawin+=n)+4096);

	/* convert to base64 */
	b64 = malloc (4*nrawin*3 + 4);
	nb64 = to64frombits(b64, rawin, nrawin);

	/* convert back to raw */
	rawback = malloc (3*nb64/4);
	nrawback = from64tobits(rawback, b64);
	if (nrawback < 0) {
	    fprintf (stderr, "base64 error: %d\n", nrawback);
	    return(1);
	}
	if (nrawback != nrawin) {
	    fprintf (stderr, "base64 back length %d != %d\n", nrawback, nrawin);
	    return(1);
	}

	/* compare */
	if (memcmp (rawback, rawin, nrawin)) {
	    fprintf (stderr, "compare error\n");
	    return (1);
	}

	/* success */
	return (0);
}
#endif
/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile$ $Date: 2006-09-30 14:19:41 +0300 (Sat, 30 Sep 2006) $ $Revision: 590506 $ $Name:  $"};
