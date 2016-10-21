/*
 * main.c
 *
 * Copyright (c) 2016    Vojtech Vladyka <vojtch.vladyka@gmail.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <ecma_167.h>

#include "udf.h"
#include "options.h"

#define PVD 0x10
#define AVDP 0x100

#define BLOCK_SIZE 2048

int is_udf(FILE *fp) {
    struct volStructDesc vsd;
    struct beginningExtendedAreaDesc bea;
    struct volStructDesc nsr;
    struct terminatingExtendedAreaDesc tea;
    
    fseek(fp,  PVD*BLOCK_SIZE, SEEK_SET); // default block size is 2048B, so PVD will be there
    
    
    for(int i = 0; i<6; i++) {
        printf("[DBG] try #%d\n", i);
        
        printf("[DBG] address: 0x%x\n", (unsigned int)ftell(fp));
        fread(&vsd, sizeof(vsd), 1, fp); // read its contents to vsd structure
        
        printf("[DBG] vsd: type:%d, id:%s, v:%d\n", vsd.structType, vsd.stdIdent, vsd.structVersion);
        
        
        if(!strncmp((char *)vsd.stdIdent, VSD_STD_ID_BEA01, 5)) {
            //It's Extended area descriptor, so it might be UDF, check next sector
            memcpy(&bea, &vsd, sizeof(bea)); // store it for later 
        } else if(!strncmp((char *)vsd.stdIdent, VSD_STD_ID_BOOT2, 5)) {
            fprintf(stderr, "BOOT2 found, unsuported for now.\n");
            return(-1);
        } else if(!strncmp((char *)vsd.stdIdent, VSD_STD_ID_CD001, 5)) { 
            //CD001 means there is ISO9660, we try search for UDF at sector 18
            //TODO do check for other parameters here
            fseek(fp, BLOCK_SIZE, SEEK_CUR);
        } else if(!strncmp((char *)vsd.stdIdent, VSD_STD_ID_CDW02, 5)) {
            fprintf(stderr, "CDW02 found, unsuported for now.\n");
            return(-1);
        } else if(!strncmp((char *)vsd.stdIdent, VSD_STD_ID_NSR01, 5)) {
            memcpy(&nsr, &vsd, sizeof(nsr));
        } else if(!strncmp((char *)vsd.stdIdent, VSD_STD_ID_NSR02, 5)) {
            memcpy(&nsr, &vsd, sizeof(nsr));
        } else if(!strncmp((char *)vsd.stdIdent, VSD_STD_ID_NSR03, 5)) {
            memcpy(&nsr, &vsd, sizeof(nsr));
        } else if(!strncmp((char *)vsd.stdIdent, VSD_STD_ID_TEA01, 5)) {
            //We found TEA01, so we can end recognition sequence
            memcpy(&tea, &vsd, sizeof(tea));
            break;
        } else {
            fprintf(stderr, "Unknown identifier. Exiting\n");
            return(-1);
        }  
    }
    

    printf("bea: type:%d, id:%s, v:%d\n", bea.structType, bea.stdIdent, bea.structVersion);
    printf("nsr: type:%d, id:%s, v:%d\n", nsr.structType, nsr.stdIdent, nsr.structVersion);
    printf("tea: type:%d, id:%s, v:%d\n", tea.structType, tea.stdIdent, tea.structVersion);

    return 0;
}

int main(int argc, char *argv[]) {
    char *path = NULL;
    parse_args(argc, argv, &path);	
    printf("path ptr: %p\n", path);

    FILE *fp;
    int status = 0;
    
    //uint32_t lsn = 0;
    if(strlen(path) == 0 || path == NULL) {
        printf("No file given. Exiting.\n");
        exit(-1);
    }
    printf("File to analyze: %s\n", path);

    if ((fp = fopen(path, "rb")) == NULL) {
        fprintf(stderr, "%s %i:", __FILE__, __LINE__);
        perror(NULL);
        return errno;
    } 

    
    status = is_udf(fp);

    fclose(fp);
    return status;
}
