/*
 * extent.c
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

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>

#include "libudffs.h"
#include "config.h"

struct udf_extent *next_extent(struct udf_extent *start_ext, enum udf_space_type type)
{
	while (start_ext != NULL && !(start_ext->space_type & type))
		start_ext = start_ext->next;

	return start_ext;
}

uint32_t next_extent_size(struct udf_extent *start_ext, enum udf_space_type type, uint32_t blocks, uint32_t offset)
{
	uint32_t inc;
	start_ext = next_extent(start_ext, type);
cont:
	while (start_ext != NULL && start_ext->blocks < blocks)
		start_ext = next_extent(start_ext->next, type);

	if (start_ext->start % offset)
	{
		inc = offset - (start_ext->start % offset);
		if (start_ext->blocks - inc < blocks)
		{
			start_ext = next_extent(start_ext->next, type);
			goto cont;
		}
	}
	else
		inc = 0;

	return start_ext->start + inc;
}

struct udf_extent *prev_extent(struct udf_extent *start_ext, enum udf_space_type type)
{
	while (start_ext != NULL && !(start_ext->space_type & type))
		start_ext = start_ext->prev;

	return start_ext;
}

uint32_t prev_extent_size(struct udf_extent *start_ext, enum udf_space_type type, uint32_t blocks, uint32_t offset)
{
	uint32_t dec;
	start_ext = prev_extent(start_ext, type);
cont:
	while (start_ext != NULL && start_ext->blocks < blocks)
		start_ext = prev_extent(start_ext->prev, type);

	if ((start_ext->start + start_ext->blocks) % offset)
	{
		dec = (start_ext->start + start_ext->blocks) % offset;
		if (start_ext->blocks - dec < blocks)
		{
			start_ext = next_extent(start_ext->next, type);
			goto cont;
		}
	}
	else
		dec = 0;

	return start_ext->start + start_ext->blocks - dec - blocks;
}

struct udf_extent *find_extent(struct udf_disc *disc, uint32_t start)
{
	struct udf_extent *start_ext;

	start_ext = disc->head;

	while (start_ext->next != NULL)
	{
		if (start_ext->start + start_ext->blocks > start)
			break;
		start_ext = start_ext->next;
	}
	return start_ext;
}

struct udf_extent *set_extent(struct udf_disc *disc, enum udf_space_type type, uint32_t start, uint32_t blocks)
{
	struct udf_extent *start_ext, *new_ext;

	start_ext = find_extent(disc, start);
	if (start == start_ext->start)
	{
		if (blocks == start_ext->blocks)
		{
			start_ext->space_type = type;

			return start_ext;
		}
		else if (blocks < start_ext->blocks)
		{
			new_ext = malloc(sizeof(struct udf_extent));
			new_ext->space_type = type;
			new_ext->start = start;
			new_ext->blocks = blocks;
			new_ext->head = new_ext->tail = NULL;
			new_ext->prev = start_ext->prev;
			if (new_ext->prev)
				new_ext->prev->next = new_ext;
			new_ext->next = start_ext;
			if (disc->head == start_ext)
				disc->head = new_ext;

			start_ext->start += blocks;
			start_ext->blocks -= blocks;
			start_ext->prev = new_ext;

			return new_ext;
		}
		else /* blocks > start_ext->blocks */
		{
			printf("trying to change type of multiple extents\n");
			exit(1);
			start_ext->space_type = type;

			return start_ext;
		}
	}
	else /* start > start_ext->start */
	{
		if (start + blocks == start_ext->start + start_ext->blocks)
		{
			new_ext = malloc(sizeof(struct udf_extent));
			new_ext->space_type = type;
			new_ext->start = start;
			new_ext->blocks = blocks;
			new_ext->head = new_ext->tail = NULL;
			new_ext->prev = start_ext;
			new_ext->next = start_ext->next;
			if (new_ext->next)
				new_ext->next->prev = new_ext;
			if (disc->tail == start_ext)
				disc->tail = new_ext;

			start_ext->blocks -= blocks;
			start_ext->next = new_ext;

			return new_ext;
		}
		else if (start + blocks < start_ext->start + start_ext->blocks)
		{
			new_ext = malloc(sizeof(struct udf_extent));
			new_ext->space_type = type;
			new_ext->start = start;
			new_ext->blocks = blocks;
			new_ext->head = new_ext->tail = NULL;
			new_ext->prev = start_ext;

			new_ext->next = malloc(sizeof(struct udf_extent));
			new_ext->next->prev = new_ext;
			new_ext->next->space_type = start_ext->space_type;
			new_ext->next->start = start + blocks;
			new_ext->next->blocks = start_ext->blocks - blocks - start + start_ext->start;
			new_ext->next->head = new_ext->next->tail = NULL;
			new_ext->next->next = start_ext->next;
			if (new_ext->next->next)
				new_ext->next->next->prev = new_ext->next;
			if (disc->tail == start_ext)
				disc->tail = new_ext->next;

			start_ext->blocks = start - start_ext->start;
			start_ext->next = new_ext;

			return new_ext;
		}
		else /* start + blocks > start_ext->start + start_ext->blocks */
		{
			new_ext = malloc(sizeof(struct udf_extent));
			new_ext->space_type = type;
			new_ext->start = start;
			new_ext->blocks = blocks;
			new_ext->head = new_ext->tail = NULL;
			new_ext->prev = start_ext;
			new_ext->next = start_ext->next;
			if (new_ext->next)
				new_ext->next->prev = new_ext;
			if (disc->tail == start_ext)
				disc->tail = new_ext;

			start_ext->blocks -= blocks;
			start_ext->next = new_ext;

			return new_ext;
		}
	}
}

