/* Note that most of this file is lifted as-is from suckless st's implementation:
   https://st.suckless.org/

   All credits to Suckless team!
   Original code is MIT licensed. So, is this repo.
*/

#include "unicode.h"
#include <stdlib.h>

typedef unsigned char uchar;

#define UTF_INVALID   0xFFFD
#define UTF_SIZ       4
#define LEN(a)			(sizeof(a) / sizeof(a)[0])
#define BETWEEN(x, a, b)	((a) <= (x) && (x) <= (b))

static const uchar utfbyte[UTF_SIZ + 1] = {0x80,    0, 0xC0, 0xE0, 0xF0};
static const uchar utfmask[UTF_SIZ + 1] = {0xC0, 0x80, 0xE0, 0xF0, 0xF8};
static const Rune utfmin[UTF_SIZ + 1] = {       0,    0,  0x80,  0x800,  0x10000};
static const Rune utfmax[UTF_SIZ + 1] = {0x10FFFF, 0x7F, 0x7FF, 0xFFFF, 0x10FFFF};

size_t
utf8validate(Rune *u, size_t i)
{
	if (!BETWEEN(*u, utfmin[i], utfmax[i]) || BETWEEN(*u, 0xD800, 0xDFFF))
		*u = UTF_INVALID;
	for (i = 1; *u > utfmax[i]; ++i)
		;

	return i;
}

Rune
utf8decodebyte(char c, size_t *i)
{
	for (*i = 0; *i < LEN(utfmask); ++(*i))
		if (((uchar)c & utfmask[*i]) == utfbyte[*i])
			return (uchar)c & ~utfmask[*i];

	return 0;
}

size_t
utf8decode(const char *c, Rune *u, size_t clen)
{
	size_t i, j, len, type;
	Rune udecoded;

	*u = UTF_INVALID;
	if (!clen)
		return 0;
	udecoded = utf8decodebyte(c[0], &len);
	if (!BETWEEN(len, 1, UTF_SIZ))
		return 1;
	for (i = 1, j = 1; i < clen && j < len; ++i, ++j) {
		udecoded = (udecoded << 6) | utf8decodebyte(c[i], &type);
		if (type != 0)
			return j;
	}
	if (j < len)
		return 0;
	*u = udecoded;
	utf8validate(u, len);

	return len;
}

/*
 * Only function here that is actually added by me. All functions above are from st.
 * Simply loops over the bytes and decoded each utf-8 encoded character into ucs4
 * Allocates memory that caller has to free
 */
size_t
utf8_to_ucs4(const char *c, Rune** u, size_t clen)
{
    size_t n, charsize, dd;
    // worst case size
    *u = (Rune *)malloc(clen*sizeof(Rune));
    Rune curr;
    dd = 0;

    for(n = 0; n < clen; n += charsize) {
        charsize = utf8decode(c + n, &curr, clen - n);
        if (charsize == 0)
            break;

        (*u)[dd] = curr;
        dd++;
    }

    return dd;
}
