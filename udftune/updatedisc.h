/*
 * Copyright (C) 2017  Pali Rohár <pali.rohar@gmail.com>
 * Copyright (C) 2023  Johannes Truschnigg <johannes@truschnigg.info>
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

#ifndef UPDATEDISC_H
#define UPDATEDISC_H

uint16_t compute_crc(void *, size_t);
uint8_t compute_checksum(tag *);
int check_desc(void *, size_t);
void update_desc(void *, size_t);
void write_desc(int, struct udf_disc *, enum udf_space_type, uint16_t, void *);
int open_existing_disc(struct udf_disc *, char *, int, int, char *);
int verify_lvd(struct udf_disc *, char *);
int verify_fsd(struct udf_disc *, char *);
int check_wr_lvd(struct udf_disc *, char *, int);
int check_wr_fsd(struct udf_disc *, char *, int);
int sync_dev_and_close(struct udf_disc *, int, char *, char *);
int check_access_type(struct udf_disc *, struct partitionDesc *, char *, int, int);

#endif /* UPDATEDISC_H */
