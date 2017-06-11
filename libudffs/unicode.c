/*
 * unicode.c
 *
 * Copyright (c) 2001-2002  Ben Fennema <bfennema@falcon.csc.calpoly.edu>
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

size_t decode_utf8(dstring *in, char *out, size_t inlen)
{
	size_t len = 0, i;
	unsigned int c;

	if (in[inlen-1] == 0)
		return 0;
	else if (in[0] != 8 && in[0] != 16)
		return 0;

	for (i=1; i<inlen;)
	{
		c = in[i++];
		if (in[0] == 16)
			c = (c << 8) | in[i++];

		if (c == 0x00U)
			break;
		else if (c < 0x80U)
			out[len++] = (uint8_t)c;
		else if (c < 0x800U)
		{
			out[len++] = (uint8_t)(0xc0 | (c >> 6));
			out[len++] = (uint8_t)(0x80 | (c & 0x3f));
		}
		else
		{
			out[len++] = (uint8_t)(0xe0 | (c >> 12));
			out[len++] = (uint8_t)(0x80 | ((c >> 6) & 0x3f));
			out[len++] = (uint8_t)(0x80 | (c & 0x3f));
		}
	}

	return len;
}

size_t encode_utf8(dstring *out, char *in, size_t outlen)
{
	size_t inlen = strlen(in);
	size_t len, i;
	int utf_cnt;
	uint32_t utf_char, max_val;
	unsigned int c;

	len = 1;
	out[0] = 8;
	max_val = 0xFF;

try_again:
	utf_cnt = 0;
	utf_char = 0;

	for (i=0; i<inlen; i++)
	{
		c = in[i];

		/* Complete a multi-byte UTF-8 character */
		if (utf_cnt)
		{
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
					goto error_out;
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
			if ( 0xFF == max_val )
			{
				len = 1;
				max_val = 0xFFFF;
				out[0] = 0x10;
				goto try_again;
			}
			goto error_out;
		}

		if (max_val == 0xFFFF)
		{
			if (len + 2 >= outlen)
				goto error_out;
			out[len++] = utf_char >> 8;
		}
		if (len + 1 >= outlen)
			goto error_out;
		out[len++] = utf_char & 0xFF;
	}

	if (utf_cnt)
error_out:
		return 0;

	return len;
}

size_t decode_string(struct udf_disc *disc, dstring *in, char *out, size_t inlen)
{
	if (disc->flags & FLAG_UTF8)
		return decode_utf8(in, out, inlen);
	else if (disc->flags & (FLAG_UNICODE8 | FLAG_UNICODE16))
	{
		memcpy(out, &in[1], inlen);
		return inlen;
	}
	else
		return 0;
}

size_t encode_string(struct udf_disc *disc, dstring *out, char *in, size_t outlen)
{
	memset(out, 0x00, outlen);
	if (disc->flags & FLAG_UTF8)
		return encode_utf8(out, in, outlen);
	else if (disc->flags & (FLAG_UNICODE8|FLAG_UNICODE16))
	{
		size_t inlen = strlen(in);
		if (inlen >= outlen - 2)
			return 0;
		memcpy(&out[1], in, inlen);
		if (disc->flags & FLAG_UNICODE8)
			out[0] = 0x08;
		else
			out[0] = 0x10;
		return inlen + 1;
	}
	else
		return 0;
}