struct udf_desc *next_desc(struct udf_desc *start_desc, uint16_t ident)
{
	while (start_desc != NULL && start_desc->ident != ident)
		start_desc = start_desc->next;

	return start_desc;
}

struct udf_desc *find_desc(struct udf_extent *ext, uint32_t offset)
{
	struct udf_desc *start_desc;

	start_desc = ext->head;

	while (start_desc->next != NULL)
	{
		if (start_desc->offset == offset)
			return start_desc;
		else if (start_desc->offset > offset)
			return start_desc->prev;
		else
			start_desc = start_desc->next;
	}
	return start_desc;
}

struct udf_desc *set_desc(struct udf_disc *disc, struct udf_extent *ext, uint16_t ident, uint32_t offset, uint32_t length, struct udf_data *data)
{
	struct udf_desc *start_desc, *new_desc;

	new_desc = calloc(1, sizeof(struct udf_desc));
	new_desc->ident = ident;
	new_desc->offset = offset;
	new_desc->length = length;
	if (data == NULL)
		new_desc->data = alloc_data(NULL, length);
	else
		new_desc->data = data;

	if (ext->head == NULL)
	{
		ext->head = ext->tail = new_desc;
		new_desc->next = new_desc->prev = NULL;
	}
	else
	{
		start_desc = find_desc(ext, offset);
		if (start_desc == NULL)
		{
			new_desc->next = ext->head;
			new_desc->prev = NULL;
			new_desc->next->prev = new_desc;
			ext->head = new_desc;
		}
		else
		{
			new_desc->next = start_desc->next;
			new_desc->prev = start_desc;
			if (start_desc->next)
				start_desc->next->prev = new_desc;
			else
				ext->tail = new_desc;
			start_desc->next = new_desc;
		}
	}

	return new_desc;
}

void append_data(struct udf_desc *desc, struct udf_data *data)
{
	struct udf_data *ndata = desc->data;

	desc->length += data->length;

	while (ndata->next != NULL)
		ndata = ndata->next;

	ndata->next = data;
	data->prev = ndata;
}

struct udf_data *alloc_data(void *buffer, int length)
{
	struct udf_data *data;

	data = calloc(1, sizeof(struct udf_data));

	if (buffer)
		data->buffer = buffer;
	else if (length)
		data->buffer = calloc(1, length);
	data->length = length;

	return data;
}
