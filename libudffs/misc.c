/*
 * Copyright (C) 2017-2018  Pali Rohár <pali.rohar@gmail.com>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>

#include "libudffs.h"

const char *appname;

size_t gen_uuid_from_vol_set_ident(char uuid[17], const dstring *vol_set_ident, size_t size)
{
	size_t i;
	size_t len;
	size_t nonhexpos;
	unsigned char buf[127*4+1];

	memset(buf, 0, sizeof(buf));

	if (size > 0 && vol_set_ident[size-1] > 0 && vol_set_ident[size-1] < size)
		len = decode_utf8((dchars *)vol_set_ident, (char *)buf, vol_set_ident[size-1], sizeof(buf));
	else
		len = 0;

	if (len < 8)
	{
		uuid[0] = 0;
		return (size_t)-1;
	}

	nonhexpos = 16;
	for (i = 0; i < 16; ++i)
	{
		if (!isxdigit(buf[i]))
		{
			nonhexpos = i;
			break;
		}
	}

	if (nonhexpos < 8)
	{
		snprintf(uuid, 17, "%02x%02x%02x%02x%02x%02x%02x%02x",
			buf[0], buf[1], buf[2], buf[3],
			buf[4], buf[5], buf[6], buf[7]);
	}
	else if (nonhexpos < 16)
	{
		for (i = 0; i < 8; ++i)
			uuid[i] = tolower(buf[i]);
		snprintf(uuid + 8, 9, "%02x%02x%02x%02x",
			buf[8], buf[9], buf[10], buf[11]);
	}
	else
	{
		for (i = 0; i < 16; ++i)
			uuid[i] = tolower(buf[i]);
		uuid[16] = 0;
	}

	if (nonhexpos < 16)
		return nonhexpos;

	return 16;
}

uint32_t strtou32(const char *str, int base, int *failed)
{
	char *endptr = NULL;
	long long int ret;

	/* strtou* does not signal underflow, so use signed variant */
	errno = 0;
	ret = strtoll(str, &endptr, base);
	/* strto* skips leading whitespaces, so detect them via isspace */
	*failed = (!*str || isspace(*str) || *endptr || errno || ret < 0 || ret > UINT32_MAX) ? 1 : 0;
	return ret;
}

uint16_t strtou16(const char *str, int base, int *failed)
{
	uint32_t ret;

	ret = strtou32(str, base, failed);
	*failed = (*failed || ret > UINT16_MAX) ? 1 : 0;
	return ret;
}

static inline unsigned int intlog2(unsigned int word)
{
	unsigned int result = 0;

	while (word >>= 1)
		result++;
	return result;
}

uint32_t randu32(void)
{
	int fd;
	uint32_t value;
	unsigned int bits;
	unsigned int rand_bits;

	static int srand_done = 0;

	fd = open("/dev/urandom", O_RDONLY);
	if (fd >= 0)
	{
		if (read(fd, &value, sizeof(value)) == sizeof(value))
		{
			close(fd);
			return value;
		}
		close(fd);
	}

	if (!srand_done)
	{
		srand(time(NULL) * getpid());
		srand_done = 1;
	}

	value = 0;
	rand_bits = intlog2((unsigned int)RAND_MAX+1);
	for (bits = 0; bits < 32; bits += rand_bits)
	{
		value <<= rand_bits;
		value |= rand();
	}

	return value;
}
