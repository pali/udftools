/*
 * unicode.c
 *
 * Copyright (c) 2001-2002  Ben Fennema
 * Copyright (c) 2014-2017  Pali Rohár <pali.rohar@gmail.com>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

/**
 * @file
 * libudffs unicode handling functions
 */

#include "config.h"

#include "libudffs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <wchar.h>
#include <errno.h>

size_t decode_utf8(const dchars *in, char *out, size_t inlen, size_t outlen)
{
	size_t len = 0, i;
	unsigned int c;

	if (outlen == 0)
		return (size_t)-1;

	if (in[0] != 8 && in[0] != 16)
		return (size_t)-1;

	if (in[0] == 16 && (inlen-1) % 2 != 0)
		return (size_t)-1;

	for (i=1; i<inlen;)
	{
		c = in[i++];
		if (in[0] == 16)
			c = (c << 8) | in[i++];

		if (c == 0x00U)
			break;
		else if (c < 0x80U)
		{
			if (len+1 >= outlen)
				return (size_t)-1;
			out[len++] = (uint8_t)c;
		}
		else if (c < 0x800U)
		{
			if (len+2 >= outlen)
				return (size_t)-1;
			out[len++] = (uint8_t)(0xc0 | (c >> 6));
			out[len++] = (uint8_t)(0x80 | (c & 0x3f));
		}
		else
		{
			if (len+3 >= outlen)
				return (size_t)-1;
			out[len++] = (uint8_t)(0xe0 | (c >> 12));
			out[len++] = (uint8_t)(0x80 | ((c >> 6) & 0x3f));
			out[len++] = (uint8_t)(0x80 | (c & 0x3f));
		}
	}

	out[len] = 0;
	return len;
}

size_t encode_utf8(dchars *out, const char *in, size_t outlen)
{
	size_t inlen = strlen(in);
	size_t len, i;
	int utf_cnt;
	uint32_t utf_char, max_val;
	unsigned int c;

	len = 1;
	out[0] = 8;
	max_val = 0x7F;

try_again:
	utf_cnt = 0;
	utf_char = 0;

	for (i=0; i<inlen; i++)
	{
		c = in[i];

		/* Complete a multi-byte UTF-8 character */
		if (utf_cnt)
		{
			if ((c & 0xC0) != 0x80)
				goto error_invalid;
			utf_char = (utf_char << 6) | (c & 0x3F);
			if (--utf_cnt)
				continue;
		}
		else
		{
			/* Check for a multi-byte UTF-8 character */
			if (c & 0x80)
			{
				/* Start a multi-byte UTF-8 character */
				if ((c & 0xE0) == 0xC0)
				{
					utf_char = c & 0x1fU;
					utf_cnt = 1;
				}
				else if ((c & 0xF0) == 0xE0)
				{
					utf_char = c & 0x0F;
					utf_cnt = 2;
				}
				else if ((c & 0xF8) == 0xF0)
				{
					utf_char = c & 0x07;
					utf_cnt = 3;
				}
				else if ((c & 0xFC) == 0xF8)
				{
					utf_char = c & 0x03;
					utf_cnt = 4;
				}
				else if ((c & 0xFE) == 0xfc)
				{
					utf_char = c & 0x01;
					utf_cnt = 5;
				}
				else
					goto error_invalid;
				continue;
			}
			else
			{
				/* Single byte UTF-8 character (most common) */
				utf_char = c;
			}
		}

		/* Choose no compression if necessary */
		if (utf_char > max_val)
		{
			if (max_val == 0x7F)
			{
				len = 1;
				max_val = 0xFFFF;
				out[0] = 0x10;
				goto try_again;
			}
			return (size_t)-1;
		}

		if (max_val == 0xFFFF)
		{
			if (len + 2 > outlen)
			{
				len = 1;
				max_val = 0xFF;
				out[0] = 0x8;
				goto try_again;
			}
			out[len++] = utf_char >> 8;
		}
		if (len + 1 > outlen)
			return (size_t)-1;
		out[len++] = utf_char & 0xFF;
	}

	if (utf_cnt)
	{
error_invalid:
		fprintf(stderr, "%s: Error: Cannot convert input string from UTF-8 encoding: Invalid or incomplete UTF-8 sequence\n", appname);
		exit(1);
	}

	return len;
}

size_t decode_locale(const dchars *in, char *out, size_t inlen, size_t outlen)
{
	size_t len = 0, i;
	size_t wcslen, clen;
	wchar_t *wcs;
	mbstate_t ps;
	char cbuf[MB_CUR_MAX];

	if (outlen == 0)
		return (size_t)-1;

	if (in[0] == 16 && (inlen-1) % 2 != 0)
		return (size_t)-1;

	if (in[0] == 8)
		wcslen = (inlen-1);
	else if (in[0] == 16)
		wcslen = (inlen-1)/2;
	else
		return (size_t)-1;

	wcs = calloc(wcslen+1, sizeof(wchar_t));
	if (!wcs)
		return (size_t)-1;

	for (i=1; i<inlen;)
	{
		wcs[len] = in[i++];
		if (in[0] == 16)
			wcs[len] = (wcs[len] << 8) | in[i++];
		++len;
	}

	memset(&ps, 0, sizeof(ps));

	len = 0;
	for (i=0; wcs[i]; ++i)
	{
		clen = wcrtomb(cbuf, wcs[i], &ps);
		if (clen == (size_t)-1)
		{
			if (errno == EILSEQ)
			{
				cbuf[0] = '?';
				clen = 1;
			}
			else
			{
				fprintf(stderr, "%s: Error: Cannot convert output string to current locale encoding: %s\n", appname, strerror(errno));
				free(wcs);
				exit(1);
			}
		}
		if (len+clen+1 > outlen)
		{
			free(wcs);
			return (size_t)-1;
		}
		memcpy(out+len, cbuf, clen);
		len += clen;
	}
	out[len] = 0;

	free(wcs);
	return len;
}

