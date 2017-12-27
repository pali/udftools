/*
 * utils.c
 *
 * Copyright (c) 2017    Vojtech Vladyka <vojtch.vladyka@gmail.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 
 * USA.
 *
 */
#define _GNU_SOURCE

#include "config.h"
#include "utils.h"
#include "options.h"

#include <stdio.h>
#include <stdarg.h>

#include <dlfcn.h>

/**
 * \brief Support function for printing basic tag information
 *
 * \param[in] id tag identifier
 */
void read_tag(tag id) {
    note("\tIdentification Tag\n"
           "\t==================\n");
    note("\tID: %d (", id.tagIdent);
    switch(id.tagIdent) {
        case TAG_IDENT_PVD:
            note("PVD");
            break;
        case TAG_IDENT_AVDP:
            note("AVDP");
            break;
        case TAG_IDENT_VDP:
            note("VDP");
            break;
        case TAG_IDENT_IUVD:
            note("IUVD");
            break;
        case TAG_IDENT_PD:
            note("PD");
            break;
        case TAG_IDENT_LVD:
            note("LVD");
            break;
        case TAG_IDENT_USD:
            note("USD");
            break;
        case TAG_IDENT_TD:
            note("TD");
            break;    
        case TAG_IDENT_LVID:
            note("LVID");
            break;
    }
    note(")\n");
    note("\tVersion: %d\n", id.descVersion);
    note("\tChecksum: 0x%x\n", id.tagChecksum);
    note("\tSerial Number: 0x%x\n", id.tagSerialNum);
    note("\tDescriptor CRC: 0x%x, Length: %d\n", id.descCRC, id.descCRCLength);
    note("\tTag Location: 0x%x\n", id.tagLocation);
}

/**
 * \brief Support function printing VDS, AVDP and LVID
 *
 * \param[in] *disc UDF disc
 */
int print_disc(struct udf_disc *disc) {
    note("\nUDF Metadata Overview\n"
          "---------------------\n");

    note("AVDP\n"
        "----\n");
    for(int i=0; i<3; i++) {
        note("[%d]\n", i);
        if(disc->udf_anchor[i] != 0) {
            read_tag(disc->udf_anchor[i]->descTag);
        }
    }
    
    note("PVD\n"
        "---\n");
    for(int i=0; i<2; i++) {
        note("[%d]\n", i);
        if(disc->udf_pvd[i] != 0) {
            read_tag(disc->udf_pvd[i]->descTag);
        }
    }

    note("LVD\n"
        "---\n");
    for(int i=0; i<2; i++) {
        note("[%d]\n", i);
        if(disc->udf_lvd[i] != 0) {
            read_tag(disc->udf_lvd[i]->descTag);
            note("\tPartition Maps: %d\n",disc->udf_lvd[i]->partitionMaps[0]);
        }
    }

    note("PD\n"
        "--\n");
    for(int i=0; i<2; i++) {
        note("[%d]\n", i);
        if(disc->udf_pd[i] != 0) {
            read_tag(disc->udf_pd[i]->descTag);
        }
    }

    note("USD\n"
        "---\n");
    for(int i=0; i<2; i++) {
        note("[%d]\n", i);
        if(disc->udf_usd[i] != 0) {
            read_tag(disc->udf_usd[i]->descTag);
            note("\tNumOfAllocDescs: %d\n", disc->udf_usd[i]->numAllocDescs);
        }
    }

    note("IUVD\n"
        "----\n");
    for(int i=0; i<2; i++) {
        note("[%d]\n", i);
        if(disc->udf_iuvd[i] != 0) {
            read_tag(disc->udf_iuvd[i]->descTag);
        }
    }

    note("TD\n"
        "--\n");
    for(int i=0; i<2; i++) {
        note("[%d]\n", i);
        if(disc->udf_td[i] != 0) {
            read_tag(disc->udf_td[i]->descTag);
        }
    }

    return 0;
}

/**
 * \brief Prints metadata error sequence
 *
 * \param[in] *seq VDS sequence 
 */
void print_metadata_sequence(vds_sequence_t *seq) {
    note("Main             Reserve\n");
    note("ident | Errors | ident | Errors \n");     
    for(int i=0; i<VDS_STRUCT_AMOUNT; ++i) {
        note("%5d |   0x%02x | %5d |   0x%02x \n", seq->main[i].tagIdent, seq->main[i].error, seq->reserve[i].tagIdent, seq->reserve[i].error);
    }
}

#if MEMTRACE
uint64_t alloc_size = 0;
uint64_t map_size = 0;

void *custom_malloc(size_t size, char * file, int line) {
    void *(*libc_malloc)(size_t) = dlsym(RTLD_NEXT, "malloc");
    void * ptr = libc_malloc(size);
    dbg("[MEMTRACE] malloc %s:%d (%d) -> %p\n", file, line, size, ptr);
    return ptr;
}

void custom_free(void *ptr, char * file, int line) {
    void (*libc_free)(void*) = dlsym(RTLD_NEXT, "free");
    dbg("[MEMTRACE] free %s:%d %p\n", file, line, ptr);
    libc_free(ptr); 
}


void *custom_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset, char * file, int line) {
    void *(*libc_mmap)(void *, size_t, int, int, int, off_t) = dlsym(RTLD_NEXT, "mmap");
    void * ptr = libc_mmap(addr, length, prot, flags, fd, offset);
    map_size += length;
    dbg("[MEMTRACE] mmap %s:%d (%d) -> %p, Total: %ld\n", file, line, length, ptr, map_size);
    return ptr;
}

int custom_munmap(void *addr, size_t length, char * file, int line) {
    int (*libc_munmap)(void*, size_t) = dlsym(RTLD_NEXT, "munmap");
    map_size -= length;
    dbg("[MEMTRACE] munmap %s:%d %p -> (%d), Total: %ld\n", file, line, addr, length, map_size);
    return libc_munmap(addr, length); 
}
#endif

