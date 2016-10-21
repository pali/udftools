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

int main(int argc, char *argv[])
{
    parse_args(argc, argv);	

    int counter;
    FILE *fp;
    long pos;
    //  struct rec my_record;
    char identifier[6];
    unsigned char type;
    unsigned char version;
    char data[2041];

    if ((fp = fopen("/home/rain/Development/udf/udf-samples/image.iso", "rb")) == NULL) {
        fprintf(stderr, "%s %i:", __FILE__, __LINE__);
        perror(NULL);
        return errno;
    } 

    fseek(fp,  0x8000, SEEK_SET);
    pos = ftell(fp);
    printf("Pos: 0x%lx\n", pos);
    fread(&type, 1,1,fp);
    fread(&identifier,5,1,fp);
    fread(&version, 1,1,fp);
    fread(&data, 2041,1,fp);
    printf("Type: 0x%x\nIdentifier: %s\nVersion: 0x%x\nData: %s\n", type, identifier, version, data+1);
    //    for ( counter=1; counter <= 10; counter++)
    //    {
    //        my_record.x= counter;
    //        fwrite(&my_record, sizeof(struct rec), 1, ptr_myfile);
    //    }
    fclose(fp);


    return 0; 

    return 0;
}