size_t encode_locale(dchars *out, const char *in, size_t outlen)
{
	size_t i;
	size_t mbslen;
	size_t len;
	wchar_t max_val;
	wchar_t *wcs;

	mbslen = mbstowcs(NULL, in, 0);
	if (mbslen == (size_t)-1)
	{
		fprintf(stderr, "%s: Error: Cannot convert input string from current locale encoding: %s\n", appname, strerror(errno));
		exit(1);
	}

	wcs = calloc(mbslen+1, sizeof(wchar_t));
	if (!wcs)
		goto error_out;

	if (mbstowcs(wcs, in, mbslen+1) == (size_t)-1)
		goto error_out;

	len = 1;
	out[0] = 8;
	max_val = 0x7F;

try_again:
	for (i=0; i<mbslen; ++i)
	{
		if (wcs[i] > max_val)
		{
			if (max_val == 0x7F)
			{
				len = 1;
				out[0] = 16;
				max_val = 0xFFFF;
				goto try_again;
			}
			goto error_out;
		}

		if (max_val == 0xFFFF)
		{
			if (len+2 > outlen)
			{
				len = 1;
				out[0] = 8;
				max_val = 0xFF;
				goto try_again;
			}
			out[len++] = (wcs[i] >> 8) & 0xFF;
		}

		if (len+1 > outlen)
			goto error_out;
		out[len++] = wcs[i] & 0xFF;
	}

	free(wcs);
	return len;

error_out:
	free(wcs);
	return (size_t)-1;
}

size_t decode_string(struct udf_disc *disc, const dstring *in, char *out, size_t inlen, size_t outlen)
{
	uint32_t flags = disc ? disc->flags : FLAG_LOCALE;
	if (in[0] == 0 && outlen)
	{
		out[0] = 0;
		return 0;
	}
	if (in[inlen-1] == 0 || in[inlen-1] >= inlen)
		return (size_t)-1;
	inlen = in[inlen-1];
	if (flags & FLAG_UTF8)
		return decode_utf8((dchars *)in, out, inlen, outlen);
	else if (flags & FLAG_LOCALE)
		return decode_locale((dchars *)in, out, inlen, outlen);
	else if (flags & (FLAG_UNICODE8 | FLAG_UNICODE16))
	{
		size_t i;
		if (in[0] != 8 && in[0] != 16)
			return (size_t)-1;
		if (in[0] == 16 && (inlen-1) % 2 != 0)
			return (size_t)-1;
		if ((in[0] == 8 && (flags & FLAG_UNICODE8)) || (in[0] == 16 && (flags & FLAG_UNICODE16)))
		{
			if (inlen > outlen)
				return (size_t)-1;
			memcpy(out, &in[1], inlen);
			if (in[0] == 0x10 && (flags & FLAG_UNICODE16))
			{
				if (inlen+1 > outlen)
					return (size_t)-1;
				out[inlen] = 0;
				return inlen+1;
			}
			return inlen;
		}
		else if (flags & FLAG_UNICODE16)
		{
			if (2*(inlen-1)+2 > outlen)
				return (size_t)-1;
			for (i=1; i<inlen; ++i)
			{
				out[2*(i-1)] = 0;
				out[2*(i-1)+1] = in[i];
			}
			out[2*(inlen-1)] = 0;
			out[2*(inlen-1)+1] = 0;
			return 2*(inlen-1);
		}
		else if (flags & FLAG_UNICODE8)
		{
			if ((inlen-1)/2+1 > outlen)
				return (size_t)-1;
			for (i=1; i<inlen; i+=2)
			{
				if (in[i])
					return (size_t)-1;
				out[i/2] = in[i+1];
			}
			out[(inlen-1)/2] = 0;
			return (inlen-1)/2;
		}
		else
			return (size_t)-1;
	}
	else
		return (size_t)-1;
}

size_t encode_string(struct udf_disc *disc, dstring *out, const char *in, size_t outlen)
{
	uint32_t flags = disc ? disc->flags : FLAG_LOCALE;
	size_t ret = (size_t)-1;
	if (outlen == 0)
		return (size_t)-1;
	if (in[0] == 0)
	{
		memset(out, 0, outlen);
		return 0;
	}
	if (flags & FLAG_UTF8)
		ret = encode_utf8((dchars *)out, in, outlen-1);
	else if (flags & FLAG_LOCALE)
		ret = encode_locale((dchars *)out, in, outlen-1);
	else if (flags & (FLAG_UNICODE8|FLAG_UNICODE16))
	{
		size_t inlen = strlen(in);
		memset(out, 0, outlen);
		if (inlen >= outlen - 2)
			return (size_t)-1;
		memcpy(&out[1], in, inlen);
		if (flags & FLAG_UNICODE8)
			out[0] = 0x08;
		else
			out[0] = 0x10;
		ret = inlen + 1;
	}
	if (ret != (size_t)-1 && ret > 1 && ret < 256)
	{
		memset(out+ret, 0, outlen-ret-1);
		out[outlen-1] = ret;
	}
	else
	{
		memset(out, 0, outlen);
		ret = (size_t)-1;
	}
	return ret;
}
